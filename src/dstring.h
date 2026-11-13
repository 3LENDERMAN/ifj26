/**
 * IFJ26 Compiler - Dynamic string header file
 * Author: Jakub Holub (xholbuj00)
 * Date : 5.10.2026
 *
 **/

#ifndef DSTRING_H
#define DSTRING_H

#include <stdbool.h>
#include <stdlib.h>

typedef struct {
  char *str;
  int length;
  int capacity;
} dyn_string_t;

bool dstring_init(dyn_string_t *s);
bool dstring_append_char(dyn_string_t *s, char c);
void dstring_free(dyn_string_t *s);

#endif
