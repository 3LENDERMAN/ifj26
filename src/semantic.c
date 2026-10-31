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
    symtable_stack_t *table = symtable_init();
    if (!table)
    {
        throw_error(INTERNAL_ERROR, "Allocation of symtable failed");
    }

    // First pass, register functions and check for main function
    int main_counter = 0;

    for (int i = 0; i < prog->count; ++i)
    {
        func_def *fn = prog->funcs[i];

        bool inserted = symtable_insert_func(table, fn->name, fn->ret_type, fn->param_count, fn->params);
        if (!inserted)
        {
            throw_error(SEMANTIC_ERROR_REDEFINITION, "Function redefinition");
        }

        if (fn->is_main)
        {
            main_counter++;
            if (main_counter > 1)
            {
                throw_error(SEMANTIC_ERROR_REDEFINITION, "Main function redefinition");
            }
            if (fn->param_count != 0)
            {
                throw_error(SEMANTIC_ERROR_FUNC_CALL, "Main function cant have parameters");
            }
            if (fn->ret_type != TYPE_UNIT)
            {
                throw_error(SEMANTIC_ERROR_DATA_TYPE, "Main function must return type Unit");
            }
        }
    }

    if (main_counter < 1)
    {
        throw_error(SEMANTIC_ERROR_UNDEFINED, "Missing main function");
    }

    // Second pass, check function bodies
    for (int i = 0; i < prog->count; ++i)
    {
        semantic_func(prog->funcs[i], table);
    }

    symtable_free(table);
}

/**
 * Check semantic of the function, parameters, and body
 **/
static void semantic_func(func_def *fn, symtable_stack_t *table)
{
    // Uses new scope for local variables and parameters
    symtable_enter_scope(table);

    for (int i = 0; i < fn->param_count; ++i)
    {
        param *p = &fn->params[i];
        if (!symtable_insert_var(table, p->name, p->type, true))
        {
            throw_error(SEMANTIC_ERROR_REDEFINITION, "Parameter name redefinition");
        }
    }

    for (int i = 0; i < fn->body.count; ++i)
    {
        semantic_stmt(fn->body.items[i], table, fn->ret_type);
    }

    // Deduce return type from the last statement if its expression, else it must be Unit
    data_type body_t = TYPE_UNIT;
    if (fn->body.count > 0)
    {
        stmt *last = fn->body.items[fn->body.count - 1];
        if (last && last->type == STMT_EXPR)
        {
            body_t = semantic_expr(last->as.expr.expr, table);
        }
    }

    // Check if return type match and check for implicit return type
    if (fn->ret_type != TYPE_UNIT && fn->ret_type != TYPE_UNKNOWN)
    {
        if (body_t != fn->ret_type && !(fn->ret_type == TYPE_DOUBLE && body_t == TYPE_INT))
        {
            throw_error(SEMANTIC_ERROR_DATA_TYPE, "Function body return type does not match");
        }
    }

    symtable_exit_scope(table);
}

static void semantic_stmt(stmt *s, symtable_stack_t *table, data_type expected_ret)
{
    return; //placeholder
}

static data_type semantic_expr(expr *e, symtable_stack_t *table)
{
    return; //placeholder
}