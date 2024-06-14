#ifndef iii_value_h
#define iii_value_h

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

typedef enum
{
    VAL_BOOL,
    VAL_NIL,
    VAL_NUM,
    VAL_OBJ,
} ValueType;

typedef struct
{
    ValueType type;
    union
    {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

// Macro stuff
#define BOOL_VAL(val) ((Value){VAL_BOOL, {.boolean = val}})
#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})
#define NUM_VAL(val) ((Value){VAL_NUM, {.number = val}})
#define OBJ_VAL(object) ((Value){VAL_OBJ, {.obj = (Obj*)object}})

#define AS_BOOL(val) ((val).as.boolean)
#define AS_NUM(val) ((val).as.number)
#define AS_OBJ(value) ((value).as.obj)

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUM)
#define IS_OBJ(value) ((value).type == VAL_OBJ)

typedef struct
{
    int capacity;
    int count;
    Value *values;
} ValueArray;

bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray *array);
void writeValueArray(ValueArray *array, Value value);
void freeValueArray(ValueArray *array);

void printValue(Value value);


#endif