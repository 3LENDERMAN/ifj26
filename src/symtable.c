/**
 * IFJ26 Compiler - Source file of symbols table
 * Author : Jakub Holub (xholubj00)
 * Date: 30.10.2026
 * Implementation: Hash table with open addressing (implicit chaining)
**/

#include <stdlib.h>
#include <string.h>
#include "symtable.h"

#define INITIAL_HT_CAPACITY 16
#define INITIAL_STACK_CAPACITY 8

//// Hash table internal private functions:

/**
 * Creates and initializes a single hash table
**/
static symtable_t *table_init(int capacity) {
    symtable_t *table = malloc(sizeof(symtable_t));
    if (table == NULL) return NULL;

    // calloc sets is_occupied and is_deleted to false - 0
    table->items = calloc(capacity, sizeof(ht_item_t));
    if (table->items == NULL) {
        free(table);
        return NULL;
    }

    table->capacity = capacity;
    table->size = 0;
    return table;
}

/**
 * Frees a single hash table and all contents
**/
static void table_free(symtable_t *table) {
    if (table == NULL) return;

    for (int i = 0; i < table->capacity; i++) {
        if (table->items[i].is_occupied) {
            free(table->items[i].key); // free key duplicate

            // If the symbol is a function, it has dynamically allocated array of parameter types
            if (table->items[i].data.kind == SYMBOL_FUNC && table->items[i].data.param_types != NULL) {
                free(table->items[i].data.param_types);
            }
        }
    }
    free(table->items);
    free(table);
}

//// Public functions for semantic analyser


symtable_stack_t *symtable_init(void) {
    symtable_stack_t *stack = malloc(sizeof(symtable_stack_t));
    if (stack == NULL) return NULL;

    stack->capacity = INITIAL_STACK_CAPACITY;
    stack->top = -1;
    stack->tables = malloc(stack->capacity * sizeof(symtable_t *));

    if (stack->tables == NULL) {
        free(stack);
        return NULL;
    }

    // automatically push the global scope (index 0) upon initialization
    symtable_t *global_table = table_init(INITIAL_HT_CAPACITY);
    if (global_table == NULL) {
        free(stack->tables);
        free(stack);
        return NULL;
    }

    stack->top = 0;
    stack->tables[0] = global_table;

    return stack;
}

void symtable_free(symtable_stack_t *stack) {
    if (stack == NULL) return;

    // free all active tables in the stack from bottom to top
    for (int i = 0; i <= stack->top; i++) {
        table_free(stack->tables[i]);
    }

    free(stack->tables);
    free(stack);
}
