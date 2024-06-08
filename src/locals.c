#include "locals.h"
#include "common.h"
#include "memory.h"

void initLocalsArray(LocalsArray *array)
{
  array->values = NULL;
  array->capacity = 0;
  array->count = 0;
}

void writeLocalsArray(LocalsArray *array, Local value)
{
  if (array->capacity < array->count + 1)
  {
    int oldCapacity = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapacity);
    array->values = GROW_ARRAY(Local, array->values,
                               oldCapacity, array->capacity);
  }
  array->values[array->count] = value;
  array->count++;
}

void freeLocalsArray(LocalsArray *array)
{
  FREE_ARRAY(Local, array->values, array->capacity);
  initLocalsArray(array);
}
