/**
 * IFJ26 Compiler - Symbol table
 * Author: Jakub Holub (xholubj00)
 * Date: 30.10.2026
 * Implementation: Hash table with open elements addresing
 **/

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "ast.h" // data types from ast provided by parser
#include <stdbool.h>

// symbol kind - variable, function or constant
typedef enum {
  SYMBOL_VAR, // variable
  SYMBOL_VAL, // set value, constant
  SYMBOL_FUNC // function
} symbol_kind_t;

// symbol data (info known about symbol)
typedef struct {
  symbol_kind_t kind;
  data_type type; // variable type OR func return type

  int param_count;        // num of params in function
  data_type *param_types; // array of param types in function

  bool is_initialized; // variable specific data -> was value assigned to
                       // variable?
} symbol_data_t;

// hash table item - open addressing element
typedef struct {
  char *key;          // name of variable or func from lexer analysis
  symbol_data_t data; // symbol info
  bool is_occupied;   // T = valid record
  bool is_deleted;    // T = item deleted

} ht_item_t;

// hash tabůe structure
typedef struct {
  ht_item_t *items; // pointer to array of items
  int capacity;     // total array capacity
  int size;         // current number of VALID items

} symtable_t;

// stack of symbol tables (for handling nested loops,functions)
typedef struct {
  symtable_t **tables; // array of pointers to tables (scopes)
  int top;             // stack top index
  int capacity;        // stack capacity
} symtable_stack_t;

//// Interface for SEMANTIC ANALYSIS

/**
 * Initializes the symbol table stack.
 */
symtable_stack_t *symtable_init(void);

/**
 * Frees the symbol table stack.
 */
void symtable_free(symtable_stack_t *stack);

/**
 * Enters a new scope (pushes a new table to the stack)
 */
void symtable_enter_scope(symtable_stack_t *stack);

/**
 * Exits the current scope (pops the top table from the stack).
 */
void symtable_exit_scope(symtable_stack_t *stack);

bool symtable_insert_var(symtable_stack_t *stack, const char *name,
                         data_type type, bool is_const);

bool symtable_insert_func(symtable_stack_t *stack, const char *name,
                          data_type ret_type, int param_count, param *params);

ht_item_t *symtable_lookup_var(symtable_stack_t *stack, const char *name);
ht_item_t *symtable_lookup_func(symtable_stack_t *stack, const char *name);

void symtable_register_builtins(symtable_stack_t *stack);

#endif // SYMTABLE_H
