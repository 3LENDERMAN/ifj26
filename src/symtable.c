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

//// helper functions for managing type_info

type_info *symtable_type_primitive(data_type kind) {
  type_info *t = ast_xcalloc(1, sizeof(type_info));
  t->kind = kind;
  return t;
}

type_info *symtable_type_clone(const type_info *t) {
  if (t == NULL) {
    return NULL;
  }

  type_info *copy = ast_xcalloc(1, sizeof(type_info));
  copy->kind = t->kind;

  switch (t->kind) {
  case TYPE_CASE_CLASS:
    if (t->as.class_name != NULL) {
      copy->as.class_name = ast_xstrdup(t->as.class_name);
    }
    break;

  case TYPE_UNION:
    copy->as.union_t.count = t->as.union_t.count;
    if (t->as.union_t.count > 0 && t->as.union_t.members != NULL) {
      copy->as.union_t.members =
          ast_xcalloc((size_t)t->as.union_t.count, sizeof(type_info *));
      for (int i = 0; i < t->as.union_t.count; i++) {
        copy->as.union_t.members[i] =
            symtable_type_clone(t->as.union_t.members[i]);
      }
    }
    break;

  case TYPE_FUNC:
    copy->as.func_t.param_count = t->as.func_t.param_count;
    if (t->as.func_t.param_count > 0 && t->as.func_t.param_types != NULL) {
      copy->as.func_t.param_types =
          ast_xcalloc((size_t)t->as.func_t.param_count, sizeof(type_info *));
      for (int i = 0; i < t->as.func_t.param_count; i++) {
        copy->as.func_t.param_types[i] =
            symtable_type_clone(t->as.func_t.param_types[i]);
      }
    }
    copy->as.func_t.ret_type = symtable_type_clone(t->as.func_t.ret_type);
    break;

  case TYPE_LIST:
    copy->as.elem_type = symtable_type_clone(t->as.elem_type);
    break;

  default:
    break;
  }

  return copy;
}

void symtable_type_free(type_info *t) {
  if (t == NULL) {
    return;
  }

  switch (t->kind) {
  case TYPE_CASE_CLASS:
    free(t->as.class_name);
    break;

  case TYPE_UNION:
    for (int i = 0; i < t->as.union_t.count; i++) {
      symtable_type_free(t->as.union_t.members[i]);
    }
    free(t->as.union_t.members);
    break;

  case TYPE_FUNC:
    for (int i = 0; i < t->as.func_t.param_count; i++) {
      symtable_type_free(t->as.func_t.param_types[i]);
    }
    free(t->as.func_t.param_types);
    symtable_type_free(t->as.func_t.ret_type);
    break;

  case TYPE_LIST:
    symtable_type_free(t->as.elem_type);
    break;

  default:
    break;
  }

  free(t);
}

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
  table->is_isolated = false;
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

      symbol_data_t *d = &table->items[i].data;
      symtable_type_free(d->full_type);

      if (d->param_names != NULL) {
        for (int j = 0; j < d->param_count; j++) {
          free(d->param_names[j]);
        }
        free(d->param_names);
      }

      if (d->param_full_types != NULL) {
        for (int j = 0; j < d->param_count; j++) {
          symtable_type_free(d->param_full_types[j]);
        }
        free(d->param_full_types);
      }

      free(d->param_types);
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
    hash = ((hash << 5) + hash) + (unsigned long)c;
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
      unsigned long index =
          hash_function(old_items[i].key) %
          (unsigned long)new_capacity; // new index for old element
      while (table->items[index].is_occupied) {
        index = (index + 1) % (unsigned long)new_capacity; // find empty spot
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
  unsigned long index = hash_function(key) % (unsigned long)table->capacity;
  int start_index = (int)index;

  // keep looping if element is occupied or deleted
  while (table->items[index].is_occupied || table->items[index].is_deleted) {
    // if elem occupied and key matches -> FOUND
    if (table->items[index].is_occupied &&
        strcmp(table->items[index].key, key) == 0) {
      return &table->items[index];
    }
    index =
        (index + 1) % (unsigned long)table->capacity; // move to next element

    // safety break if we searched the whole table - it should not happen
    if ((int)index == start_index) {
      break;
    }
  }
  return NULL; // key doesnt exit..
}

// finds empty slot for insert and resizes table
static ht_item_t *table_alloc_slot(symtable_t *table, const char *key) {
  if (table_find(table, key) != NULL) {
    return NULL; // collision/redefinition
  }

  if ((float)table->size / (float)table->capacity >= LIMIT_TO_RESIZE) {
    if (!table_resize(table)) {
      return NULL;
    }
  }

  unsigned long index = hash_function(key) % (unsigned long)table->capacity;
  while (table->items[index].is_occupied && !table->items[index].is_deleted) {
    index = (index + 1) % (unsigned long)table->capacity;
  }

  ht_item_t *slot = &table->items[index];
  memset(slot, 0, sizeof(ht_item_t));
  slot->key = ast_xstrdup(key);
  slot->is_occupied = true;
  slot->is_deleted = false;
  table->size++;
  return slot;
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
    int new_cap = stack->capacity * 2;
    symtable_t **new_tables =
        ast_xrealloc(stack->tables, (size_t)new_cap * sizeof(symtable_t *));
    stack->tables = new_tables;
    stack->capacity = new_cap;
  }

  // create new symbol table for the new scope
  symtable_t *new_table = table_init(INITIAL_HT_CAPACITY);

  // push it to the top of the stack
  stack->top++;
  stack->tables[stack->top] = new_table;
}
/**
 * opens isolated scope for lambda func (LAMBDAS):
 * brání přístupu k lokálním proměnným vnějších funkcí (zákaz uzávěrů).
 */
void symtable_enter_lambda_scope(symtable_stack_t *stack) {
  symtable_enter_scope(stack);
  if (stack != NULL && stack->top > 0 && stack->tables[stack->top] != NULL) {
    stack->tables[stack->top]->is_isolated = true;
  }
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
bool symtable_insert_var_typed(symtable_stack_t *stack, const char *name,
                               data_type type, const type_info *full_type,
                               bool is_const) {
  if (stack == NULL || stack->top < 0) {
    return false;
  }

  ht_item_t *slot = table_alloc_slot(stack->tables[stack->top], name);
  if (slot == NULL) {
    return false;
  }

  slot->data.kind = is_const ? SYMBOL_VAL : SYMBOL_VAR;
  slot->data.type = (full_type != NULL) ? full_type->kind : type;
  slot->data.full_type = (full_type != NULL) ? symtable_type_clone(full_type)
                                             : symtable_type_primitive(type);
  slot->data.is_initialized = true;
  return true;
}

bool symtable_insert_var(symtable_stack_t *stack, const char *name,
                         data_type type, bool is_const) {
  return symtable_insert_var_typed(stack, name, type, NULL, is_const);
}

/**
 * Inserts a new function definition into the global scope at the bottom of the
 * stack dynamically allocates memory for parameter types,returns false if the
 * function is already defined
 */
bool symtable_insert_func_typed(symtable_stack_t *stack, const char *name,
                                data_type ret_type,
                                const type_info *ret_full_type, int param_count,
                                param *params) {
  if (stack == NULL) {
    return false;
  }

  // function always belongs to global table (index 0)
  ht_item_t *slot = table_alloc_slot(stack->tables[0], name);
  if (slot == NULL) {
    return false;
  }

  slot->data.kind = SYMBOL_FUNC;
  slot->data.type = (ret_full_type != NULL) ? ret_full_type->kind : ret_type;
  slot->data.full_type = (ret_full_type != NULL)
                             ? symtable_type_clone(ret_full_type)
                             : symtable_type_primitive(ret_type);
  slot->data.param_count = param_count;

  if (param_count > 0 && params != NULL) {
    slot->data.param_types =
        ast_xcalloc((size_t)param_count, sizeof(data_type));
    slot->data.param_full_types =
        ast_xcalloc((size_t)param_count, sizeof(type_info *));

    for (int i = 0; i < param_count; i++) {
      if (params[i].full_type != NULL) {
        slot->data.param_types[i] = params[i].full_type->kind;
        slot->data.param_full_types[i] =
            symtable_type_clone(params[i].full_type);
      } else {
        slot->data.param_types[i] = params[i].type;
        slot->data.param_full_types[i] =
            symtable_type_primitive(params[i].type);
      }
    }
  }

  return true;
}

bool symtable_insert_func(symtable_stack_t *stack, const char *name,
                          data_type ret_type, int param_count, param *params) {
  return symtable_insert_func_typed(stack, name, ret_type, NULL, param_count,
                                    params);
}

/**
 * ALGEBDT: inserts global alias for union of types (type Scalar = Int | String)
 */
bool symtable_insert_type_alias(symtable_stack_t *stack, const char *name,
                                const type_info *aliased_type) {
  if (stack == NULL || aliased_type == NULL) {
    return false;
  }

  ht_item_t *slot = table_alloc_slot(stack->tables[0], name);
  if (slot == NULL) {
    return false;
  }

  slot->data.kind = SYMBOL_TYPE_ALIAS;
  slot->data.type = aliased_type->kind;
  slot->data.full_type = symtable_type_clone(aliased_type);
  return true;
}

/**
 * ALGEBDT: inserts global definition of data class (case class Point(x: Int, y:
 * Int))
 */
bool symtable_insert_case_class(symtable_stack_t *stack, const char *name,
                                int field_count, param *fields) {
  if (stack == NULL) {
    return false;
  }

  ht_item_t *slot = table_alloc_slot(stack->tables[0], name);
  if (slot == NULL) {
    return false;
  }

  slot->data.kind = SYMBOL_CASE_CLASS;
  slot->data.type = TYPE_CASE_CLASS;

  type_info *cls_type = ast_xcalloc(1, sizeof(type_info));
  cls_type->kind = TYPE_CASE_CLASS;
  cls_type->as.class_name = ast_xstrdup(name);
  slot->data.full_type = cls_type;

  slot->data.param_count = field_count;

  if (field_count > 0 && fields != NULL) {
    slot->data.param_names = ast_xcalloc((size_t)field_count, sizeof(char *));
    slot->data.param_types =
        ast_xcalloc((size_t)field_count, sizeof(data_type));
    slot->data.param_full_types =
        ast_xcalloc((size_t)field_count, sizeof(type_info *));

    for (int i = 0; i < field_count; i++) {
      slot->data.param_names[i] = ast_xstrdup(fields[i].name);
      if (fields[i].full_type != NULL) {
        slot->data.param_types[i] = fields[i].full_type->kind;
        slot->data.param_full_types[i] =
            symtable_type_clone(fields[i].full_type);
      } else {
        slot->data.param_types[i] = fields[i].type;
        slot->data.param_full_types[i] =
            symtable_type_primitive(fields[i].type);
      }
    }
  }

  return true;
}
/**
 * Searches for a variable in the symbol table stack from the most local scope
 * down to the global scope handles variable shadowing by returning the closest
 * active definition (local variables have bigger priority than global ones)
 */
ht_item_t *symtable_lookup_var(symtable_stack_t *stack, const char *name) {
  if (stack == NULL) {
    return NULL;
  }
  // Search the stack from top (loacl) to bottom (global)
  for (int i = stack->top; i >= 0; i--) {
    ht_item_t *item = table_find(stack->tables[i], name);
    if (item != NULL &&
        (item->data.kind == SYMBOL_VAR || item->data.kind == SYMBOL_VAL)) {
      return item; // item found
    }
    if (stack->tables[i]->is_isolated) {
      break; // next local variables are outer functions -> not allowed
    }
  }
  return NULL; // not found
}

/**
 * Searches for a function in the global scope at the bottom of the stack (index
 * 0) it provides optimalization
 */
ht_item_t *symtable_lookup_func(symtable_stack_t *stack, const char *name) {
  if (stack == NULL) {
    return NULL;
  }
  ht_item_t *item = table_find(stack->tables[0], name);
  if (item != NULL && item->data.kind == SYMBOL_FUNC) {
    return item;
  }
  return NULL;
}

ht_item_t *symtable_lookup_type(symtable_stack_t *stack, const char *name) {
  if (stack == NULL) {
    return NULL;
  }
  ht_item_t *item = table_find(stack->tables[0], name);
  if (item != NULL && (item->data.kind == SYMBOL_TYPE_ALIAS ||
                       item->data.kind == SYMBOL_CASE_CLASS)) {
    return item;
  }
  return NULL;
}

ht_item_t *symtable_lookup_case_class(symtable_stack_t *stack,
                                      const char *name) {
  if (stack == NULL) {
    return NULL;
  }
  ht_item_t *item = table_find(stack->tables[0], name);
  if (item != NULL && item->data.kind == SYMBOL_CASE_CLASS) {
    return item;
  }
  return NULL;
}

int symtable_case_class_field_index(const ht_item_t *cls_item,
                                    const char *field_name) {
  if (cls_item == NULL || cls_item->data.kind != SYMBOL_CASE_CLASS ||
      field_name == NULL || cls_item->data.param_names == NULL) {
    return -1;
  }

  for (int i = 0; i < cls_item->data.param_count; i++) {
    if (strcmp(cls_item->data.param_names[i], field_name) == 0) {
      return i;
    }
  }
  return -1;
}

/**
 * Register built-in functions
 */
void symtable_register_builtins(symtable_stack_t *stack) {
  if (stack == NULL)
    return;

  param p1 = {0};
  param p_substr[3] = {{0}, {0}, {0}};
  param p_ord[2] = {{0}, {0}};

  p1.name = "value";
  p1.type = TYPE_UNKNOWN;
  symtable_insert_func(stack, "print", TYPE_UNIT, 1, &p1);

  symtable_insert_func(stack, "readLine", TYPE_STRING, 0, NULL);

  symtable_insert_func(stack, "readInt", TYPE_INT, 0, NULL);

  symtable_insert_func(stack, "readDouble", TYPE_DOUBLE, 0, NULL);

  // BOOLTHEN
  symtable_insert_func(stack, "readBoolean", TYPE_BOOL, 0, NULL);

  // ALGEBDT: readIntMaybe -> Int | Unit, readDoubleMaybe -> Double | Unit
  type_info t_int = {.kind = TYPE_INT};
  type_info t_double = {.kind = TYPE_DOUBLE};
  type_info t_unit = {.kind = TYPE_UNIT};

  type_info *int_maybe_members[2] = {&t_int, &t_unit};
  type_info t_int_maybe = {
      .kind = TYPE_UNION,
      .as.union_t = {.members = int_maybe_members, .count = 2}};
  symtable_insert_func_typed(stack, "readIntMaybe", TYPE_UNION, &t_int_maybe, 0,
                             NULL);

  type_info *double_maybe_members[2] = {&t_double, &t_unit};
  type_info t_double_maybe = {
      .kind = TYPE_UNION,
      .as.union_t = {.members = double_maybe_members, .count = 2}};
  symtable_insert_func_typed(stack, "readDoubleMaybe", TYPE_UNION,
                             &t_double_maybe, 0, NULL);

  p1.name = "value";
  p1.type = TYPE_UNKNOWN;
  symtable_insert_func(stack, "toStr", TYPE_STRING, 1, &p1);

  p1.name = "value";
  p1.type = TYPE_DOUBLE;
  symtable_insert_func(stack, "toInt", TYPE_INT, 1, &p1);

  p1.name = "value";
  p1.type = TYPE_STRING;
  symtable_insert_func(stack, "strLength", TYPE_INT, 1, &p1);

  p_substr[0].name = "value";
  p_substr[0].type = TYPE_STRING;
  p_substr[1].name = "start";
  p_substr[1].type = TYPE_INT;
  p_substr[2].name = "end";
  p_substr[2].type = TYPE_INT;
  symtable_insert_func(stack, "subStr", TYPE_STRING, 3, p_substr);

  p_ord[0].name = "value";
  p_ord[0].type = TYPE_STRING;
  p_ord[1].name = "index";
  p_ord[1].type = TYPE_INT;
  symtable_insert_func(stack, "ord", TYPE_INT, 2, p_ord);

  p1.name = "code";
  p1.type = TYPE_INT;
  symtable_insert_func(stack, "chr", TYPE_STRING, 1, &p1);
}
