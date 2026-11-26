/**
 * IFJ26 Compiler - Built-in functions implementation
 * Author: Oliver Truchlý (xtrucho00)
 * Date: 1.11.2026
 **/
/*
 * Whole file refactored by AI (Claude Code), prompt: 
 * "Please make me short unified comments for all functions in this file, describing what they do in single line.
 * keep the comments short and concise."
*/

#include "builtins.h"
#include "codegen.h"
#include <stdio.h>

static void gen_print(void);
static void gen_read_line(void);
static void gen_read_int(void);
static void gen_read_int_maybe(void);
static void gen_read_double(void);
static void gen_read_double_maybe(void);
static void gen_read_boolean(void);
static void gen_to_str(void);
static void gen_to_int(void);
static void gen_floor(void);
static void gen_str_length(void);
static void gen_substr(void);
static void gen_ord(void);
static void gen_chr(void);
static void gen_list_make(void);
static void gen_list_apply(void);
static void gen_list_head(void);
static void gen_list_tail(void);
static void gen_list_drop(void);
static void gen_list_take(void);
static void gen_list_slice(void);
static void gen_list_concat(void);
static void gen_list_range(void);
static void gen_list_range_to(void);
static void gen_list_filter(void);
static void gen_list_take_while(void);
static void gen_list_drop_while(void);

// builtins_generate - all built-in functions
void builtins_generate(void) {
  gen_print();
  gen_read_line();
  gen_read_int();
  gen_read_int_maybe();
  gen_read_double();
  gen_read_double_maybe();
  gen_read_boolean();
  gen_to_str();
  gen_to_int();
  gen_floor();
  gen_str_length();
  gen_substr();
  gen_ord();
  gen_chr();
  gen_list_make();
  gen_list_apply();
  gen_list_head();
  gen_list_tail();
  gen_list_drop();
  gen_list_take();
  gen_list_slice();
  gen_list_concat();
  gen_list_range();
  gen_list_range_to();
  gen_list_filter();
  gen_list_take_while();
  gen_list_drop_while();
}

// print - writes value to stdout
static void gen_print(void) {
  codegen_function_start("print");
  printf("DEFVAR LF@value\n");
  printf("DEFVAR LF@type\n");
  printf("POPS LF@value\n");

  printf("TYPE LF@type LF@value\n");
  printf("JUMPIFEQ $print$str LF@type string@nil\n");
  printf("JUMPIFNEQ $print$write LF@type string@array\n");
  printf("LABEL $print$str\n");
  printf("PUSHS LF@value\n");
  codegen_call("toStr");
  printf("POPS LF@value\n");

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
  codegen_call("readIntMaybe");
  printf("POPS LF@n\n");

  printf("JUMPIFNEQ $readInt$end LF@n nil@nil\n");
  printf("MOVE LF@n int@0\n");
  printf("LABEL $readInt$end\n");
  printf("PUSHS LF@n\n");
  codegen_function_end();
}

// readIntMaybe - reads Int, () on error or outside 32 bits
static void gen_read_int_maybe(void) {
  codegen_function_start("readIntMaybe");
  printf("DEFVAR LF@n\n");
  printf("DEFVAR LF@tmp\n");
  printf("READ LF@n int\n");
  printf("JUMPIFEQ $readIntMaybe$end LF@n nil@nil\n");

  printf("LT LF@tmp LF@n int@-2147483648\n");
  printf("JUMPIFEQ $readIntMaybe$nil LF@tmp bool@true\n");
  printf("GT LF@tmp LF@n int@2147483647\n");
  printf("JUMPIFEQ $readIntMaybe$nil LF@tmp bool@true\n");
  printf("JUMP $readIntMaybe$end\n");

  printf("LABEL $readIntMaybe$nil\n");
  printf("MOVE LF@n nil@nil\n");
  printf("LABEL $readIntMaybe$end\n");
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

// readDoubleMaybe - reads Double, () on error
static void gen_read_double_maybe(void) {
  codegen_function_start("readDoubleMaybe");
  printf("DEFVAR LF@d\n");
  printf("READ LF@d float\n");
  printf("PUSHS LF@d\n");
  codegen_function_end();
}

// readBoolean - reads Boolean, false on error
static void gen_read_boolean(void) {
  codegen_function_start("readBoolean");
  printf("DEFVAR LF@b\n");
  printf("READ LF@b bool\n");
  printf("JUMPIFNEQ $readBoolean$end LF@b nil@nil\n");
  printf("MOVE LF@b bool@false\n");
  printf("LABEL $readBoolean$end\n");
  printf("PUSHS LF@b\n");
  codegen_function_end();
}

// toStr - value to String
static void gen_to_str(void) {
  codegen_function_start("toStr");
  printf("DEFVAR LF@value\n");
  printf("DEFVAR LF@type\n");
  printf("DEFVAR LF@text\n");
  printf("DEFVAR LF@len\n");
  printf("DEFVAR LF@i\n");
  printf("DEFVAR LF@item\n");
  printf("DEFVAR LF@tmp\n");
  printf("POPS LF@value\n");
  printf("TYPE LF@type LF@value\n");

  printf("JUMPIFEQ $toStr$int LF@type string@int\n");
  printf("JUMPIFEQ $toStr$float LF@type string@float\n");
  printf("JUMPIFEQ $toStr$bool LF@type string@bool\n");
  printf("JUMPIFEQ $toStr$list LF@type string@array\n");
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
  printf("JUMP $toStr$end\n");

  printf("LABEL $toStr$list\n");
  printf("ARRAYGET LF@len LF@value int@0\n");
  printf("MOVE LF@text string@List(\n");
  printf("MOVE LF@i int@1\n");
  printf("LABEL $toStr$loop\n");
  printf("GT LF@tmp LF@i LF@len\n");
  printf("JUMPIFEQ $toStr$close LF@tmp bool@true\n");
  printf("JUMPIFEQ $toStr$item LF@i int@1\n");
  printf("CONCAT LF@text LF@text string@,\\032\n");
  printf("LABEL $toStr$item\n");
  printf("ARRAYGET LF@item LF@value LF@i\n");
  printf("PUSHS LF@item\n");
  codegen_call("toStr");
  printf("POPS LF@item\n");
  printf("CONCAT LF@text LF@text LF@item\n");
  printf("ADD LF@i LF@i int@1\n");
  printf("JUMP $toStr$loop\n");
  printf("LABEL $toStr$close\n");
  printf("CONCAT LF@value LF@text string@)\n");

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

// floor - Double rounded down
static void gen_floor(void) {
  codegen_function_start("Double$floor");
  printf("DEFVAR LF@d\n");
  printf("DEFVAR LF@t\n");
  printf("DEFVAR LF@tmp\n");
  printf("POPS LF@d\n");
  printf("ISINT LF@tmp LF@d\n");
  printf("JUMPIFEQ $Double$floor$end LF@tmp bool@true\n");

  printf("FLOAT2INT LF@t LF@d\n");
  printf("INT2FLOAT LF@t LF@t\n");
  printf("LT LF@tmp LF@d float@0x0p+0\n");
  printf("JUMPIFEQ $Double$floor$set LF@tmp bool@false\n");
  printf("SUB LF@t LF@t float@0x1p+0\n");
  printf("LABEL $Double$floor$set\n");
  printf("MOVE LF@d LF@t\n");

  printf("LABEL $Double$floor$end\n");
  printf("PUSHS LF@d\n");
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

// lists - array, length at index 0, items from index 1

// List(...) - new list from items
static void gen_list_make(void) {
  codegen_function_start("List$make");
  printf("DEFVAR LF@n\n");
  printf("DEFVAR LF@size\n");
  printf("DEFVAR LF@lst\n");
  printf("DEFVAR LF@item\n");
  printf("POPS LF@n\n");
  printf("ADD LF@size LF@n int@1\n");
  printf("NEWARRAY LF@lst LF@size\n");
  printf("ARRAYSET LF@lst int@0 LF@n\n");

  printf("LABEL $List$make$loop\n");
  printf("JUMPIFEQ $List$make$end LF@n int@0\n");
  printf("POPS LF@item\n");
  printf("ARRAYSET LF@lst LF@n LF@item\n");
  printf("SUB LF@n LF@n int@1\n");
  printf("JUMP $List$make$loop\n");

  printf("LABEL $List$make$end\n");
  printf("PUSHS LF@lst\n");
  codegen_function_end();
}

// lst(i) - item at index i, 40 outside the list
static void gen_list_apply(void) {
  codegen_function_start("List$apply");
  printf("DEFVAR LF@lst\n");
  printf("DEFVAR LF@i\n");
  printf("DEFVAR LF@len\n");
  printf("DEFVAR LF@item\n");
  printf("DEFVAR LF@tmp\n");
  printf("POPS LF@i\n");
  printf("POPS LF@lst\n");
  printf("ARRAYGET LF@len LF@lst int@0\n");

  printf("LT LF@tmp LF@i int@0\n");
  printf("JUMPIFEQ $List$apply$error LF@tmp bool@true\n");
  printf("LT LF@tmp LF@i LF@len\n");
  printf("JUMPIFEQ $List$apply$error LF@tmp bool@false\n");

  printf("ADD LF@i LF@i int@1\n");
  printf("ARRAYGET LF@item LF@lst LF@i\n");
  printf("PUSHS LF@item\n");
  codegen_function_end();

  printf("LABEL $List$apply$error\n");
  printf("EXIT int@40\n");
}

// head - first item, 40 on an empty list
static void gen_list_head(void) {
  codegen_function_start("List$head");
  printf("PUSHS int@0\n");
  codegen_call("List$apply");
  codegen_function_end();
}

// tail - all but the first item, 40 on an empty list
static void gen_list_tail(void) {
  codegen_function_start("List$tail");
  printf("DEFVAR LF@lst\n");
  printf("DEFVAR LF@len\n");
  printf("POPS LF@lst\n");
  printf("ARRAYGET LF@len LF@lst int@0\n");
  printf("JUMPIFEQ $List$tail$error LF@len int@0\n");

  printf("PUSHS LF@lst\n");
  printf("PUSHS int@1\n");
  printf("PUSHS LF@len\n");
  codegen_call("List$slice");
  codegen_function_end();

  printf("LABEL $List$tail$error\n");
  printf("EXIT int@40\n");
}

// drop(n) - list without the first n items
static void gen_list_drop(void) {
  codegen_function_start("List$drop");
  printf("DEFVAR LF@lst\n");
  printf("DEFVAR LF@n\n");
  printf("DEFVAR LF@len\n");
  printf("POPS LF@n\n");
  printf("POPS LF@lst\n");
  printf("ARRAYGET LF@len LF@lst int@0\n");

  printf("PUSHS LF@lst\n");
  printf("PUSHS LF@n\n");
  printf("PUSHS LF@len\n");
  codegen_call("List$slice");
  codegen_function_end();
}

// take(n) - at most the first n items
static void gen_list_take(void) {
  codegen_function_start("List$take");
  printf("DEFVAR LF@n\n");
  printf("POPS LF@n\n");
  printf("PUSHS int@0\n");
  printf("PUSHS LF@n\n");
  codegen_call("List$slice");
  codegen_function_end();
}

// slice(from, until) - items from index from to until - 1
static void gen_list_slice(void) {
  codegen_function_start("List$slice");
  printf("DEFVAR LF@lst\n");
  printf("DEFVAR LF@from\n");
  printf("DEFVAR LF@until\n");
  printf("DEFVAR LF@len\n");
  printf("DEFVAR LF@n\n");
  printf("DEFVAR LF@size\n");
  printf("DEFVAR LF@res\n");
  printf("DEFVAR LF@i\n");
  printf("DEFVAR LF@j\n");
  printf("DEFVAR LF@item\n");
  printf("DEFVAR LF@tmp\n");
  printf("POPS LF@until\n");
  printf("POPS LF@from\n");
  printf("POPS LF@lst\n");
  printf("ARRAYGET LF@len LF@lst int@0\n");

  printf("LT LF@tmp LF@from int@0\n");
  printf("JUMPIFEQ $List$slice$from LF@tmp bool@false\n");
  printf("MOVE LF@from int@0\n");
  printf("LABEL $List$slice$from\n");
  printf("GT LF@tmp LF@until LF@len\n");
  printf("JUMPIFEQ $List$slice$until LF@tmp bool@false\n");
  printf("MOVE LF@until LF@len\n");
  printf("LABEL $List$slice$until\n");
  printf("SUB LF@n LF@until LF@from\n");
  printf("LT LF@tmp LF@n int@0\n");
  printf("JUMPIFEQ $List$slice$alloc LF@tmp bool@false\n");
  printf("MOVE LF@n int@0\n");

  printf("LABEL $List$slice$alloc\n");
  printf("ADD LF@size LF@n int@1\n");
  printf("NEWARRAY LF@res LF@size\n");
  printf("ARRAYSET LF@res int@0 LF@n\n");
  printf("MOVE LF@i int@1\n");
  printf("LABEL $List$slice$loop\n");
  printf("GT LF@tmp LF@i LF@n\n");
  printf("JUMPIFEQ $List$slice$end LF@tmp bool@true\n");
  printf("ADD LF@j LF@from LF@i\n");
  printf("ARRAYGET LF@item LF@lst LF@j\n");
  printf("ARRAYSET LF@res LF@i LF@item\n");
  printf("ADD LF@i LF@i int@1\n");
  printf("JUMP $List$slice$loop\n");

  printf("LABEL $List$slice$end\n");
  printf("PUSHS LF@res\n");
  codegen_function_end();
}

// ++ - joins two lists
static void gen_list_concat(void) {
  codegen_function_start("List$concat");
  printf("DEFVAR LF@a\n");
  printf("DEFVAR LF@b\n");
  printf("DEFVAR LF@la\n");
  printf("DEFVAR LF@lb\n");
  printf("DEFVAR LF@n\n");
  printf("DEFVAR LF@size\n");
  printf("DEFVAR LF@res\n");
  printf("DEFVAR LF@i\n");
  printf("DEFVAR LF@j\n");
  printf("DEFVAR LF@item\n");
  printf("DEFVAR LF@tmp\n");
  printf("POPS LF@b\n");
  printf("POPS LF@a\n");
  printf("ARRAYGET LF@la LF@a int@0\n");
  printf("ARRAYGET LF@lb LF@b int@0\n");
  printf("ADD LF@n LF@la LF@lb\n");
  printf("ADD LF@size LF@n int@1\n");
  printf("NEWARRAY LF@res LF@size\n");
  printf("ARRAYSET LF@res int@0 LF@n\n");

  printf("MOVE LF@i int@1\n");
  printf("LABEL $List$concat$first\n");
  printf("GT LF@tmp LF@i LF@la\n");
  printf("JUMPIFEQ $List$concat$second LF@tmp bool@true\n");
  printf("ARRAYGET LF@item LF@a LF@i\n");
  printf("ARRAYSET LF@res LF@i LF@item\n");
  printf("ADD LF@i LF@i int@1\n");
  printf("JUMP $List$concat$first\n");

  printf("LABEL $List$concat$second\n");
  printf("MOVE LF@j int@1\n");
  printf("LABEL $List$concat$loop\n");
  printf("GT LF@tmp LF@j LF@lb\n");
  printf("JUMPIFEQ $List$concat$end LF@tmp bool@true\n");
  printf("ARRAYGET LF@item LF@b LF@j\n");
  printf("ARRAYSET LF@res LF@i LF@item\n");
  printf("ADD LF@i LF@i int@1\n");
  printf("ADD LF@j LF@j int@1\n");
  printf("JUMP $List$concat$loop\n");

  printf("LABEL $List$concat$end\n");
  printf("PUSHS LF@res\n");
  codegen_function_end();
}

// List.range(start, end, step) - list without end, 40 on zero step
static void gen_list_range(void) {
  codegen_function_start("List$range");
  printf("DEFVAR LF@start\n");
  printf("DEFVAR LF@end\n");
  printf("DEFVAR LF@step\n");
  printf("DEFVAR LF@neg\n");
  printf("DEFVAR LF@n\n");
  printf("DEFVAR LF@size\n");
  printf("DEFVAR LF@res\n");
  printf("DEFVAR LF@i\n");
  printf("DEFVAR LF@tmp\n");
  printf("POPS LF@step\n");
  printf("POPS LF@end\n");
  printf("POPS LF@start\n");
  printf("JUMPIFEQ $List$range$error LF@step int@0\n");
  printf("MOVE LF@n int@0\n");
  printf("GT LF@tmp LF@step int@0\n");
  printf("JUMPIFEQ $List$range$down LF@tmp bool@false\n");

  printf("GT LF@tmp LF@end LF@start\n");
  printf("JUMPIFEQ $List$range$alloc LF@tmp bool@false\n");
  printf("SUB LF@n LF@end LF@start\n");
  printf("ADD LF@n LF@n LF@step\n");
  printf("SUB LF@n LF@n int@1\n");
  printf("IDIV LF@n LF@n LF@step\n");
  printf("JUMP $List$range$alloc\n");

  printf("LABEL $List$range$down\n");
  printf("GT LF@tmp LF@start LF@end\n");
  printf("JUMPIFEQ $List$range$alloc LF@tmp bool@false\n");
  printf("SUB LF@neg int@0 LF@step\n");
  printf("SUB LF@n LF@start LF@end\n");
  printf("ADD LF@n LF@n LF@neg\n");
  printf("SUB LF@n LF@n int@1\n");
  printf("IDIV LF@n LF@n LF@neg\n");

  printf("LABEL $List$range$alloc\n");
  printf("ADD LF@size LF@n int@1\n");
  printf("NEWARRAY LF@res LF@size\n");
  printf("ARRAYSET LF@res int@0 LF@n\n");
  printf("MOVE LF@i int@1\n");
  printf("LABEL $List$range$loop\n");
  printf("GT LF@tmp LF@i LF@n\n");
  printf("JUMPIFEQ $List$range$end LF@tmp bool@true\n");
  printf("ARRAYSET LF@res LF@i LF@start\n");
  printf("ADD LF@start LF@start LF@step\n");
  printf("ADD LF@i LF@i int@1\n");
  printf("JUMP $List$range$loop\n");

  printf("LABEL $List$range$end\n");
  printf("PUSHS LF@res\n");
  codegen_function_end();

  printf("LABEL $List$range$error\n");
  printf("EXIT int@40\n");
}

// (start to end).toList - list with end, 40 on zero step
static void gen_list_range_to(void) {
  codegen_function_start("List$rangeTo");
  printf("DEFVAR LF@end\n");
  printf("DEFVAR LF@step\n");
  printf("DEFVAR LF@tmp\n");
  printf("POPS LF@step\n");
  printf("POPS LF@end\n");
  printf("GT LF@tmp LF@step int@0\n");
  printf("JUMPIFEQ $List$rangeTo$down LF@tmp bool@false\n");
  printf("ADD LF@end LF@end int@1\n");
  printf("JUMP $List$rangeTo$call\n");
  printf("LABEL $List$rangeTo$down\n");
  printf("SUB LF@end LF@end int@1\n");

  printf("LABEL $List$rangeTo$call\n");
  printf("PUSHS LF@end\n");
  printf("PUSHS LF@step\n");
  codegen_call("List$range");
  codegen_function_end();
}

// filter(f) - items for which f returns true
static void gen_list_filter(void) {
  codegen_function_start("List$filter");
  printf("DEFVAR LF@lst\n");
  printf("DEFVAR LF@f\n");
  printf("DEFVAR LF@len\n");
  printf("DEFVAR LF@size\n");
  printf("DEFVAR LF@res\n");
  printf("DEFVAR LF@n\n");
  printf("DEFVAR LF@i\n");
  printf("DEFVAR LF@item\n");
  printf("DEFVAR LF@ok\n");
  printf("DEFVAR LF@tmp\n");
  printf("POPS LF@f\n");
  printf("POPS LF@lst\n");
  printf("ARRAYGET LF@len LF@lst int@0\n");
  printf("ADD LF@size LF@len int@1\n");
  printf("NEWARRAY LF@res LF@size\n");
  printf("MOVE LF@n int@0\n");
  printf("MOVE LF@i int@1\n");

  printf("LABEL $List$filter$loop\n");
  printf("GT LF@tmp LF@i LF@len\n");
  printf("JUMPIFEQ $List$filter$end LF@tmp bool@true\n");
  printf("ARRAYGET LF@item LF@lst LF@i\n");
  printf("PUSHS LF@item\n");
  printf("CALL LF@f\n");
  printf("POPS LF@ok\n");
  printf("JUMPIFEQ $List$filter$next LF@ok bool@false\n");
  printf("ADD LF@n LF@n int@1\n");
  printf("ARRAYSET LF@res LF@n LF@item\n");
  printf("LABEL $List$filter$next\n");
  printf("ADD LF@i LF@i int@1\n");
  printf("JUMP $List$filter$loop\n");

  printf("LABEL $List$filter$end\n");
  printf("ARRAYSET LF@res int@0 LF@n\n");
  printf("PUSHS LF@res\n");
  codegen_function_end();
}

// takeWhile(f) - leading items for which f returns true
static void gen_list_take_while(void) {
  codegen_function_start("List$takeWhile");
  printf("DEFVAR LF@lst\n");
  printf("DEFVAR LF@f\n");
  printf("DEFVAR LF@len\n");
  printf("DEFVAR LF@size\n");
  printf("DEFVAR LF@res\n");
  printf("DEFVAR LF@n\n");
  printf("DEFVAR LF@i\n");
  printf("DEFVAR LF@item\n");
  printf("DEFVAR LF@ok\n");
  printf("DEFVAR LF@tmp\n");
  printf("POPS LF@f\n");
  printf("POPS LF@lst\n");
  printf("ARRAYGET LF@len LF@lst int@0\n");
  printf("ADD LF@size LF@len int@1\n");
  printf("NEWARRAY LF@res LF@size\n");
  printf("MOVE LF@i int@1\n");

  printf("LABEL $List$takeWhile$loop\n");
  printf("GT LF@tmp LF@i LF@len\n");
  printf("JUMPIFEQ $List$takeWhile$end LF@tmp bool@true\n");
  printf("ARRAYGET LF@item LF@lst LF@i\n");
  printf("PUSHS LF@item\n");
  printf("CALL LF@f\n");
  printf("POPS LF@ok\n");
  printf("JUMPIFEQ $List$takeWhile$end LF@ok bool@false\n");
  printf("ARRAYSET LF@res LF@i LF@item\n");
  printf("ADD LF@i LF@i int@1\n");
  printf("JUMP $List$takeWhile$loop\n");

  printf("LABEL $List$takeWhile$end\n");
  printf("SUB LF@n LF@i int@1\n");
  printf("ARRAYSET LF@res int@0 LF@n\n");
  printf("PUSHS LF@res\n");
  codegen_function_end();
}

// dropWhile(f) - items from the first one for which f returns false
static void gen_list_drop_while(void) {
  codegen_function_start("List$dropWhile");
  printf("DEFVAR LF@lst\n");
  printf("DEFVAR LF@f\n");
  printf("DEFVAR LF@len\n");
  printf("DEFVAR LF@i\n");
  printf("DEFVAR LF@item\n");
  printf("DEFVAR LF@ok\n");
  printf("DEFVAR LF@tmp\n");
  printf("POPS LF@f\n");
  printf("POPS LF@lst\n");
  printf("ARRAYGET LF@len LF@lst int@0\n");
  printf("MOVE LF@i int@1\n");

  printf("LABEL $List$dropWhile$loop\n");
  printf("GT LF@tmp LF@i LF@len\n");
  printf("JUMPIFEQ $List$dropWhile$end LF@tmp bool@true\n");
  printf("ARRAYGET LF@item LF@lst LF@i\n");
  printf("PUSHS LF@item\n");
  printf("CALL LF@f\n");
  printf("POPS LF@ok\n");
  printf("JUMPIFEQ $List$dropWhile$end LF@ok bool@false\n");
  printf("ADD LF@i LF@i int@1\n");
  printf("JUMP $List$dropWhile$loop\n");

  printf("LABEL $List$dropWhile$end\n");
  printf("SUB LF@i LF@i int@1\n");
  printf("PUSHS LF@lst\n");
  printf("PUSHS LF@i\n");
  printf("PUSHS LF@len\n");
  codegen_call("List$slice");
  codegen_function_end();
}
