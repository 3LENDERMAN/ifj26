/**
 * IFJ26 Compiler - main.c
 * Author: Jakub Holub (xholubj00)
 * Date: 5.10.2026
 **/

#include "dstring.h"
#include "error.h"
#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>

// Helper function to print tokens
void print_token(Token *t) {
  if (t == NULL)
    return;

  printf("Token: ");
  switch (t->type) {
  case TOKEN_IDENTIFIER:
    printf("IDENTIFIER (hodnota: '%s')\n", t->attribute.str_val);
    break;
  case TOKEN_KW_IF:
    printf("KW_IF\n");
    break;
  case TOKEN_KW_ELSE:
    printf("KW_ELSE\n");
    break;
  case TOKEN_KW_DEF:
    printf("KW_DEF\n");
    break;
  case TOKEN_KW_VAR:
    printf("KW_VAR\n");
    break;
  case TOKEN_ASSIGN:
    printf("ASSIGN (=)\n");
    break;
  case TOKEN_LESS_EQ:
    printf("LESS_EQ (<=)\n");
    break;
  case TOKEN_PLUS:
    printf("PLUS (+)\n");
    break;
  case TOKEN_DIV:
    printf("DIV (/)\n");
    break;
  case TOKEN_LEFT_PAREN:
    printf("LEFT_PAREN ('(')\n");
    break;
  case TOKEN_RIGHT_PAREN:
    printf("RIGHT_PAREN (')')\n");
    break;
  case TOKEN_NEWLINE:
    printf("NEWLINE (\\n)\n");
    break;
  case TOKEN_EOF:
    printf("EOF (Konec souboru)\n");
    break;
  default:
    printf("other (ID of type: %d)\n", t->type);
    break;
  }
}

int main(int argc, char **argv) {
  FILE *input = stdin;
  if (argc > 1) {
    input = fopen(argv[1], "r");
    if (input == NULL) {
      fprintf(stderr, "Error:cannot open file %s\n", argv[1]);
      return 1;
    }
  }

  printf("--- START LEXER TEST ---\n");

  Token *t = NULL;
  do {
    t = get_token(input);
    print_token(t);
  } while (t != NULL && t->type != TOKEN_EOF);

  printf("-end of test-\n");

  if (input != stdin) {
    fclose(input);
  }
  cleanup_garbage();

  return 0;
}
