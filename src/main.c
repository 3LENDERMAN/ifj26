/**
 * IFJ26 Compiler - main.c
 * Author: Jakub Holub (xholubj00)
 * Date: 5.10.2026
 **/

#include "dstring.h"
#include <stdio.h>

int main() {
  dyn_string_t my_string;

  // Initialization
  if (!dstring_init(&my_string)) {
    printf("Allocation error!\n");
    return 1;
  }

  // Test for realloc
  for (int i = 0; i < 1000; i++) {
    if (!dstring_append_char(&my_string, 'A')) {
      printf("reallocation fault for append %d!\n", i);
      break;
    }
  }

  // Check length and capacity
  printf("Length: %d\n", my_string.length);
  printf("Capacity: %d\n", my_string.capacity);

  dstring_free(&my_string);

  printf("Test successful!\n");

  return 0;
}
