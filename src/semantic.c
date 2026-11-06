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

    register_builtins(table);

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
    if (!s)
    {
        return;
    }

    switch (s->type)
    {
    case STMT_VAR:
    case STMT_VAL:
    {
        // Deduce type from initialization expression
        data_type init_t = semantic_expr(s->as.decl.init, table);

        if (init_t == TYPE_UNKNOWN)
        {
            throw_error(SEMANTIC_ERROR_DATA_TYPE, "Cannot deduce type from initialization expression");
        }

        // If the variable has a declared type, check if matches the initialization type
        if (s->as.decl.declared != TYPE_UNKNOWN && s->as.decl.declared != init_t)
        {
            throw_error(SEMANTIC_ERROR_DATA_TYPE, "Type does not match the initialization expression");
        }

        s->as.decl.declared = init_t;

        const bool is_const = (s->type == STMT_VAL);
        if (!symtable_insert_var(table, s->as.decl.name, init_t, is_const))
        {
            throw_error(SEMANTIC_ERROR_REDEFINITION, "Variable redefinition");
        }
        break;
    }

    case STMT_ASSIGN:
    {
        ht_item_t *var = symtable_lookup_var(table, s->as.assign.name);
        if (!var)
        {
            throw_error(SEMANTIC_ERROR_UNDEFINED, "Undefined variable");
        }

        if (var->data.kind == SYMBOL_VAL)
        {
            throw_error(SEMANTIC_ERROR_MUTABILITY, "Cannot modify a constant");
        }

        data_type val_t = semantic_expr(s->as.assign.value, table);
        if (var->data.type != val_t && !(var->data.type == TYPE_DOUBLE && val_t == TYPE_INT))
        {
            throw_error(SEMANTIC_ERROR_DATA_TYPE, "Type does not match the assignment expression");
        }
        break;
    }

    case STMT_IF:
    {
        if (semantic_expr(s->as.if_.cond, table) != TYPE_BOOL)
        {
            throw_error(SEMANTIC_ERROR_DATA_TYPE, "'if' condition must be type Bool");
        }
        semantic_block(&s->as.if_.then_b, table);
        semantic_block(&s->as.if_.else_b, table);
        break;
    }

    case STMT_WHILE:
    {
        if (semantic_expr(s->as.while_.cond, table) != TYPE_BOOL)
        {
            throw_error(SEMANTIC_ERROR_DATA_TYPE, "'while' condition must be type Bool");
        }
        semantic_block(&s->as.while_.body, table);
        break;
    }

    case STMT_RETURN:
    {
        data_type ret_t = s->as.expr.expr ? semantic_expr(s->as.expr.expr, table) : TYPE_UNIT;
        if (expected_ret != TYPE_UNKNOWN && expected_ret != TYPE_UNIT)
        {
            if (ret_t != expected_ret && !(expected_ret == TYPE_DOUBLE && ret_t == TYPE_INT))
            {
                throw_error(SEMANTIC_ERROR_DATA_TYPE, "Return type does not match the function's declared return type");
            }
        }
        break;
    }

    case STMT_EXPR:
        semantic_expr(s->as.expr.expr, table);
        break;

    default:
        break;
    }
}

/**
 * Check semantic of the block
 **/
static void semantic_block(block *blk, symtable_stack_t *table)
{
    // Use new scope for local variables
    symtable_enter_scope(table);
    for (int i = 0; i < blk->count; ++i)
    {
        semantic_stmt(blk->items[i], table, TYPE_UNKNOWN);
    }
    symtable_exit_scope(table);
}

static data_type semantic_expr(expr *e, symtable_stack_t *table)
{
    if (!e)
    {
        return TYPE_UNKNOWN;
    }

    switch (e->e_type)
    {
    // Literal values
    case EXPR_INT:
        return TYPE_INT;
    case EXPR_DOUBLE:
        return TYPE_DOUBLE;
    case EXPR_STRING:
        return TYPE_STRING;
    case EXPR_UNIT:
        return TYPE_UNIT;

    case EXPR_VAR:
    {
        ht_item_t *var = symtable_lookup_var(table, e->as.name);
        if (!var)
        {
            throw_error(SEMANTIC_ERROR_UNDEFINED, "Cannot use undeclared variable");
        }
        return var->data.type;
    }

    case EXPR_BINARY:
    {
        // Recursively check the types of the left and right expressions
        data_type l_t = semantic_expr(e->as.binary.left, table);
        data_type r_t = semantic_expr(e->as.binary.right, table);

        // Check for comparison operators, which always return Bool
        if (e->as.binary.op == OP_LT || e->as.binary.op == OP_LE ||
            e->as.binary.op == OP_GT || e->as.binary.op == OP_GE ||
            e->as.binary.op == OP_EQ || e->as.binary.op == OP_NE)
        {
            if (l_t == r_t || ((l_t == TYPE_INT && r_t == TYPE_DOUBLE) || (l_t == TYPE_DOUBLE && r_t == TYPE_INT)))
            {
                return TYPE_BOOL;
            }
            throw_error(SEMANTIC_ERROR_DATA_TYPE, "Type does not match in comparison expression");
        }

        if (l_t == r_t)
        {
            return l_t;
        }

        // Allow implicit conversion from Int to Double
        if ((l_t == TYPE_INT && r_t == TYPE_DOUBLE) || (l_t == TYPE_DOUBLE && r_t == TYPE_INT))
        {
            return TYPE_DOUBLE;
        }

        throw_error(SEMANTIC_ERROR_DATA_TYPE, "Type does not match in binary expression");
    }

    case EXPR_UNARY:
        return semantic_expr(e->as.unary.expr, table);

    case EXPR_CALL:
    {
        ht_item_t *fn = symtable_lookup_func(table, e->as.call.name);
        if (!fn)
        {
            throw_error(SEMANTIC_ERROR_UNDEFINED, "Call to an undeclared function");
        }

        if (e->as.call.argc != fn->data.param_count)
        {
            throw_error(SEMANTIC_ERROR_FUNC_CALL, "Wrong number of arguments in function call");
        }

        // Check argument types
        for (int i = 0; i < e->as.call.argc; ++i)
        {
            data_type arg_t = semantic_expr(e->as.call.args[i], table);
            data_type param_t = fn->data.param_types[i];

            if (arg_t != param_t && !(param_t == TYPE_DOUBLE && arg_t == TYPE_INT))
            {
                throw_error(SEMANTIC_ERROR_DATA_TYPE, "Argument type does not match the function's declared parameter type");
            }
        }

        return fn->data.type;
    }

    default:
        return TYPE_UNKNOWN;
    }
}