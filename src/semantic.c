/**
 * IFJ26 Compiler - Implementation of semantic analysis
 * Author: Vít Kutěj (xkutejv00)
 * Date: 30.10.2026
 **/

#include "semantic.h"
#include "symtable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void semantic_func(func_def *fn, symtable_stack_t *table);
static void semantic_stmt(stmt *s, symtable_stack_t *table, data_type expected_ret);
static void semantic_block(block *blk, symtable_stack_t *table);
static data_type semantic_expr(expr *e, symtable_stack_t *table);


/**
 * Check semantic of the program
 **/
void semantic_analyze(program *prog)
{
    return; // placeholder
}