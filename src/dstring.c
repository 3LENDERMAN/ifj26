/**
 * IFJ26 Compiler - Dynamic string implementation
 * Author: Jakub Holub (xholubj00)
 * Date: 5.10.2026
 *
 **/

#include "dstring.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define DSTRING_INIT_MAX_SIZE 10
/**
 * Initialization of empty string
 **/
bool dstring_init(dyn_string_t *input_str) {
  if (input_str == NULL) {
    fprintf(stderr, "Memory allocation failed.\n");
    return false;
  }

  // Allocate memory from heap
  input_str->str = (char *)malloc(DSTRING_INIT_MAX_SIZE * sizeof(char));

  if (input_str->str == NULL) {
    return false;
  }

  // Init values
  input_str->length = 0;
  input_str->capacity = DSTRING_INIT_MAX_SIZE;
  input_str->str[0] = '\0';

  return true;
}

/**
 * Append character to dynamic string
 **/
bool dstring_append_char(dyn_string_t *input_str, char c) {
  if (input_str == NULL || input_str->str == NULL) {
    return false;
  }

  if (input_str->length + 1 >= input_str->capacity) {
    int new_capacity = input_str->capacity * 2;

    // Safe reallocation with temporary pointer
    char *temp = (char *)realloc(input_str->str, new_capacity * sizeof(char));

    if (temp == NULL) {
      return false;
    }

    input_str->str = temp;
    input_str->capacity = new_capacity;
  }

  // Write char on index of current length
  input_str->str[input_str->length] = c;
  input_str->length++;
  input_str->str[input_str->length] = '\0';

  return true;
}

/**
 * Free up allocated memory
 **/
void dstring_free(dyn_string_t *input_str) {
  if (input_str != NULL && input_str->str != NULL) {
    free(input_str->str);

    input_str->str = NULL;
    input_str->length = 0;
    input_str->capacity = 0;
  }
}
