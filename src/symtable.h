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
  SYMBOL_VAR,  // variable
  SYMBOL_VAL,  // set value, constant
  SYMBOL_FUNC, // function

  SYMBOL_TYPE_ALIAS, // type Scalar = Int | Double  | String
  SYMBOL_CASE_CLASS  // case class Point(x:Int, y: Int);
} symbol_kind_t;

// symbol data (info known about symbol)
typedef struct {
  symbol_kind_t kind;
  data_type type;               // variable type OR func return type
  type_info *full_type;         // complete info about type
  type_info **param_full_types; // complete types of params/attributes

  int param_count;        // num of params in function
  data_type *param_types; // array of param types in function

  bool is_initialized; // variable specific data -> was value assigned to
                       // variable?
  char **param_names;
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
  bool is_isolated; // true = edge of lambda func

} symtable_t;

// stack of symbol tables (for handling nested loops,functions)
typedef struct {
  symtable_t **tables; // array of pointers to tables (scopes)
  int top;             // stack top index
  int capacity;        // stack capacity
} symtable_stack_t;

//// Helper functions for type_info

type_info *symtable_type_primitive(data_type kind);
type_info *symtable_type_clone(const type_info *t);
void symtable_type_free(type_info *t);

//// Interface for SEMANTIC ANALYSIS

void symtable_enter_lambda_scope(symtable_stack_t *stack);

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

bool symtable_insert_var_typed(symtable_stack_t *stack, const char *name,
                               data_type type, const type_info *full_type,
                               bool is_const);

bool symtable_insert_func(symtable_stack_t *stack, const char *name,
                          data_type ret_type, int param_count, param *params);

bool symtable_insert_func_typed(symtable_stack_t *stack, const char *name,
                                data_type ret_type,
                                const type_info *ret_full_type, int param_count,
                                param *params);

bool symtable_insert_type_alias(symtable_stack_t *stack, const char *name,
                                const type_info *aliased_type);

bool symtable_insert_case_class(symtable_stack_t *stack, const char *name,
                                int field_count, param *fields);

ht_item_t *symtable_lookup_var(symtable_stack_t *stack, const char *name);
ht_item_t *symtable_lookup_func(symtable_stack_t *stack, const char *name);
ht_item_t *symtable_lookup_type(symtable_stack_t *stack, const char *name);
ht_item_t *symtable_lookup_case_class(symtable_stack_t *stack,
                                      const char *name);

// returns index of attribute case class (0..param_count-1), or -1 if it doesnt
// exists
int symtable_case_class_field_index(const ht_item_t *cls_item,
                                    const char *field_name);

void symtable_register_builtins(symtable_stack_t *stack);

#endif // SYMTABLE_H
