// Hash table implementation for storing key-value pairs

#ifndef iii_table_h
#define iii_table_h

#include "common.h"
#include "value.h"

typedef struct {
    ObjString * key;
    Value value;
} Entry;

typedef struct  {
    int count;
    int capacity;
    Entry *entries;
} Table;

void initTable(Table *table);
void freeTable(Table *table);

bool tableSet(Table *table, ObjString *key, Value value);
bool tableGet(Table *table, ObjString *key, Value *value);
bool tableDelete(Table* table, ObjString* key);

ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);

void tableAddAll(Table *from, Table *to);

// mark all entry keys and values for GC  
void markTable(Table* table);

#endif //iii_table_h
