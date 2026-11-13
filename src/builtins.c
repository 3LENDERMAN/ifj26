/**
 * IFJ26 Compiler - Built-in functions implementation
 * Author: Oliver Truchlý (xtrucho00)
 * Date: 1.11.2026
 **/

#include "builtins.h"
#include "codegen.h"
#include <stdio.h>

static void gen_print(void);
static void gen_read_line(void);
static void gen_read_int(void);
static void gen_read_double(void);
static void gen_to_str(void);
static void gen_to_int(void);
static void gen_str_length(void);
static void gen_substr(void);
static void gen_ord(void);
static void gen_chr(void);

// builtins_generate - all built-in functions
void builtins_generate(void) {
  gen_print();
  gen_read_line();
  gen_read_int();
  gen_read_double();
  gen_to_str();
  gen_to_int();
  gen_str_length();
  gen_substr();
  gen_ord();
  gen_chr();
}

// print - writes value to stdout
static void gen_print(void) {
  codegen_function_start("print");
  printf("DEFVAR LF@value\n");
  printf("DEFVAR LF@type\n");
  printf("POPS LF@value\n");

  printf("TYPE LF@type LF@value\n");
  printf("JUMPIFNEQ $print$write LF@type string@nil\n");
  printf("MOVE LF@value string@()\n");
  printf("LABEL $print$write\n");
  printf("WRITE LF@value\n");

  printf("PUSHS nil@nil\n");
  codegen_function_end();
}

// readLine - reads one line
static void gen_read_line(void) {
  codegen_function_start("readLine");
  printf("DEFVAR LF@line\n");
  printf("READ LF@line string\n");
  printf("PUSHS LF@line\n");
  codegen_function_end();
}

// readInt - reads Int, 0 on error
static void gen_read_int(void) {
  codegen_function_start("readInt");
  printf("DEFVAR LF@n\n");
  printf("DEFVAR LF@tmp\n");
  printf("READ LF@n int\n");
  printf("JUMPIFEQ $readInt$zero LF@n nil@nil\n");

  printf("LT LF@tmp LF@n int@-2147483648\n");
  printf("JUMPIFEQ $readInt$zero LF@tmp bool@true\n");
  printf("GT LF@tmp LF@n int@2147483647\n");
  printf("JUMPIFEQ $readInt$zero LF@tmp bool@true\n");
  printf("JUMP $readInt$end\n");

  printf("LABEL $readInt$zero\n");
  printf("MOVE LF@n int@0\n");
  printf("LABEL $readInt$end\n");
  printf("PUSHS LF@n\n");
  codegen_function_end();
}

// readDouble - reads Double, 0.0 on error
static void gen_read_double(void) {
  codegen_function_start("readDouble");
  printf("DEFVAR LF@d\n");
  printf("READ LF@d float\n");
  printf("JUMPIFNEQ $readDouble$end LF@d nil@nil\n");
  printf("MOVE LF@d float@0x0p+0\n");
  printf("LABEL $readDouble$end\n");
  printf("PUSHS LF@d\n");
  codegen_function_end();
}

// toStr - value to String
static void gen_to_str(void) {
  codegen_function_start("toStr");
  printf("DEFVAR LF@value\n");
  printf("DEFVAR LF@type\n");
  printf("POPS LF@value\n");
  printf("TYPE LF@type LF@value\n");

  printf("JUMPIFEQ $toStr$int LF@type string@int\n");
  printf("JUMPIFEQ $toStr$float LF@type string@float\n");
  printf("JUMPIFEQ $toStr$bool LF@type string@bool\n");
  printf("JUMPIFNEQ $toStr$end LF@type string@nil\n");
  printf("MOVE LF@value string@()\n");
  printf("JUMP $toStr$end\n");

  printf("LABEL $toStr$int\n");
  printf("INT2STR LF@value LF@value\n");
  printf("JUMP $toStr$end\n");

  printf("LABEL $toStr$float\n");
  printf("FLOAT2STR LF@value LF@value\n");
  printf("JUMP $toStr$end\n");

  printf("LABEL $toStr$bool\n");
  printf("JUMPIFEQ $toStr$true LF@value bool@true\n");
  printf("MOVE LF@value string@false\n");
  printf("JUMP $toStr$end\n");
  printf("LABEL $toStr$true\n");
  printf("MOVE LF@value string@true\n");

  printf("LABEL $toStr$end\n");
  printf("PUSHS LF@value\n");
  codegen_function_end();
}

// toInt - Double to Int
static void gen_to_int(void) {
  codegen_function_start("toInt");
  printf("FLOAT2INTS\n");
  codegen_function_end();
}

// strLength - length of string
static void gen_str_length(void) {
  codegen_function_start("strLength");
  printf("DEFVAR LF@s\n");
  printf("POPS LF@s\n");
  printf("STRLEN LF@s LF@s\n");
  printf("PUSHS LF@s\n");
  codegen_function_end();
}

// subStr - substring from start to end
static void gen_substr(void) {
  codegen_function_start("subStr");
  printf("DEFVAR LF@s\n");
  printf("DEFVAR LF@i\n");
  printf("DEFVAR LF@end\n");
  printf("DEFVAR LF@len\n");
  printf("DEFVAR LF@tmp\n");
  printf("DEFVAR LF@c\n");
  printf("DEFVAR LF@result\n");
  printf("POPS LF@end\n");
  printf("POPS LF@i\n");
  printf("POPS LF@s\n");
  printf("STRLEN LF@len LF@s\n");

  printf("LT LF@tmp LF@i int@0\n");
  printf("JUMPIFEQ $subStr$error LF@tmp bool@true\n");
  printf("GT LF@tmp LF@i LF@end\n");
  printf("JUMPIFEQ $subStr$error LF@tmp bool@true\n");
  printf("LT LF@tmp LF@i LF@len\n");
  printf("JUMPIFEQ $subStr$error LF@tmp bool@false\n");
  printf("GT LF@tmp LF@end LF@len\n");
  printf("JUMPIFEQ $subStr$error LF@tmp bool@true\n");

  printf("MOVE LF@result string@\n");
  printf("LABEL $subStr$loop\n");
  printf("JUMPIFEQ $subStr$end LF@i LF@end\n");
  printf("GETCHAR LF@c LF@s LF@i\n");
  printf("CONCAT LF@result LF@result LF@c\n");
  printf("ADD LF@i LF@i int@1\n");
  printf("JUMP $subStr$loop\n");

  printf("LABEL $subStr$end\n");
  printf("PUSHS LF@result\n");
  codegen_function_end();

  printf("LABEL $subStr$error\n");
  printf("EXIT int@40\n");
}

// ord - ASCII code at index
static void gen_ord(void) {
  codegen_function_start("ord");
  printf("DEFVAR LF@s\n");
  printf("DEFVAR LF@i\n");
  printf("DEFVAR LF@len\n");
  printf("DEFVAR LF@tmp\n");
  printf("DEFVAR LF@code\n");
  printf("POPS LF@i\n");
  printf("POPS LF@s\n");
  printf("STRLEN LF@len LF@s\n");
  printf("MOVE LF@code int@-1\n");

  printf("LT LF@tmp LF@i int@0\n");
  printf("JUMPIFEQ $ord$end LF@tmp bool@true\n");
  printf("LT LF@tmp LF@i LF@len\n");
  printf("JUMPIFEQ $ord$end LF@tmp bool@false\n");
  printf("STRI2INT LF@code LF@s LF@i\n");

  printf("LABEL $ord$end\n");
  printf("PUSHS LF@code\n");
  codegen_function_end();
}

// chr - string from ASCII code
static void gen_chr(void) {
  codegen_function_start("chr");
  printf("INT2CHARS\n");
  codegen_function_end();
}
