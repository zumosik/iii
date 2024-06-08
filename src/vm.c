#include "common.h"
#include "vm.h"
#include "debug.h"
#include <stdarg.h>
#include <stdio.h>
#include "string.h"
#include "object.h"
#include "memory.h"

VM vm;

static void resetStack()
{
    vm.stackTop = vm.stack;
}

void initVM()
{
    resetStack();
    vm.objects = NULL;
    initTable(&vm.strings);
    initTable(&vm.globals);
}

void freeVM()
{
    freeObjects(); // free all objects
    freeTable(&vm.strings);
    freeTable(&vm.globals);
}

void push(Value value)
{
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop()
{
    vm.stackTop--;
    return *vm.stackTop;
}

static Value peek(int distance)
{
    return vm.stackTop[-1 - distance];
}

static bool isFalsey(Value value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void runtimeError(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);
    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    resetStack();
}

static void concatenate()
{
    ObjString *b = AS_STRING(pop());
    ObjString *a = AS_STRING(pop());

    int length = a->length + b->length;
    char *chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString *result = takeString(chars, length);
    push(OBJ_VAL(result));
}

static void undefinedVarError(ObjString *name)
{
    runtimeError("Undefined variable '%s'.", name->chars);
}

static InterpretResult run()
{
#define READ_BYTE() (*vm.ip++)
#define READ_SHORT() \
    (vm.ip += 2, (uint16_t)((vm.ip[-2] << 8) | vm.ip[-1]))
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_CONSTANT_LONG() (vm.chunk->constants.values[(READ_BYTE() << 8) | READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define READ_STRING_LONG() AS_STRING(READ_CONSTANT_LONG())
#define BINARY_OP(valType, op)                          \
    do                                                  \
    {                                                   \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) \
        {                                               \
            runtimeError("Operands must be numbers");   \
            return INTERPRET_RUNTIME_ERROR;             \
        }                                               \
        double b = AS_NUM(pop());                       \
        double a = AS_NUM(pop());                       \
        push(valType(a op b));                          \
    } while (false)

    printf("\n running... \n");

    for (;;)
    {
#ifdef DEBUG_TRACE_EXECUTION // enable debug trace if macro is defined
        printf("          ");
        for (Value *slot = vm.stack; slot < vm.stackTop; slot++)
        {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif
        uint8_t instruction;
        switch (instruction = READ_BYTE())
        {
        case OP_CONSTANT_LONG:
            Value constant = READ_CONSTANT_LONG();
            push(constant);
            break;
        case OP_CONSTANT:
        {
            Value constant = READ_CONSTANT();
            push(constant);
            break;
        }
        case OP_NIL:
            push(NIL_VAL);
            break;
        case OP_TRUE:
            push(BOOL_VAL(true));
            break;
        case OP_FALSE:
            push(BOOL_VAL(false));
            break;
        case OP_RETURN:
        {
            return INTERPRET_OK;
        }
        case OP_ADD:
        {
            if (IS_STRING(peek(0)) && IS_STRING(peek(1)))
            {
                concatenate();
            }
            else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1)))
            {
                double b = AS_NUM(pop());
                double a = AS_NUM(pop());
                push(NUM_VAL(a + b));
            }
            else
            {
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
        case OP_NEGATE:
        {
            if (!IS_NUMBER(peek(0)))
            {
                runtimeError("Operand must be a number");
                return INTERPRET_RUNTIME_ERROR;
            }

            push(NUM_VAL(-AS_NUM(pop())));

            break;
        }
        case OP_PRINT:
        {
            printf("\n==================\n");
            printValue(pop());
            printf("\n==================\n");
            break;
        }
        case OP_POP:
            pop();
            break;
        case OP_DEFINE_GLOBAL:
        {
            ObjString *name = READ_STRING();
            tableSet(&vm.globals, name, peek(0));
            pop();
            break;
        }
        case OP_GET_GLOBAL:
        {
            ObjString *name = READ_STRING();
            Value val;
            if (!tableGet(&vm.globals, name, &val))
            {
                undefinedVarError(name);
                return INTERPRET_RUNTIME_ERROR;
            }

            push(val);
            break;
        }
        case OP_SET_GLOBAL:
        {
            ObjString *name = READ_STRING();
            if (tableSet(&vm.globals, name, peek(0)))
            {
                tableDelete(&vm.globals, name);
                undefinedVarError(name);
                return INTERPRET_RUNTIME_ERROR;
            }

            break;
        }
        case OP_DEFINE_GLOBAL_LONG:
        {
            ObjString *name = READ_STRING_LONG();
            tableSet(&vm.globals, name, peek(0));
            pop();
            break;
        }
        case OP_GET_GLOBAL_LONG:
        {
            ObjString *name = READ_STRING_LONG();
            Value val;
            if (!tableGet(&vm.globals, name, &val))
            {
                undefinedVarError(name);
                return INTERPRET_RUNTIME_ERROR;
            }

            push(val);
            break;
        }
        case OP_SET_GLOBAL_LONG:
        {
            ObjString *name = READ_STRING_LONG();
            if (tableSet(&vm.globals, name, peek(0)))
            {
                tableDelete(&vm.globals, name);
                undefinedVarError(name);
                return INTERPRET_RUNTIME_ERROR;
            }

            break;
        }
        case OP_GET_LOCAL:
        {
            uint8_t slot = READ_BYTE();
            push(vm.stack[slot]);
            break;
        }
        case OP_SET_LOCAL:
        {
            uint8_t slot = READ_BYTE();
            vm.stack[slot] = peek(0);
            break;
        }
        case OP_GET_LOCAL_LONG:
        {
            uint16_t slot = READ_SHORT();
            push(vm.stack[slot]);
            break;
        }
        case OP_SET_LOCAL_LONG:
        {
            uint16_t slot = READ_BYTE();
            vm.stack[slot] = peek(0);
            break;
        }
        case OP_JUMP_FALSE:
        {
            uint16_t offset = READ_SHORT();
            if (isFalsey(peek(0)))
                vm.ip += offset;
            break;
        }
        case OP_JUMP:
        {
            uint16_t offset = READ_SHORT();
            vm.ip += offset;
            break;
        }
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_CONSTANT_LONG
#undef READ_STRING
#undef READ_STRING_LONG
#undef BINARY_OP
}

InterpretResult interpret(const char *source)
{
    Chunk chunk;
    initChunk(&chunk);

    if (!compile(source, &chunk))
    {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    printf("Compile end \n");

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = run();

    freeChunk(&chunk);
    return result;
}