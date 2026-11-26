/**
 * IFJ26 Compiler - Header file for IFJcode26 code generator
 * Author: Oliver Truchlý (xtrucho00)
 * Date: 11.10.2026
 **/

#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdbool.h>

void codegen_int_const(long long value);
void codegen_float_const(double value);
void codegen_string_const(const char *value);
void codegen_bool_const(bool value);
void codegen_var(const char *name, int id);
int codegen_new_label(void);

void codegen_function_start(const char *name);
void codegen_function_end(void);
void codegen_call(const char *name);

#endif
