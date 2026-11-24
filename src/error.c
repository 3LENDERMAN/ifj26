/**
 * IFJ Compiler - Error message automation
 * Author: Jakub Holub (xholubj00)
 * Date: 7.10.2026
 **/

#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>

#define GB_COLLECT_CAPACITY 20000

static void *gb_collector_arr[GB_COLLECT_CAPACITY];
static int gb_collector_count = 0;

/**
 * Prints error message and frees allocated memory
 **/
noreturn void throw_error(Error_code code, const char *msg) {
  fprintf(stderr, "Error %d: %s\n", code, msg);

  cleanup_garbage();
  exit(code);
}

/**
 * Insert new structure to static array to be freed in cleanup_garbage
 **/
void garbage_register(void *ptr) {
  if (ptr != NULL && gb_collector_count < GB_COLLECT_CAPACITY) {
    gb_collector_arr[gb_collector_count] = ptr;
    gb_collector_count++;
  }
}

/**
 * Goes through static array of all registered structures
 * Frees memory of all those data structures
 **/
void cleanup_garbage(void) {
  for (int idx = 0; idx < gb_collector_count; idx++) {
    free(gb_collector_arr[idx]);
  }

  gb_collector_count = 0;
}
