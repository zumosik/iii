#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "chunk.h"
#include "object.h"
#include "compiler_arrays.h"

typedef struct
{
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

Parser parser;

typedef enum
{
    TYPE_FUNCTION,
    TYPE_SCRIPT
} FunctionType;

typedef struct Compiler
{
    struct Compiler *enclosing;

    ObjFunc *function;
    FunctionType type;

    LocalsArray locals;
    UpvaluesArray upvalues;

    int scopeDepth;
} Compiler;

Compiler *current = NULL;

static Chunk *currentChunk()
{
    return &current->function->chunk;
}

static void initCompiler(Compiler *compiler, FunctionType type)
{
    compiler->enclosing = current;

    compiler->function = NULL;
    compiler->type = type;

    initLocalsArray(&compiler->locals);
    initUpvaluesArray(&compiler->upvalues);

    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    current = compiler;

    if (type != TYPE_SCRIPT)
    {
        current->function->name = copyString(parser.previous.start,
                                             parser.previous.length);
    }

    Local local;
    local.depth = 0;
    local.name.start = "";
    local.name.length = 0;

    writeLocalsArray(&compiler->locals, local);
}

static void errorAt(Token *token, const char *message)
{
    if (parser.panicMode)
        return;

    parser.panicMode = true;

    fprintf(stderr, "[line %d] Error", token->line);
    if (token->type == TOKEN_EOF)
    {
        fprintf(stderr, " at end");
    }
    else if (token->type == TOKEN_ERROR)
    {
        // Nothing.
    }
    else
    {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }
    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void error(const char *message)
{
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char *message)
{
    errorAt(&parser.current, message);
}

static void advance()
{
    parser.previous = parser.current;

    for (;;)
    {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR)
            break;

        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char *message)
{
    if (parser.current.type == type)
    {
        advance();
        return;
    }

    errorAtCurrent(message);
}

static void emitByte(uint8_t byte)
{
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2)
{
    emitByte(byte1);
    emitByte(byte2);
}

static void emitReturn()
{
    emitBytes(OP_NIL, OP_RETURN);
}

static int emitJump(uint8_t instruction)
{
    emitByte(instruction);
    emitBytes(0xff, 0xff);

    return currentChunk()->count - 2;
}

static void emitLoop(int loopStart)
{
    emitByte(OP_LOOP);
    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX)
        error("Loop body is too large");
    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

// ! endCompiler would not free upvalues array
static ObjFunc *endCompiler()
{
    emitReturn();

    current->function->upvalueCount = current->upvalues.count;
    ObjFunc *func = current->function;

#ifdef DEBUG_PRINT_CODE

#include "debug.h"

    if (!parser.hadError)
    {
        disassembleChunk(currentChunk(), func->name != NULL
                                             ? func->name->chars
                                             : "<script>");
    }
#endif

    // freeLocalsArray(&current->locals);
    // FIXME

    current = current->enclosing;

    return func;
}

static void beginScope()
{
    current->scopeDepth++;
}

static void endScope()
{
    current->scopeDepth--;

    int count = current->locals.count;

    while (count > 0 &&
           current->locals.values[count - 1].depth >
               current->scopeDepth)
    {
        emitByte(OP_POP);
        count--;
    }
}

static bool check(TokenType type)
{
    return parser.current.type == type;
}

static bool match(TokenType type)
{
    if (!check(type))
        return false;
    advance();
    return true;
}

static void parsePrecedence(Precedence precedence)
{
    advance();
    ParseFn prefixFn = getRule(parser.previous.type)->prefix;
    if (prefixFn == NULL)
    {
        error("Expect expression");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixFn(canAssign);

    while (precedence <= getRule(parser.current.type)->precedence)
    {
        advance();
        ParseFn infixFn = getRule(parser.previous.type)->infix;
        infixFn(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL))
    {
        error("Invalid assignment target.");
    }
}

static uint16_t makeConstant(Value val)
{
    return (uint16_t)addConst(currentChunk(), val);
}

static int identifierConstant(Token *name)
{
    return (int)makeConstant(OBJ_VAL(copyString(name->start,
                                           name->length)));
}

static bool identifiersEqual(Token *a, Token *b)
{
    if (a->length != b->length)
        return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler *compiler, Token *name)
{
    for (int i = compiler->locals.count-1; i >= 0; i--)
    {
        Local *local = &compiler->locals.values[i];
        if (identifiersEqual(name, &local->name))
        {
            if (local->depth == -1)
            {
                error("Can't read local variable in its own initializer.");
            }
      return i;
        }

    }

    return -1;
}

static int addUpvalue(Compiler *compiler, uint16_t index, bool isLocal)
{

    uint16_t upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; i++)
    {
        Upvalue *upvalue = &compiler->upvalues.values[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal)
        {
            return i;
        }
    }

    Upvalue val;
    val.isLocal = isLocal;
    val.index = index;
    writeUpvaluesArray(&compiler->upvalues, val);
    return compiler->upvalues.count - 1; // count is index for next element
}

static int resolveUpvalue(Compiler *compiler, Token *name)
{
    if (compiler->enclosing == NULL)
        return -1; //  if the enclosing Compiler is NULL, we know we’ve
                   //  reached the outermost function without finding a variable.

    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1)
    {
        printf("!local: %d\n", local);
        return addUpvalue(compiler, (uint16_t)local, true);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1)
    {
        printf("!upvalue: %d, %d\n", upvalue, upvalue == -1);
        return addUpvalue(compiler, (uint16_t)upvalue, false);
    }

    return -1;
}

static void addLocal(Token name)
{
    Local local;
    local.depth = -1;
    local.name = name;
    writeLocalsArray(&current->locals, local);
}

static void declareVar()
{
    // Globals are implicitly declared
    if (current->scopeDepth == 0)
        return;

    Token *name = &parser.previous;

    for (int i = current->locals.count - 1; i >= 0; i--)
    {
        Local *local = &current->locals.values[i];
        if (local->depth != -1 && local->depth < current->scopeDepth)
        {
            break;
        }
        if (identifiersEqual(name, &local->name))
        {
            error("Here is already variable with this name in this scope");
        }
    }

    addLocal(*name);
}

static void expression()
{
    parsePrecedence(PREC_ASSIGNMENT);
}

static void block()
{
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
    {
        declaration();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void markInitialized()
{
    if (current->scopeDepth == 0)
        return;

    current->locals.values[current->locals.count - 1].depth = current->scopeDepth;
}

static void defineVar(uint16_t global)
{
    if (current->scopeDepth > 0)
    {
        markInitialized();
        return;
    }

    emitByte(OP_DEFINE_GLOBAL);
    emitBytes((global >> 8) & 0xff, global & 0xff);
}

static uint8_t argumentList()
{
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN))
    {
        do
        {
            expression();

            if (argCount == 255)
            {
                error("Can't have more than 255 arguments");
                // if you need more than 255 arguments, you're doing something wrong :)
            }

            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments");
    return argCount;
}

static uint16_t parseVar(const char *errorMessage)
{
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVar();
    if (current->scopeDepth > 0)
        return 0;

    return identifierConstant(&parser.previous);
}

static void function(FunctionType type)
{
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name");
    if (!check(TOKEN_RIGHT_PAREN))
    {
        do
        {
            current->function->arity++;

            if (current->function->arity > 255)
            {
                errorAtCurrent("Can't have more than 255 parameters");
                // just to not deal with _LONG instructions
                // why would you need more than 255 parameters anyway?
            
                // FIXME: make it until 65535 
            }

            uint16_t constant = parseVar("Expect parameter name");
            defineVar(constant);
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters");

    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body");

    block();

 
    ObjFunc *func = endCompiler();
    uint16_t constant = makeConstant(OBJ_VAL(func));

    emitByte(OP_CLOSURE);
    emitBytes((constant >> 8) & 0xff, constant & 0xff);


    uint16_t count = func->upvalueCount;
    printf("upvalueCount: %04d\n", count);
    for (int i = 0; i < count; i++)
    {
        printf("\n%d\n", compiler.upvalues.values[i].index);
        emitByte(compiler.upvalues.values[i].isLocal ? 1 : 0);
        emitBytes((compiler.upvalues.values[i].index >> 8) & 0xff,
                  compiler.upvalues.values[i].index & 0xff);
    }

    freeUpvaluesArray(&compiler.upvalues);

    printf("\n");
}

static void fnDeclaration()
{
    uint8_t global = parseVar("Expect function name");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVar(global);
}

static void expressionStatement()
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

static void varDeclaration()
{
    uint16_t global = parseVar("Expect variable name.");

    if (match(TOKEN_EQUAL))
    {
        expression();
    }
    else
    {
        emitByte(OP_NIL);
    }

    consume(TOKEN_SEMICOLON, "Expect ';' after variable name");

    defineVar(global);
}

static void synchronize()
{
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF)
    {
        if (parser.previous.type == TOKEN_SEMICOLON)
            return;
        switch (parser.current.type)
        {
        case TOKEN_CLASS:
        case TOKEN_VAR:
        case TOKEN_FOR:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_RETURN:
            return;
        default:
            // Do nothing.
            ;
        }
        advance();
    }
}

static void declaration()
{
    if (match(TOKEN_VAR))
    {
        varDeclaration();
    }
    else if (match(TOKEN_FN))
    {
        fnDeclaration();
    }
    else
    {
        statement();
    }
    if (parser.panicMode)
    {
        synchronize();
    }
}

static void patchJump(int offset)
{
    // -2 to adjust for the bytecode for the jump offset
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX)
    { // shouldn't be a problem
        error("To much code to jump over.");
    }

    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

static void and_(bool canAssign)
{
    int endJump = emitJump(OP_JUMP_FALSE);
    emitByte(OP_POP);
    parsePrecedence(PREC_AND);
    patchJump(endJump);
}

static void or_(bool canAssign)
{
    int elseJump = emitJump(OP_JUMP_FALSE);
    int endJump = emitJump(OP_JUMP);
    patchJump(elseJump);
    emitByte(OP_POP);
    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

static void ifStatement()
{
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition");

    int jumpTo = emitJump(OP_JUMP_FALSE);
    emitByte(OP_POP);
    statement();
    int elseJump = emitJump(OP_JUMP);

    patchJump(jumpTo);
    emitByte(OP_POP);

    if (match(TOKEN_ELSE))
        statement();

    patchJump(elseJump);
}

static void whileStatement()
{
    int loopStart = currentChunk()->count;

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition");
    int exitJump = emitJump(OP_JUMP_FALSE);
    emitByte(OP_POP);
    statement();
    emitLoop(loopStart);
    patchJump(exitJump);
    emitByte(OP_POP);
}

static void forStatement()
{

    beginScope();

    // initializer clause
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'");
    if (match(TOKEN_SEMICOLON))
    {
        // No initializer.
    }
    else if (match(TOKEN_VAR))
    {
        varDeclaration();
    }
    else
    {
        expressionStatement();
    }

    int loopStart = currentChunk()->count;

    // condition clause
    int exitJump = -1;
    if (!match(TOKEN_SEMICOLON))
    {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition");
        // jump out of the loop if the condition is false
        exitJump = emitJump(OP_JUMP_FALSE);
        emitByte(OP_POP);
    }

    // increment clause
    if (!match(TOKEN_RIGHT_PAREN))
    {
        int bodyJump = emitJump(OP_JUMP);
        int incrementStart = currentChunk()->count;
        expression();
        emitByte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses");
        emitLoop(loopStart);
        loopStart = incrementStart;
        patchJump(bodyJump);
    }

    statement();

    emitLoop(loopStart);

    if (exitJump != -1)
    {
        patchJump(exitJump);
        emitByte(OP_POP);
    }

    endScope();
}

static void returnStatement()
{
    if (current->type == TYPE_SCRIPT)
    {
        error("Can't return from top-level code");
    }
    if (match(TOKEN_SEMICOLON))
    {
        emitReturn();
    }
    else
    {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value");
        emitByte(OP_RETURN);
    }
}

static void statement()
{
    if (match(TOKEN_IF))
    {
        ifStatement();
    }
    else if (match(TOKEN_WHILE))
    {
        whileStatement();
    }
    else if (match(TOKEN_FOR))
    {
        forStatement();
    }
    else if (match(TOKEN_LEFT_BRACE))
    {
        beginScope();
        block();
        endScope();
    }
    else if (match(TOKEN_RETURN))
    {
        returnStatement();
    }
    else
    {
        expressionStatement();
    }
}

static void grouping(bool canAssign)
{
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void emitConstant(Value value)
{
    writeConstant(currentChunk(), value, parser.previous.line);
}

static void number(bool canAssign)
{
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUM_VAL(value));
}

static void unary(bool canAssign)
{
    TokenType operatorType = parser.previous.type;

    parsePrecedence(PREC_UNARY);

    switch (operatorType)
    {
    case TOKEN_BANG:
        emitByte(OP_NOT);
        break;
    case TOKEN_MINUS:
        emitByte(OP_NEGATE);
        break;
    default:
        return; // unreachable
    }
}

static void binary(bool canAssign)
{
    TokenType operatorType = parser.previous.type;
    ParseRule *rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));
    switch (operatorType)
    {
    case TOKEN_PLUS:
        emitByte(OP_ADD);
        break;
    case TOKEN_MINUS:
        emitByte(OP_SUBTRACT);
        break;
    case TOKEN_STAR:
        emitByte(OP_MULTIPLY);
        break;
    case TOKEN_SLASH:
        emitByte(OP_DIVIDE);
        break;
    case TOKEN_BANG_EQUAL:
        emitBytes(OP_EQUAL, OP_NOT);
        break;
    case TOKEN_EQUAL_EQUAL:
        emitByte(OP_EQUAL);
        break;
    case TOKEN_GREATER:
        emitByte(OP_GREATER);
        break;
    case TOKEN_GREATER_EQUAL:
        emitBytes(OP_LESS, OP_NOT);
        break;
    case TOKEN_LESS:
        emitByte(OP_LESS);
        break;
    case TOKEN_LESS_EQUAL:
        emitBytes(OP_GREATER, OP_NOT);
        break;
    default:
        return; // unreachable
    }
}

static void call(bool canAssign)
{
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

static void literal(bool canAssign)
{
    switch (parser.previous.type)
    {
    case TOKEN_FALSE:
        emitByte(OP_FALSE);
        break;
    case TOKEN_TRUE:
        emitByte(OP_TRUE);
        break;
    case TOKEN_NIL:
        emitByte(OP_NIL);
        break;
    default:
        break; // unreachable
    }
}

static void string(bool canAssign)
{
    emitConstant(OBJ_VAL(
        copyString(
            parser.previous.start + 1,
            parser.previous.length - 2)));
}

static void namedVar(Token name, bool canAssign)
{
    int arg;  
    uint8_t getOp, setOp;

    arg = resolveLocal(current, &name);
    if (arg != -1)
    {
      printf("local\n");
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    }
    else if ((arg = resolveUpvalue(current, &name)) != -1 ) 
    {
            getOp = OP_GET_UPVALUE;
            setOp = OP_SET_UPVALUE;
    }
    else
    { // global var
            arg = identifierConstant(&name);
            getOp = OP_GET_GLOBAL;
            setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL)) // x = ...
    {
    printf("setOp\n");
        expression();
        emitByte(setOp);
        emitBytes((arg >> 8) & 0xff, arg & 0xff);
    }
    else
    {
    printf("getOp\n");
        emitByte(getOp);
        emitBytes((arg >> 8) & 0xff, arg & 0xff);
    }
}

static void variable(bool canAssign)
{
    namedVar(parser.previous, canAssign);
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static ParseRule *getRule(TokenType type)
{
    return &rules[type];
}

ObjFunc *compile(const char *source)
{
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;

    advance();

    while (!match(TOKEN_EOF))
    {
        declaration();
    }

    ObjFunc *func = endCompiler();
    freeUpvaluesArray(&compiler.upvalues);
    
     return parser.hadError ? NULL : func;
}
