/**
 * IFJ26 Compiler - Symbol table
 * Author: Jakub Holub (xholubj00)
 * Date: 30.10.2026
 * Implementation: Hash table with open elements addresing 
**/

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>
#include "ast.h" // data types from ast provided by parser

// symbol kind - variable, function or constant
typedef enum {
    SYMBOL_VAR,// variable
    SYMBOL_VAL, // set value, constant
    SYMBOL_FUNC // function
} symbol_kind_t


// symbol data (info known about symbol)
typedef struct {
    symbol_kind_t kind;
    data_type type; // variable type OR func return type

    int param_count; // num of params in function
    data_type *param_types; // array of param types in function

    bool is_init; // variable specific data -> was value assigned to variable?
} symbol_data_t;

// hash table item - open addressing element 
typedef struct {
    char *key; // name of variable or func from lexer analysis
    symbol_data_t data; // symbol info
    bool is_occupied; // T = valid record
    bool is_deleted; // T = item deleted

} ht_item_t;

// hash tabůe structure
typedef struct {
    ht_item_t *items; // pointer to array of items
    int capacity; // total array capacity
    int size; // current number of VALID items

} symtable_t;

// stack of symbol tables (for handling nested loops,functions)
typedef struct {
    symtable_t **tables; // array of pointers to tables (scopes)
    int top;             // stack top index
    int capacity;        // stack capacity
} symtable_stack_t;

//// Interface for SEMANTIC ANALYSIS
// TODO: add interface header functions


#endif // SYMTABLE_H

