#ifndef iii_locals_h
#define iii_locals_h

#include "scanner.h"

typedef struct
{
  Token name;
  int depth;
} Local;

typedef struct
{
  int capacity;
  int count;
  Local *values;
} LocalsArray;

void initLocalsArray(LocalsArray *array);
void writeLocalsArray(LocalsArray *array, Local value);
void freeLocalsArray(LocalsArray *array);

#endif