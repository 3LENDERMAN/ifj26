/**
 * IFJ26 Compiler - Dynamic string implementation
 * Author: Jakub Holub (xholubj00)
 * Date: 5.10.2026
 * 
**/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "dstring.h"

/**
 * Initialization of empty string
**/
bool dstring_init(dyn_string_t *input_str) {
    if (input_str == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        return false;
    }

    return true;
}

/**
 * Append character to dynamic string
**/
bool dstring_append_char(dyn_string_t *input_str, char c) {
    if (input_str == NULL || input_str->str == NULL) {
        return false;
    }

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
