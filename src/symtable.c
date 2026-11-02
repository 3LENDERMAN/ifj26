/**
 * IFJ26 Compiler - Source file of symbols table
 * Author : Jakub Holub (xholubj00)
 * Date: 30.10.2026
 * Implementation: Hash table with open addressing (implicit chaining)
 **/

#include "symtable.h"
#include <stdlib.h>
#include <string.h>

#define INITIAL_HT_CAPACITY 16
#define INITIAL_STACK_CAPACITY 8
#define LIMIT_TO_RESIZE 0.75 // resizing threshold

//// Hash table internal private functions:

/**
 * Creates and initializes a single hash table
 **/
static symtable_t *table_init(int capacity) {
  symtable_t *table = malloc(sizeof(symtable_t));
  if (table == NULL)
    return NULL;

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
  if (table == NULL)
    return;

  for (int i = 0; i < table->capacity; i++) {
    if (table->items[i].is_occupied) {
      free(table->items[i].key); // free key duplicate

      // If the symbol is a function, it has dynamically allocated array of
      // parameter types
      if (table->items[i].data.kind == SYMBOL_FUNC &&
          table->items[i].data.param_types != NULL) {
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

//// Internal open addressing logic implementation

/**
 * Resizes hash table when its almost full
 */
static bool table_resize(symtable_t *table) {
  int old_capacity = table->capacity;
  ht_item_t *old_items = table->items;
  int new_capacity = old_capacity * 2;
  ht_item_t *new_items =
      calloc(new_capacity, sizeof(ht_item_t)); // create new bigger array
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
      unsigned long index = hash_function(old_items[i].key) %
                            new_capacity; // new index for old element
      while (table->items[index].is_occupied) {
        index = (index + 1) % new_capacity; // find empty spot
      }

      // move item to the new slot
      table->items[index] = old_items[i];
      table->size++;
    }
  }
  free(old_items);
  return true;
}

/**
 * Internal static find function for a single table using linear search
 */
static ht_item_t *table_find(symtable_t *table, const char *key) {
  unsigned long index = hash_function(key) % table->capacity;
  int start_index = (int)index;

  // keep looping if element is occupied or deleted
  while (table->items[index].is_occupied || table->items[index].is_deleted) {
    // if elem occupied and key matches -> FOUND
    if (table->items[index].is_occupied &&
        strcmp(table->items[index].key, key) == 0) {
      return &table->items[index];
    }
    index = (index + 1) % table->capacity; // move to next element

    // safety break if we searched the whole table - it should not happen
    if ((int)index == start_index) {
      break;
    }
  }
  return NULL; // key doesnt exit..
}

//// Public functions for semantic analyser

/**
 * Initializes the symbol table stack and creates global scope (idx 0)
 * function called at the beginning of the semantic analysis
 */
symtable_stack_t *symtable_init(void) {
  symtable_stack_t *stack = malloc(sizeof(symtable_stack_t));
  if (stack == NULL)
    return NULL;

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

/**
 * Frees the whole symbol table stack, including all remaining hash tables
 * all dynamically allocated memory is properly released before the end of
 * compilation
 */
void symtable_free(symtable_stack_t *stack) {
  if (stack == NULL)
    return;

  // free all active tables in the stack from bottom to top
  for (int i = 0; i <= stack->top; i++) {
    table_free(stack->tables[i]);
  }

  free(stack->tables);
  free(stack);
}

/**
 * Enters a new scope by creating new hash table and pushing it on top of the
 * stack used to handle local variables inside blocks (if,while,functions)
 */
void symtable_enter_scope(symtable_stack_t *stack) {
  if (stack == NULL)
    return;

  // if stack is full -> double its capacity
  if (stack->top + 1 >= stack->capacity) {
    stack->capacity *= 2;
    stack->tables =
        realloc(stack->tables, stack->capacity * sizeof(symtable_t *));
  }

  // create new symbol table for the new scope
  symtable_t *new_table = table_init(INITIAL_HT_CAPACITY);

  // push it to the top of the stack
  stack->top++;
  stack->tables[stack->top] = new_table;
}

/**
 * Exits the current scope by destroying the top hash table on the stack
 * all local variables declared in this scope are removed
 */
void symtable_exit_scope(symtable_stack_t *stack) {
  if (stack == NULL)
    return;

  if (stack->top > 0) {
    // free the top table
    table_free(stack->tables[stack->top]);
    stack->top--; // move top pointer down
  }
}

//// Public insert and lookup for semantics

/**
 * Inserts a new variable or constant into the current active scope at the top
 * of stack It uses linear search for placement, returns false if the name is
 * already declared in this scope
 */
bool symtable_insert_var(symtable_stack_t *stack, const char *name,
                         data_type type, bool is_const) {
  symtable_t *current_table = stack->tables[stack->top];
  if (table_find(current_table, name) != NULL) {
    return false;
  }

  // resize if table is getting full
  if ((float)current_table->size / current_table->capacity >= LIMIT_TO_RESIZE) {
    table_resize(current_table);
  }

  // find next empty element using linear search
  unsigned long index = hash_function(name) % current_table->capacity;
  while (current_table->items[index].is_occupied &&
         !current_table->items[index].is_deleted) {
    index = (index + 1) % current_table->capacity;
  }

  // Insert the new variable
  current_table->items[index].key =
      ast_xstrdup(name); // Duplicate string into heap
  current_table->items[index].is_occupied = true;
  current_table->items[index].is_deleted = false;

  current_table->items[index].data.kind = is_const ? SYMBOL_VAL : SYMBOL_VAR;
  current_table->items[index].data.type = type;
  current_table->items[index].data.is_initialized = true;
  current_table->items[index].data.param_count = 0;
  current_table->items[index].data.param_types = NULL;

  current_table->size++;
  return true;
}

/**
 * Inserts a new function definition into the global scope at the bottom of the
 * stack dynamically allocates memory for parameter types,returns false if the
 * function is already defined
 */
bool symtable_insert_func(symtable_stack_t *stack, const char *name,
                          data_type ret_type, int param_count, param *params) {
  // Functions ALWAYS go to the global scope (index 0)
  symtable_t *global_table = stack->tables[0];

  if (table_find(global_table, name) != NULL) {
    return false; // function already exists
  }

  // resize if table is getting full
  if ((float)global_table->size / global_table->capacity >= LIMIT_TO_RESIZE) {
    table_resize(global_table);
  }

  // find next empty element using linear search
  unsigned long index = hash_function(name) % global_table->capacity;
  while (global_table->items[index].is_occupied &&
         !global_table->items[index].is_deleted) {
    index = (index + 1) % global_table->capacity;
  }

  // Insert the new function
  global_table->items[index].key =
      ast_xstrdup(name); // Duplicate string into heap
  global_table->items[index].is_occupied = true;
  global_table->items[index].is_deleted = false;

  global_table->items[index].data.kind = SYMBOL_FUNC;
  global_table->items[index].data.type = ret_type;
  global_table->items[index].data.param_count = param_count;

  // allocate memory for array for parameter types and copy them
  if (param_count > 0 && params != NULL) {
    global_table->items[index].data.param_types =
        malloc(param_count * sizeof(data_type));
    for (int i = 0; i < param_count; i++) {
      global_table->items[index].data.param_types[i] = params[i].type;
    }
  } else {
    global_table->items[index].data.param_types = NULL;
  }

  global_table->size++;
  return true;
}

/**
 * Searches for a variable in the symbol table stack from the most local scope
 * down to the global scope handles variable shadowing by returning the closest
 * active definition (local variables have bigger priority than global ones)
 */
ht_item_t *symtable_lookup_var(symtable_stack_t *stack, const char *name) {
  // Search the stack from top (loacl) to bottom (global)
  for (int i = stack->top; i >= 0; i--) {
    ht_item_t *item = table_find(stack->tables[i], name);
    if (item != NULL) {
      return item; // item found
    }
  }
  return NULL; // not found
}

/**
 * Searches for a function in the global scope at the bottom of the stack (index
 * 0) it provides optimalization
 */
ht_item_t *symtable_lookup_func(symtable_stack_t *stack, const char *name) {
  // functions are only in the global scope (index 0)
  return table_find(stack->tables[0], name);
}

/**
 * Register built-in functions
*/
void symtable_register_builtins(symtable_stack_t *stack) {
    if (stack == NULL) return;

    param p1;
    param p_substr[3];
    param p_ord[2];

    symtable_insert_func(stack, "print", TYPE_UNIT, 0, NULL);

    symtable_insert_func(stack, "readLine", TYPE_STRING, 0, NULL);

    symtable_insert_func(stack, "readInt", TYPE_INT, 0, NULL);

    symtable_insert_func(stack, "readDouble", TYPE_DOUBLE, 0, NULL);

    p1.name = "value"; p1.type = TYPE_UNKNOWN;
    symtable_insert_func(stack, "toStr", TYPE_STRING, 1, &p1);

    p1.name = "value"; p1.type = TYPE_DOUBLE;
    symtable_insert_func(stack, "toInt", TYPE_INT, 1, &p1);

    p1.name = "value"; p1.type = TYPE_STRING;
    symtable_insert_func(stack, "strLength", TYPE_INT, 1, &p1);

    p_substr[0].name = "value"; p_substr[0].type = TYPE_STRING;
    p_substr[1].name = "start"; p_substr[1].type = TYPE_INT;
    p_substr[2].name = "end";   p_substr[2].type = TYPE_INT;
    symtable_insert_func(stack, "subStr", TYPE_STRING, 3, p_substr);

    p_ord[0].name = "value"; p_ord[0].type = TYPE_STRING;
    p_ord[1].name = "index"; p_ord[1].type = TYPE_INT;
    symtable_insert_func(stack, "ord", TYPE_INT, 2, p_ord);

    p1.name = "code"; p1.type = TYPE_INT;
    symtable_insert_func(stack, "chr", TYPE_STRING, 1, &p1);
}
