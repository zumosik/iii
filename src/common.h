#ifndef iii_common_h
#define iii_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define INIT_STRING "init"  // string for init method of class
#define INIT_STRING_LEN 4   // len of init string

#define GC_HEAP_GROW_FACTOR 2    // grow factor for GC
#define GC_BEFORE_FIRST 1048576  // 1024 * 1024 before first GC call

// Uncomment to get debug info
// #define DEBUG_LOG_GC           // logs about GC
// #define DEBUG_STRESS_GC        // run GC as often as it possibly can
// #define DEBUG_PRINT_CODE       // print code after compilation
// #define DEBUG_TRACE_EXECUTION  // print every vm state while running

#endif
