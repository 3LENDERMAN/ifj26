/**
 * IFJ26 Compiler - IFJcode26 code generator implementation
 * Author: Oliver Truchlý (xtrucho00)
 * Date: 11.10.2026
 **/

#include "codegen.h"
#include <stdio.h>
#define FUNCTION_LABEL "$%s"

static int label_counter = 0;

// codegen_int_const - int constant
void codegen_int_const(long long value) { printf("int@%lld", value); }

// codegen_float_const - float constant
void codegen_float_const(double value) { printf("float@%a", value); }

// codegen_string_const - string constant with escapes
void codegen_string_const(const char *value) {
  printf("string@");

  for (int i = 0; value[i] != '\0'; i++) {
    unsigned char c = value[i];
    if (c <= 32 || c == '#' || c == '\\' || c >= 127)
      printf("\\%03d", c);
    else
      putchar(c);
  }
}

// codegen_var - local variable
void codegen_var(const char *name, int id) { printf("LF@%s$%d", name, id); }

// codegen_new_label - new label number
int codegen_new_label(void) { return label_counter++; }

// codegen_function_start - function label and frame
void codegen_function_start(const char *name) {
  printf("LABEL " FUNCTION_LABEL "\n", name);
  printf("CREATEFRAME\n");
  printf("PUSHFRAME\n");
}

// codegen_function_end - pop frame and return
void codegen_function_end(void) {
  printf("POPFRAME\n");
  printf("RETURN\n");
}

// codegen_call - function call
void codegen_call(const char *name) {
  printf("CALL " FUNCTION_LABEL "\n", name);
}
