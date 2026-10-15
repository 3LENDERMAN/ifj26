/**
 * IFJ Compiler - Expression parser header file
 * Author: David Pokorák (xpokord00)
 * Date: 15.10.2026
 **/

#include "expr_parser.h"
#include "error.h"
#include "lexer.h"

#include <stdlib.h>
#include <string.h>
/* Precedence table */

typedef enum {
    PS_U,   /* unary minus                */
    PS_MD,  /* * /                        */
    PS_AS,  /* + -                        */
    PS_REL, /* < <= > >=                  */
    PS_EQ,  /* == !=                      */
    PS_LP,  /* (                          */
    PS_RP,  /* )                          */
    PS_I,   /* literal, identifier, call  */
    PS_END, /* end of expression          */
    PS_COUNT,
    PS_E    /* nonterminal E              */
} prec_symbol;

/* '<' shift, '>' reduce, '=' shift, '#' error, 'A' accept.
 * Row = terminal on top of the stack, column = input symbol. */
static const char prec_table[PS_COUNT][PS_COUNT] = {
    /*          U    MD   AS   REL  EQ   (    )    i    $  */
    /* U   */ {'<', '>', '>', '>', '>', '<', '>', '<', '>'},
    /* MD  */ {'<', '>', '>', '>', '>', '<', '>', '<', '>'},
    /* AS  */ {'<', '<', '>', '>', '>', '<', '>', '<', '>'},
    /* REL */ {'<', '<', '<', '#', '>', '<', '>', '<', '>'},
    /* EQ  */ {'<', '<', '<', '<', '#', '<', '>', '<', '>'},
    /* (   */ {'<', '<', '<', '<', '<', '<', '=', '<', '#'},
    /* )   */ {'#', '>', '>', '>', '>', '#', '>', '#', '>'},
    /* i   */ {'#', '>', '>', '>', '>', '#', '>', '#', '>'},
    /* $   */ {'<', '<', '<', '<', '<', '<', '#', '<', 'A'},
};

/* Tree generation functions */
static expr make() {
    /**/
}

/* Token parsing functions */
static void parse_token() {
    /**/
}

/* Stack functions */
typedef struct {
    
} stack;

static void stack_push() {
    /**/
} 

static void stack_pop() {
    /**/
}

expr *parse_expression(TokenStream *ts) {
    /**/
}