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

/**
 * Hash function djb2 - converts string into integer
 */
static unsigned long hash_function(const char *str) {
    unsigned long hash = 5381;
    int c;

    // loop through every char of string until char '\0'
    while ((c = *str++)) {
        // hash * 33 + c
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}


/**
 * Resizes hash table when its almost full 
*/
static bool table_resize(symtable_t *table) {
    int old_capacity = table->capacity;
    ht_item_t *old_items = table->items;
    int new_capacity = old_capacity * 2;
    ht_item_t *new_items = calloc(new_capacity, sizeof(ht_item_t)); // create new bigger array
    if (new_items == NULL) {
        return false;
    }
    // update table ptr and capacity, reset size
    table->capacity = new_capacity;
    table->items = new_items;
    table->size = 0;

    // move and recalculate hash for all existing elems into new array
    for (int i = 0; i < old_capacity; i++) {
        if (old_items[i].is_occupied && !old_items[i].is_deleted) {
            // find new slot
            unsigned long index = hash_function(old_items[i].key) % new_capacity; // new index for old element
            while (table->items[index].is_occupied) {
                index = (index+1) % new_capacity; // find empty spot
            }

            // move item to the new slot
            table->items[index] = old_items[i];
            table->size++;
        }        
    }
    free(old_items);
    return true;
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

void symtable_enter_scope(symtable_stack_t *stack) {
    if (stack ==NULL) return;

    // if stack is full -> double its capacity
    if (stack->top + 1 >= stack->capacity) {
        stack->capacity *= 2;
        stack->tables = realloc(stack->tables, stack->capacity * sizeof(symtable_t *));
    }

    // create new symbol table for the new scope
    symtable_t *new_table = table_init(INITIAL_HT_CAPACITY);

    // push it to the top of the stack
    stack->top++;
    stack->tables[stack->top] = new_table;
}

void symtable_exit_scope(symtable_stack_t *stack) {
    if (stack == NULL) return;

    if (stack->top > 0) {
        // free the top table
        table_free(stack->tables[stack->top]);
        stack->top--; // move top pointer down
    }
}
