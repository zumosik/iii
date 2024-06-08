#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "chunk.h"
#include "object.h"
#include "locals.h"

Chunk *compilingChunk;

static Chunk *currentChunk()
{
    return compilingChunk;
}

typedef struct
{
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

Parser parser;

typedef struct
{
    LocalsArray locals;
    int scopeDepth;
} Compiler;

Compiler *current = NULL;

static void initCompiler(Compiler *compiler)
{
    initLocalsArray(&compiler->locals);
    compiler->scopeDepth = 0;
    current = compiler;
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

static int emitJump(uint8_t instruction)
{
    emitByte(instruction);
    emitBytes(0xff, 0xff);

    return currentChunk()->count - 2;
}

static void endCompiler()
{

    emitByte(OP_RETURN);

#ifdef DEBUG_PRINT_CODE

#include "debug.h"

    if (!parser.hadError)
    {
        disassembleChunk(currentChunk(), "code");
    }
#endif

    freeLocalsArray(&current->locals);
}

static void beginScope()
{
    current->scopeDepth++;
}

static void endScope()
{
    current->scopeDepth--;

    while (current->locals.count > 0 &&
           current->locals.values[current->locals.count - 1].depth >
               current->scopeDepth)
    {
        emitByte(OP_POP);
        current->locals.count--;
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

static uint16_t identifierConstant(Token *name)
{
    return makeConstant(OBJ_VAL(copyString(name->start,
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
    for (int i = compiler->locals.count - 1; i >= 0; i--)
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

static void expressionStatement()
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

static uint16_t parseVar(const char *errorMessage)
{
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVar();
    if (current->scopeDepth > 0)
        return 0;

    return identifierConstant(&parser.previous);
}

static void markInitialized()
{
    current->locals.values[current->locals.count - 1].depth = current->scopeDepth;
}

static void defineVar(uint16_t global)
{
    if (current->scopeDepth > 0)
    {
        markInitialized();
        return;
    }

    if (global < 256)
    {
        emitBytes(OP_DEFINE_GLOBAL, (uint8_t)global);
    }
    else
    {
        emitByte(OP_DEFINE_GLOBAL_LONG);
        emitBytes((global >> 8) & 0xff, global & 0xff);
    }
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

static void printStatement()
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
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

static void statement()
{
    if (match(TOKEN_PRINT)) 
    {
        printStatement();
    }
    else if (match(TOKEN_IF))
    {
        ifStatement();
    }
    else if (match(TOKEN_LEFT_BRACE))
    {
        beginScope();
        block();
        endScope();
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
    uint16_t arg;
    uint8_t getOp, setOp;
    bool isLong;

    int argLocal = resolveLocal(current, &name);
    if (argLocal > -1)
    {
        arg = (uint16_t)argLocal;
        if (arg < 256)
        {
            isLong = false;
            getOp = OP_GET_LOCAL;
            setOp = OP_SET_LOCAL;
        }
        else
        {
            isLong = true;
            getOp = OP_GET_LOCAL_LONG;
            setOp = OP_SET_LOCAL_LONG;
        }
    }
    else
    { // global var
        arg = identifierConstant(&name);
        if (arg < 256)
        {
            isLong = false;
            getOp = OP_GET_GLOBAL;
            setOp = OP_SET_GLOBAL;
        }
        else
        {
            isLong = true;
            getOp = OP_GET_GLOBAL_LONG;
            setOp = OP_SET_GLOBAL_LONG;
        }
    }

    if (canAssign && match(TOKEN_EQUAL)) // x = ...
    {
        expression();
        if (isLong)
        {
            emitByte(setOp);
            emitBytes((arg >> 8) & 0xff, arg & 0xff);
        }
        else
        {
            emitBytes(setOp, (uint8_t)arg);
        }
    }
    else
    {
        if (isLong)
        {
            emitByte(getOp);
            emitBytes((arg >> 8) & 0xff, arg & 0xff);
        }
        else
        {
            emitBytes(getOp, (uint8_t)arg);
        }
    }
}

static void variable(bool canAssign)
{
    namedVar(parser.previous, canAssign);
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
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

bool compile(const char *source, Chunk *chunk)
{
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler);

    compilingChunk = chunk;
    parser.hadError = false;
    parser.panicMode = false;

    advance();

    while (!match(TOKEN_EOF))
    {
        declaration();
    }

    endCompiler();

    return !parser.hadError;
}
