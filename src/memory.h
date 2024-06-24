#ifndef iii_memory_h
#define iii_memory_h

#include "common.h"
#include "object.h"
#include "value.h"


#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)
    
#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))


void* reallocate(void* pointer, size_t oldSize, size_t newSize);
void freeObjects(); 

void collectGarbage();
void markObject(Obj* obj);
void markValue(Value value);

#endif
