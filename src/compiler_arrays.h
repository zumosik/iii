#ifndef iii_compiler_arrays_h
#define iii_compiler_arrays_h

#include "scanner.h"
#include "common.h"

typedef struct
{
  Token name;
  int depth;
  bool isCaptured;
} Local;

typedef struct
{
  uint16_t index;
  bool isLocal;
} Upvalue;

typedef struct
{
  int capacity;
  int count;
  Local *values;
} LocalsArray;

typedef struct
{
  int capacity;
  int count;
  Upvalue *values;
} UpvaluesArray;

void initLocalsArray(LocalsArray *array);
void writeLocalsArray(LocalsArray *array, Local value);
void freeLocalsArray(LocalsArray *array);

void initUpvaluesArray(UpvaluesArray *array);
void writeUpvaluesArray(UpvaluesArray *array, Upvalue value);
void freeUpvaluesArray(UpvaluesArray *array);

#endif
