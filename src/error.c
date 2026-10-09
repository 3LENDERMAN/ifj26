/**
 * IFJ Compiler - Error message automation
 * Author: Jakub Holub (xholubj00)
 * Date: 7.10.2026
 **/

#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>

/**
 * Prints error message and frees allocated memory
 **/
noreturn void throw_error(Error_code code, const char *msg) {
  fprintf(stderr, "Error %d: %s\n", code, msg);

  cleanup_garbage();
  exit(code);
}

/**
 * Frees memory of all data structures
 **/
void cleanup_garbage(void) {
  // ..
}
