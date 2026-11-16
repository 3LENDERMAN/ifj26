/**
 * IFJ26 Compiler - main.c
 * Author: Jakub Holub (xholubj00)
 * Date: 16.11.2026
 **/

#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include "parser.h"      
#include "symtable.h"    
#include "lexer.h"
#include "generator.h"
#include "semantic.h"
#include "token_stream.h"

int main(int argc, char **argv) {
  // Set input (stdin or file)
  FILE *input = stdin;
  if (argc > 1) {
    input = fopen(argv[1], "r");
    if (input == NULL) {
      throw_error(INTERNAL_ERROR, "Could not open input file");
    }
  }

  TokenStream *ts = ts_create(input);
  program *prog = parse(ts);
  ts_destroy(ts);

  if (input != stdin) {
    fclose(input);
  }

  if (!prog) {
    throw_error(SYNTAX_ERROR, "Parser returned empty AST");
  }
  semantic_analyze(prog);
  generate_code(prog);
  ast_free_program(prog);
  // cleanup dynamic memory allocation
  cleanup_garbage();

  return 0;
}
