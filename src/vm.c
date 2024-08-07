#include "vm.h"

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "string.h"
#include "table.h"
#include "time.h"
#include "value.h"

VM vm;

// TODO: more native functions
// TODO: arrays and tables 

// Native functions:

static Value clockNative(int argCount, Value *args) {
  return NUM_VAL((double)clock() / CLOCKS_PER_SEC);
}

static Value printNative(int argCount, Value *args) {
  for (int i = 0; i < argCount; i++) {
    printValue(args[i]);
  }
  printf("\n");
  return NIL_VAL;
}

static Value lenNative(int argCount, Value* args) {
  if (argCount == 1) {
    // add arrays and tables when they will exist 
    if (IS_STRING(args[0])) {
      ObjString* s = AS_STRING(args[0]);
      return NUM_VAL(s->length); 
    }
  }
  return NIL_VAL;
}

static Value exitNative(int argCount, Value* args) {
  if (argCount == 1) {
    double code = AS_NUM(args[0]);
    exit(code);
  }
  exit(0);
  return NIL_VAL;
}

static void resetStack() {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
  vm.openUpvalues = NULL;
}

void push(Value value) {
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}

static Value peek(int distance) { return vm.stackTop[-1 - distance]; }

static void runtimeError(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);
  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame *frame = &vm.frames[i];
    ObjFunc *function = frame->closure->function;
    // -1 because the IP is sitting on the next instruction to be executed.
    size_t instruction = frame->ip - function->chunk.code - 1;
    fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }
  resetStack();
}

static void defineNative(const char *name, NativeFn function) {
  // pushing and popping to ensure that the function is not collected by the GC
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function)));
  tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
  pop();
  pop();
}

void initVM() {
  resetStack();
  vm.objects = NULL;
  initTable(&vm.strings);
  initTable(&vm.globals);

  vm.initString = NULL;  // just to be safe from GC
  vm.initString = copyString(INIT_STRING, INIT_STRING_LEN);

  vm.grayCount = 0;
  vm.grayCapacity = 0;
  vm.grayStack = NULL;

  vm.bytesAllocated = 0;
  vm.nextGC = GC_BEFORE_FIRST;

  // -----------------------------------
  defineNative("clock", clockNative);
  defineNative("print", printNative);
  defineNative("len", lenNative);
  defineNative("exit", exitNative);
}

void freeVM() {
  freeObjects();  // free all objects
  freeTable(&vm.strings);
  freeTable(&vm.globals);
  vm.initString = NULL;
}

static bool call(ObjClosure *closure, int argCount) {
  if (argCount != closure->function->arity) {
    runtimeError("Expected %d arguments but got %d", closure->function->arity,
                 argCount);
    return false;
  }

  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow");
    return false;
  }

  CallFrame *frame = &vm.frames[vm.frameCount++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  return true;
}

static bool callValue(Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_CLOSURE:
        return call(AS_CLOSURE(callee), argCount);
      case OBJ_NATIVE: {
        NativeFn native = AS_NATIVE(callee);
        Value result = native(argCount, vm.stackTop - argCount);
        vm.stackTop -= argCount + 1;
        push(result);
        return true;
      }
      case OBJ_CLASS: {  // creating a class
        ObjClass *cclass = AS_CLASS(callee);
        vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(cclass));

        Value initializer;
        if (tableGet(&cclass->methods, vm.initString, &initializer)) {
          return call(AS_CLOSURE(initializer), argCount);
        } else if (argCount != 0) {
          runtimeError("Expected 0 arguments but got %d", argCount);
        }

        return true;
      }
      case OBJ_BOUND_METHOD: {
        ObjBoundMethod *bound = AS_BOUND_METHOD(callee);
        vm.stackTop[-argCount - 1] = bound->receiver;
        return call(bound->method, argCount);
      }
      default:
        break;
    }
  }

  runtimeError("Can only call functions and classes");
  return false;
}

static bool invokeFromClass(ObjClass *cclass, ObjString *name, int argCount) {
  Value method;
  if (!tableGet(&cclass->methods, name, &method)) {
    runtimeError("Undefined property '%s'", name->chars);
    return false;
  }

  return call(AS_CLOSURE(method), argCount);
}

static bool invoke(ObjString *name, int argCount) {
  Value receiver = peek(argCount);

  if (!IS_INSTANCE(receiver)) {
    runtimeError("Only instances have methods");
    return false;
  }

  ObjInstance *instance = AS_INSTANCE(receiver);

  Value val;
  if (tableGet(&instance->fields, name, &val)) {
    vm.stackTop[-argCount - 1] = val;
    return callValue(val, argCount);
  }

  return invokeFromClass(instance->cclass, name, argCount);
}

static bool bindMethod(ObjClass *cclass, ObjString *name) {
  Value method;
  if (!tableGet(&cclass->methods, name, &method)) {
    runtimeError("Undefined property '%s'", name->chars);
    return false;
  }

  ObjBoundMethod *bound = newBoundMethod(peek(0), AS_CLOSURE(method));

  pop();
  push(OBJ_VAL(bound));
  return true;
}

static ObjUpvalue *captureUpvalue(Value *local) {
  ObjUpvalue *prevUpvalue = NULL;
  ObjUpvalue *upvalue = vm.openUpvalues;

  // all this can be a little bit slow, but should be ok
  while (upvalue != NULL && upvalue->location > local) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }

  ObjUpvalue *createdUpvalue = newUpvalue(local);

  if (prevUpvalue == NULL) {
    vm.openUpvalues = createdUpvalue;
  } else {
    prevUpvalue->next = createdUpvalue;
  }

  return createdUpvalue;
}

static void closeUpvalues(Value *last) {
  while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
    ObjUpvalue *upvalue = vm.openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.openUpvalues = upvalue->next;
  }
}

static void defineMethod(ObjString *name) {
  Value method = peek(0);
  ObjClass *cclass = AS_CLASS(peek(1));
  tableSet(&cclass->methods, name, method);
  pop();
}

static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
  ObjString *b = AS_STRING(peek(0));
  ObjString *a = AS_STRING(peek(1));
  int length = a->length + b->length;

  char *chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  pop();
  pop();

  ObjString *result = takeString(chars, length);
  push(OBJ_VAL(result));
}

static void undefinedVarError(ObjString *name) {
  runtimeError("Undefined variable '%s'.", name->chars);
}

static InterpretResult run() {
  CallFrame *frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
  (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT_LONG()                 \
  (frame->closure->function->chunk.constants \
       .values[(READ_BYTE() << 8) | READ_BYTE()])
#define READ_STRING_LONG() AS_STRING(READ_CONSTANT_LONG())
#define BINARY_OP(valType, op)                        \
  do {                                                \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
      runtimeError("Operands must be numbers");       \
      return INTERPRET_RUNTIME_ERROR;                 \
    }                                                 \
    double b = AS_NUM(pop());                         \
    double a = AS_NUM(pop());                         \
    push(valType(a op b));                            \
  } while (false)

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION  // enable debug trace if macro is defined
    printf("\nrunning... \n");
    printf("          ");
    for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
    disassembleInstruction(
        &frame->closure->function->chunk,
        (int)(frame->ip - frame->closure->function->chunk.code));
#endif
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
      case OP_CONSTANT:
        Value constant = READ_CONSTANT_LONG();
        push(constant);
        break;
      case OP_NIL:
        push(NIL_VAL);
        break;
      case OP_TRUE:
        push(BOOL_VAL(true));
        break;
      case OP_FALSE:
        push(BOOL_VAL(false));
        break;
      case OP_RETURN: {
        Value result = pop();
        closeUpvalues(frame->slots);
        vm.frameCount--;
        if (vm.frameCount == 0) {
          pop();
          return INTERPRET_OK;
        }
        vm.stackTop = frame->slots;
        push(result);
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
      case OP_ADD: {
        if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
          concatenate();
        } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
          double b = AS_NUM(pop());
          double a = AS_NUM(pop());
          push(NUM_VAL(a + b));
        } else {
          runtimeError("Operands must be two numbers or two strings.");
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_SUBTRACT:
        BINARY_OP(NUM_VAL, -);
        break;
      case OP_MULTIPLY:
        BINARY_OP(NUM_VAL, *);
        break;
      case OP_DIVIDE:
        BINARY_OP(NUM_VAL, /);
        break;
      case OP_POWER: {
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
          runtimeError("Operands must be numbers");
          return INTERPRET_RUNTIME_ERROR;
        }
        double b = AS_NUM(pop());
        double a = AS_NUM(pop());
        push(NUM_VAL(pow(a, b)));
        break;
      }
      case OP_EQUAL:
        Value a = pop();
        Value b = pop();
        push(BOOL_VAL(valuesEqual(a, b)));
        break;
      case OP_GREATER:
        BINARY_OP(BOOL_VAL, >);
        break;
      case OP_LESS:
        BINARY_OP(BOOL_VAL, <);
        break;
      case OP_NOT:
        push(BOOL_VAL(isFalsey(pop())));
        break;
      case OP_NEGATE: {
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number");
          return INTERPRET_RUNTIME_ERROR;
        }

        push(NUM_VAL(-AS_NUM(pop())));

        break;
      }
      case OP_POP:
        pop();
        break;
      case OP_DEFINE_GLOBAL: {
        ObjString *name = READ_STRING_LONG();
        tableSet(&vm.globals, name, peek(0));
        pop();
        break;
      }
      case OP_GET_GLOBAL: {
        ObjString *name = READ_STRING_LONG();
        Value val;
        if (!tableGet(&vm.globals, name, &val)) {
          undefinedVarError(name);
          return INTERPRET_RUNTIME_ERROR;
        }

        push(val);
        break;
      }
      case OP_SET_GLOBAL: {
        ObjString *name = READ_STRING_LONG();
        if (tableSet(&vm.globals, name, peek(0))) {
          tableDelete(&vm.globals, name);
          undefinedVarError(name);
          return INTERPRET_RUNTIME_ERROR;
        }

        break;
      }
      case OP_GET_LOCAL: {
        uint16_t slot = READ_SHORT();
        push(frame->slots[slot]);
        break;
      }
      case OP_SET_LOCAL: {
        uint16_t slot = READ_SHORT();
        frame->slots[slot] = peek(0);
        break;
      }
      case OP_JUMP_FALSE: {
        uint16_t offset = READ_SHORT();
        if (isFalsey(peek(0))) frame->ip += offset;
        break;
      }
      case OP_JUMP: {
        uint16_t offset = READ_SHORT();
        frame->ip += offset;
        break;
      }
      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        frame->ip -= offset;
        break;
      }
      case OP_CALL: {
        int argCount = READ_BYTE();
        if (!callValue(peek(argCount), argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
      case OP_CLOSURE: {
        ObjFunc *function = AS_FUNCTION(READ_CONSTANT_LONG());
        ObjClosure *closure = newClosure(function);
        push(OBJ_VAL(closure));
        for (int i = 0; i < closure->upvalueCount; i++) {
          uint8_t isLocal = READ_BYTE();
          uint16_t index = READ_SHORT();
          if (isLocal) {
            closure->upvalues[i] = captureUpvalue(frame->slots + index);
          } else {
            closure->upvalues[i] = frame->closure->upvalues[index];
          }
        }
        break;
      }
      case OP_GET_UPVALUE: {
        uint16_t slot = READ_SHORT();
        push(*frame->closure->upvalues[slot]->location);
        break;
      }
      case OP_SET_UPVALUE: {
        uint16_t slot = READ_SHORT();
        *frame->closure->upvalues[slot]->location = peek(0);
        break;
      }
      case OP_CLOSE_UPVALUE: {
        closeUpvalues(vm.stackTop - 1);
        pop();
        break;
      }
      case OP_CLASS:
        push(OBJ_VAL(newClass(READ_STRING_LONG())));
        break;
      case OP_GET_PROPERTY: {
        if (!IS_INSTANCE(peek(0))) {
          runtimeError("Only instances can have properties");
          return INTERPRET_RUNTIME_ERROR;
        }

        ObjInstance *instance = AS_INSTANCE(peek(0));
        ObjString *name = READ_STRING_LONG();
        Value val;

        if (tableGet(&instance->fields, name, &val)) {
          pop();  // pop instance
          push(val);
          break;
        }

        if (!bindMethod(instance->cclass, name)) {
          return INTERPRET_RUNTIME_ERROR;
        }

        break;
      }
      case OP_SET_PROPERTY: {
        if (!IS_INSTANCE(peek(1))) {
          runtimeError("Only instances can have fields");
          return INTERPRET_RUNTIME_ERROR;
        }

        ObjInstance *instance = AS_INSTANCE(peek(1));
        tableSet(&instance->fields, READ_STRING_LONG(), peek(0));

        Value val = pop();
        pop();
        push(val);
        break;
      }
      case OP_METHOD:
        defineMethod(READ_STRING_LONG());
        break;
      case OP_INVOKE: {
        ObjString *method = READ_STRING_LONG();
        int argCount = READ_BYTE();
        if (!invoke(method, argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
      case OP_INHERIT: {
        Value superclass = peek(1);
        if (!IS_CLASS(superclass)) {
          runtimeError("Superclass must be a class");
          return INTERPRET_RUNTIME_ERROR;
        }
        ObjClass *subclass = AS_CLASS(peek(0));
        tableAddAll(&AS_CLASS(superclass)->methods, &subclass->methods);
        pop();  // subclass
        break;
      }
      case OP_GET_SUPER: {
        ObjString *name = READ_STRING_LONG();
        ObjClass *superclass = AS_CLASS(pop());
        if (!bindMethod(superclass, name)) {
          return INTERPRET_RUNTIME_ERROR;
        }

        break;
      }
      case OP_SUPER_INVOKE: {
        ObjString *method = READ_STRING_LONG();
        int argCount = READ_BYTE();
        ObjClass *superclass = AS_CLASS(pop());
        if (!invokeFromClass(superclass, method, argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }

        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
    }
  }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT_LONG
#undef READ_STRING_LONG
#undef BINARY_OP
}

InterpretResult interpret(const char *source) {
  ObjFunc *func = compile(source);
  if (func == NULL) return INTERPRET_COMPILE_ERROR;

  push(OBJ_VAL(func));
  ObjClosure *closure = newClosure(func);
  pop();
  push(OBJ_VAL(closure));
  callValue(OBJ_VAL(closure), 0);

  return run();
}
