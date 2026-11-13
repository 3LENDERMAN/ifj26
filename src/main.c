/**
 * IFJ26 Compiler - main.c
 * Author: Jakub Holub (xholubj00)
 * Date: 30.10.2026
 **/

#include "error.h"
#include <stdio.h>
#include <stdlib.h>
// #include "parser.h"      // later..
// #include "symtable.h"    // later..

int main(int argc, char **argv) {
  // Set input (stdin or file)
  FILE *input = stdin;
  if (argc > 1) {
    input = fopen(argv[1], "r");
    if (input == NULL) {
      fprintf(stderr, "Error: cannot open file %s\n", argv[1]);
      return 1;
    }
  }

  // TODO: init of compiler, call parser

  if (input != stdin) {
    fclose(input);
  }

  // cleanup dynamic memory allocation
  cleanup_garbage();

  return 0;
}
