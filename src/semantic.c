/**
 * IFJ26 Compiler - Implementation of semantic analysis
 * Author: Vít Kutěj (xkutejv00)
 * Date: 30.10.2026
 **/

#include "semantic.h"
#include "symtable.h"
#include <stdlib.h>
#include <string.h>

static void semantic_func(func_def *fn, symtable_stack_t *table);
static void semantic_stmt(stmt *s, symtable_stack_t *table,
                          type_info *expected_ret);
static void semantic_block(block *blk, symtable_stack_t *table,
                           type_info *expected_ret);
static data_type semantic_expr(expr *e, symtable_stack_t *table);
static const type_info *branches_value(block *then_b, block *else_b,
                                       const type_info *expected,
                                       bool *diverges);
static void define_type(symtable_stack_t *table, int idx);

/* Analysis state shared between the functions */
static program *current_prog = NULL;
static int *func_state = NULL; // 0 = not analyzed, 1 = in progress, 2 = done
static type_info *current_ret = NULL; // expected return type of current body
static type_info lambda_ret_marker;   // return is forbidden inside lambdas
static int *type_state = NULL;        // like func_state, for type definitions
static const type_info *expect_hint = NULL; // declared type of the next value

// Helpers for the types (complete types, unions, lists, functions)

// Shared immutable type_info of a primitive type
static type_info *prim(data_type kind) { return ast_prim_type(kind); }

// Copy of a type that does not depend on the symbol table (stored to the AST)
static type_info *ty_own(const type_info *t) {
  if (!t) {
    return NULL;
  }
  switch (t->kind) {
  case TYPE_UNION:
  case TYPE_CASE_CLASS:
  case TYPE_FUNC:
  case TYPE_LIST:
    return symtable_type_clone(t);
  default:
    return prim(t->kind);
  }
}

static type_info *ty_list(const type_info *elem) {
  type_info *t = ast_xcalloc(1, sizeof(type_info));
  t->kind = TYPE_LIST;
  t->as.elem_type = ty_own(elem);
  return t;
}

static type_info *ty_func(int param_count) {
  type_info *t = ast_xcalloc(1, sizeof(type_info));
  t->kind = TYPE_FUNC;
  t->as.func_t.param_count = param_count;
  if (param_count > 0) {
    t->as.func_t.param_types =
        ast_xcalloc((size_t)param_count, sizeof(type_info *));
  }
  return t;
}

// Does the type contain an unknown element type (empty List())?
static bool ty_has_unknown(const type_info *t) {
  return !t || t->kind == TYPE_UNKNOWN ||
         (t->kind == TYPE_LIST && ty_has_unknown(t->as.elem_type));
}

// Exact equality of types (unknown element type matches everything)
static bool ty_equal(const type_info *a, const type_info *b) {
  if (!a || !b) {
    return a == b;
  }
  if (a->kind == TYPE_UNKNOWN || b->kind == TYPE_UNKNOWN) {
    return true;
  }
  if (a->kind != b->kind) {
    return false;
  }

  switch (a->kind) {
  case TYPE_CASE_CLASS:
    return strcmp(a->as.class_name, b->as.class_name) == 0;
  case TYPE_LIST:
    return ty_equal(a->as.elem_type, b->as.elem_type);
  case TYPE_UNION:
    if (a->as.union_t.count != b->as.union_t.count) {
      return false;
    }
    for (int i = 0; i < a->as.union_t.count; i++) {
      bool found = false;
      for (int j = 0; j < b->as.union_t.count && !found; j++) {
        found = ty_equal(a->as.union_t.members[i], b->as.union_t.members[j]);
      }
      if (!found) {
        return false;
      }
    }
    return true;
  case TYPE_FUNC:
    if (a->as.func_t.param_count != b->as.func_t.param_count) {
      return false;
    }
    for (int i = 0; i < a->as.func_t.param_count; i++) {
      if (!ty_equal(a->as.func_t.param_types[i], b->as.func_t.param_types[i])) {
        return false;
      }
    }
    return ty_equal(a->as.func_t.ret_type, b->as.func_t.ret_type);
  default:
    return true;
  }
}

// Position of the type t among the members of the union u (u itself when it
// is not a union), -1 when it is not there
static int member_index(const type_info *u, const type_info *t) {
  int n = (u->kind == TYPE_UNION) ? u->as.union_t.count : 1;
  for (int i = 0; i < n; i++) {
    if (ty_equal(u->kind == TYPE_UNION ? u->as.union_t.members[i] : u, t)) {
      return i;
    }
  }
  return -1;
}

// Can a value of type src be used where the type target is expected?
// Allowed: equal types, Int -> Double, member -> union, union -> bigger union
static bool ty_assignable(const type_info *target, const type_info *src) {
  if (!target || !src) {
    return false;
  }
  if (target->kind == TYPE_UNKNOWN) { // Any
    return true;
  }
  if (src->kind == TYPE_UNKNOWN) {
    return false;
  }

  if (src->kind == TYPE_UNION) {
    // every member of the source must be a member of the target union
    if (target->kind != TYPE_UNION) {
      return false;
    }
    for (int i = 0; i < src->as.union_t.count; i++) {
      if (member_index(target, src->as.union_t.members[i]) < 0) {
        return false;
      }
    }
    return true;
  }

  if (target->kind == TYPE_UNION) {
    for (int i = 0; i < target->as.union_t.count; i++) {
      if (ty_assignable(target->as.union_t.members[i], src)) {
        return true;
      }
    }
    return false;
  }

  if (target->kind == TYPE_DOUBLE && src->kind == TYPE_INT) {
    return true;
  }

  if (target->kind == TYPE_FUNC && src->kind == TYPE_FUNC) {
    if (target->as.func_t.param_count != src->as.func_t.param_count) {
      return false;
    }
    for (int i = 0; i < target->as.func_t.param_count; i++) {
      if (!ty_equal(target->as.func_t.param_types[i],
                    src->as.func_t.param_types[i])) {
        return false;
      }
    }
    return target->as.func_t.ret_type->kind == TYPE_UNIT ||
           ty_assignable(target->as.func_t.ret_type, src->as.func_t.ret_type);
  }

  return ty_equal(target, src);
}

// Common type of two branches (if / match), Int and Double give Double;
// branches of different types are joined to the declared type that fits both
static const type_info *ty_join(const type_info *a, const type_info *b,
                                const type_info *expected) {
  if (ty_has_unknown(a) && !ty_has_unknown(b) && ty_assignable(b, a)) {
    return b;
  }
  if (ty_assignable(a, b)) {
    return a;
  }
  if (ty_assignable(b, a)) {
    return b;
  }
  if (expected && ty_assignable(expected, a) && ty_assignable(expected, b)) {
    return expected;
  }
  throw_error(SEMANTIC_ERROR_DATA_TYPE,
              "Types of the branches are not compatible");
}

// Replaces names of aliases by the aliased types, checks that all names exist
static type_info *resolve_type(symtable_stack_t *table, const type_info *t) {
  if (!t) {
    return NULL;
  }

  switch (t->kind) {
  case TYPE_CASE_CLASS: {
    ht_item_t *item = symtable_lookup_type(table, t->as.class_name);
    // ALGEBDT: a type defined later in the file
    for (int i = 0; !item && i < current_prog->type_count; ++i) {
      type_def *def = &current_prog->types[i];
      if (strcmp(def->name, t->as.class_name) != 0) {
        continue;
      }
      if (def->is_class) {
        type_info *cls = ast_xcalloc(1, sizeof(type_info));
        cls->kind = TYPE_CASE_CLASS;
        cls->as.class_name = ast_xstrdup(def->name);
        return cls;
      }
      define_type(table, i);
      item = symtable_lookup_type(table, t->as.class_name);
    }
    if (!item) {
      throw_error(SEMANTIC_ERROR_UNDEFINED, "Use of an undefined type");
    }
    return symtable_type_clone(item->data.full_type);
  }

  case TYPE_LIST: {
    type_info *elem = resolve_type(table, t->as.elem_type);
    type_info *list = ty_list(elem);
    ast_free_type(elem);
    return list;
  }

  case TYPE_FUNC: {
    type_info *func = ty_func(t->as.func_t.param_count);
    for (int i = 0; i < t->as.func_t.param_count; i++) {
      func->as.func_t.param_types[i] =
          resolve_type(table, t->as.func_t.param_types[i]);
    }
    func->as.func_t.ret_type = resolve_type(table, t->as.func_t.ret_type);
    return func;
  }

  case TYPE_UNION: {
    // Members are flattened (alias of union inside of union) and unique
    type_info *uni = ast_xcalloc(1, sizeof(type_info));
    uni->kind = TYPE_UNION;
    for (int i = 0; i < t->as.union_t.count; i++) {
      type_info *m = resolve_type(table, t->as.union_t.members[i]);
      int n = (m->kind == TYPE_UNION) ? m->as.union_t.count : 1;
      for (int k = 0; k < n; k++) {
        type_info *item =
            (m->kind == TYPE_UNION) ? m->as.union_t.members[k] : m;
        if (member_index(uni, item) < 0) {
          APPEND(uni->as.union_t.members, uni->as.union_t.count, ty_own(item));
        }
      }
      ast_free_type(m);
    }
    if (uni->as.union_t.count == 1) {
      type_info *single = ty_own(uni->as.union_t.members[0]);
      ast_free_type(uni);
      return single;
    }
    return uni;
  }

  default:
    return ty_own(t);
  }
}

// Replaces the parsed type by the resolved one
static void resolve_in_place(symtable_stack_t *table, type_info **t) {
  type_info *parsed = *t;
  *t = resolve_type(table, parsed);
  ast_free_type(parsed);
}

// Type of the argument/parameter at the given position of a function symbol
static const type_info *param_type_of(ht_item_t *fn, int i) {
  if (fn->data.param_full_types && fn->data.param_full_types[i]) {
    return fn->data.param_full_types[i];
  }
  return prim(fn->data.param_types[i]);
}

// Type of a function used as a value (LAMBDAS)
static type_info *func_type_of(ht_item_t *fn) {
  type_info *t = ty_func(fn->data.param_count);
  for (int i = 0; i < fn->data.param_count; i++) {
    t->as.func_t.param_types[i] = ty_own(param_type_of(fn, i));
  }
  t->as.func_t.ret_type =
      fn->data.full_type ? ty_own(fn->data.full_type) : prim(fn->data.type);
  return t;
}

// After the type is known from the context, empty List() gets its element type
static void retag(expr *e, const type_info *t) {
  if (e && e->full_type && ty_has_unknown(e->full_type) &&
      t->kind != TYPE_UNKNOWN) {
    ast_free_type(e->full_type);
    e->full_type = ty_own(t);
    e->d_type = t->kind;
  }
}

// Value of an analyzed block = value of its last statement, diverges is set
// when the block always ends by return
static const type_info *block_value(block *blk, const type_info *expected,
                                    bool *diverges) {
  *diverges = false;
  if (!blk || blk->count == 0) {
    return prim(TYPE_UNIT);
  }

  stmt *last = blk->items[blk->count - 1];
  if (!last) {
    return prim(TYPE_UNIT);
  }

  switch (last->type) {
  case STMT_EXPR:
    return (last->as.expr.expr && last->as.expr.expr->full_type)
               ? last->as.expr.expr->full_type
               : prim(TYPE_UNIT);

  case STMT_RETURN:
    *diverges = true;
    return prim(TYPE_UNIT);

  case STMT_IF:
    // Missing else behaves as a branch with ()
    if (last->as.if_.else_b.count == 0) {
      return prim(TYPE_UNIT);
    }
    return branches_value(&last->as.if_.then_b, &last->as.if_.else_b, expected,
                          diverges);

  default:
    return prim(TYPE_UNIT);
  }
}

// Common value of the branches of if, a branch that ends by return is skipped
static const type_info *branches_value(block *then_b, block *else_b,
                                       const type_info *expected,
                                       bool *diverges) {
  bool d1 = false, d2 = false;
  const type_info *t1 = block_value(then_b, expected, &d1);
  const type_info *t2 = block_value(else_b, expected, &d2);
  *diverges = d1 && d2;
  if (d1) {
    return d2 ? prim(TYPE_UNIT) : t2;
  }
  return d2 ? t1 : ty_join(t1, t2, expected);
}

// A function without the declared return type is analyzed before its use
static void analyze_func_idx(symtable_stack_t *table, int idx) {
  if (func_state[idx] != 0) {
    return;
  }
  func_state[idx] = 1;
  // Function body can not see variables of the function that is calling it
  symtable_enter_lambda_scope(table);
  semantic_func(current_prog->funcs[idx], table);
  symtable_exit_scope(table);
  func_state[idx] = 2;
}

// Function symbol, its body is analyzed first when the return type is deduced
static ht_item_t *lookup_func(symtable_stack_t *table, const char *name) {
  ht_item_t *fn = symtable_lookup_func(table, name);
  if (!fn || fn->data.type != TYPE_UNKNOWN) {
    return fn;
  }
  for (int i = 0; i < current_prog->count; ++i) {
    if (strcmp(current_prog->funcs[i]->name, name) == 0) {
      if (func_state[i] == 1) {
        throw_error(SEMANTIC_ERROR_DATA_TYPE,
                    "Recursive function must have an explicit return type");
      }
      analyze_func_idx(table, i);
      break;
    }
  }
  return symtable_lookup_func(table, name);
}

// ALGEBDT: definition of a type alias or a case class, a type alias is defined
// before its first use when the use comes first
static void define_type(symtable_stack_t *table, int idx) {
  if (type_state[idx] == 2) {
    return;
  }
  if (type_state[idx] == 1) {
    throw_error(SEMANTIC_ERROR_DATA_TYPE, "Cyclic type alias");
  }
  type_state[idx] = 1;
  type_def *def = &current_prog->types[idx];

  if (!def->is_class) {
    resolve_in_place(table, &def->alias);
    if (!symtable_insert_type_alias(table, def->name, def->alias)) {
      throw_error(SEMANTIC_ERROR_REDEFINITION, "Type redefinition");
    }
  } else {
    for (int j = 0; j < def->field_count; ++j) {
      param *f = &def->fields[j];
      resolve_in_place(table, &f->full_type);
      f->type = f->full_type->kind;
      for (int k = 0; k < j; ++k) {
        if (strcmp(def->fields[k].name, f->name) == 0) {
          throw_error(SEMANTIC_ERROR_REDEFINITION,
                      "Attribute name redefinition");
        }
      }
    }
    if (!symtable_insert_case_class(table, def->name, def->field_count,
                                    def->fields)) {
      throw_error(SEMANTIC_ERROR_REDEFINITION, "Type redefinition");
    }
  }
  type_state[idx] = 2;
}

/**
 * Check semantic of the program
 **/
void semantic_analyze(program *prog) {
  symtable_stack_t *table = symtable_init();
  if (!table) {
    throw_error(INTERNAL_ERROR, "Allocation of symtable failed");
  }

  symtable_register_builtins(table);

  current_prog = prog;
  func_state =
      ast_xcalloc((size_t)(prog->count > 0 ? prog->count : 1), sizeof(int));

  type_state = ast_xcalloc(
      (size_t)(prog->type_count > 0 ? prog->type_count : 1), sizeof(int));

  // ALGEBDT: type aliases and case classes
  for (int i = 0; i < prog->type_count; ++i) {
    define_type(table, i);
  }

  // First pass, register functions and check for main function
  int main_counter = 0;

  for (int i = 0; i < prog->count; ++i) {
    func_def *fn = prog->funcs[i];

    // Types of parameters and the return type can use the aliases
    for (int j = 0; j < fn->param_count; ++j) {
      param *p = &fn->params[j];
      if (p->full_type) {
        resolve_in_place(table, &p->full_type);
        p->type = p->full_type->kind;
      }
    }
    if (fn->ret_full) {
      resolve_in_place(table, &fn->ret_full);
      fn->ret_type = fn->ret_full->kind;
    }

    bool inserted =
        symtable_insert_func_typed(table, fn->name, fn->ret_type, fn->ret_full,
                                   fn->param_count, fn->params);
    if (!inserted) {
      throw_error(SEMANTIC_ERROR_REDEFINITION, "Function redefinition");
    }

    if (fn->is_main) {
      main_counter++;
      if (main_counter > 1) {
        throw_error(SEMANTIC_ERROR_REDEFINITION, "Main function redefinition");
      }
      if (fn->param_count != 0) {
        throw_error(SEMANTIC_ERROR_FUNC_CALL,
                    "Main function cant have parameters");
      }
      if (fn->ret_type != TYPE_UNIT && fn->ret_type != TYPE_UNKNOWN) {
        throw_error(SEMANTIC_ERROR_DATA_TYPE,
                    "Main function must return type Unit");
      }
      if (fn->ret_type == TYPE_UNKNOWN) {
        fn->ret_type = TYPE_UNIT;
        ht_item_t *main_item = symtable_lookup_func(table, fn->name);
        if (main_item) {
          main_item->data.type = TYPE_UNIT;
        }
      }
    }
  }

  if (main_counter < 1) {
    throw_error(SEMANTIC_ERROR_UNDEFINED, "Missing main function");
  }

  // Second pass, check function bodies
  for (int i = 0; i < prog->count; ++i) {
    analyze_func_idx(table, i);
  }

  free(func_state);
  free(type_state);
  symtable_free(table);
}

// Value whose type is declared by its context (variable, parameter, return
// type): if, match and List(...) may then join types that both fit it
static data_type semantic_expect(expr *e, symtable_stack_t *table,
                                 const type_info *expected) {
  expect_hint = (expected && expected->kind != TYPE_UNKNOWN) ? expected : NULL;
  return semantic_expr(e, table);
}

// Statements of a body, its last expression is a value of the declared type
static void semantic_stmts(block *blk, symtable_stack_t *table,
                           const type_info *expected) {
  for (int i = 0; i < blk->count; ++i) {
    stmt *s = blk->items[i];
    if (i == blk->count - 1 && s && s->type == STMT_EXPR) {
      semantic_expect(s->as.expr.expr, table, expected);
    } else {
      semantic_stmt(s, table, current_ret);
    }
  }
}

static void check_bool(expr *e, symtable_stack_t *table, const char *msg) {
  if (semantic_expr(e, table) != TYPE_BOOL) {
    throw_error(SEMANTIC_ERROR_DATA_TYPE, msg);
  }
}

static void declare(symtable_stack_t *table, const char *name, type_info *t,
                    bool is_const, const char *msg) {
  if (!symtable_insert_var_typed(table, name, t->kind, t, is_const)) {
    throw_error(SEMANTIC_ERROR_REDEFINITION, msg);
  }
}

/**
 * Check semantic of the function, parameters, and body
 **/
static void semantic_func(func_def *fn, symtable_stack_t *table) {
  // Uses new scope for local variables and parameters
  symtable_enter_scope(table);

  for (int i = 0; i < fn->param_count; ++i) {
    declare(table, fn->params[i].name, fn->params[i].full_type, true,
            "Parameter name redefinition");
  }

  // Expected type for return statements, NULL = not declared
  type_info *expected = NULL;
  if (fn->ret_type != TYPE_UNKNOWN) {
    expected = fn->ret_full ? fn->ret_full : prim(fn->ret_type);
  }

  type_info *saved_ret = current_ret;
  current_ret = expected;

  semantic_stmts(&fn->body, table, expected);

  // Deduce return type from the last statement if its expression, else it must
  // be Unit
  // An if statement (or match) at the end behaves as an expression, the type
  // is the common type of its branches
  const type_info *body_t = prim(TYPE_UNIT);
  bool ends_with_flow = false;
  if (fn->ret_type != TYPE_UNIT) {
    body_t = block_value(&fn->body, expected, &ends_with_flow);
  }

  // Check if return type match and check for implicit return type
  if (fn->ret_type == TYPE_UNKNOWN) {
    fn->ret_type = body_t->kind;
    fn->ret_full = ty_own(body_t);

    ht_item_t *func = symtable_lookup_func(table, fn->name);
    if (func) {
      func->data.type = body_t->kind;
      symtable_type_free(func->data.full_type);
      func->data.full_type = symtable_type_clone(fn->ret_full);
    }
  } else if (fn->ret_type != TYPE_UNIT && !ends_with_flow) {
    if (!ty_assignable(expected, body_t)) {
      throw_error(SEMANTIC_ERROR_DATA_TYPE,
                  "Function body return type does not match");
    }
  }

  current_ret = saved_ret;
  symtable_exit_scope(table);
}

static void semantic_stmt(stmt *s, symtable_stack_t *table,
                          type_info *expected_ret) {
  if (!s) {
    return;
  }

  switch (s->type) {
  case STMT_VAR:
  case STMT_VAL: {
    if (s->as.decl.declared_full) {
      resolve_in_place(table, &s->as.decl.declared_full);
    }
    type_info *declared = s->as.decl.declared_full;

    // Deduce type from initialization expression
    data_type init_t = semantic_expect(s->as.decl.init, table, declared);

    if (init_t == TYPE_UNKNOWN) {
      throw_error(SEMANTIC_ERROR_DATA_TYPE,
                  "Cannot deduce type from initialization expression");
    }

    type_info *init_full = s->as.decl.init->full_type;
    type_info *target = init_full;

    // If the variable has a declared type, check if matches the initialization
    // type
    if (declared != NULL) {
      if (!ty_assignable(declared, init_full)) {
        throw_error(SEMANTIC_ERROR_DATA_TYPE,
                    "Type does not match the initialization expression");
      }
      retag(s->as.decl.init, declared);
      s->as.decl.declared = declared->kind;
      target = declared;
    } else {
      // A function can be assigned only to a variable with a declared type
      if (s->as.decl.init->e_type == EXPR_VAR &&
          !symtable_lookup_var(table, s->as.decl.init->as.name)) {
        throw_error(SEMANTIC_ERROR_DATA_TYPE,
                    "Variable holding a function needs an explicit type");
      }
      // Empty list needs the expected type
      if (ty_has_unknown(init_full)) {
        throw_error(SEMANTIC_ERROR_DATA_TYPE,
                    "Cannot deduce type from initialization expression");
      }
      s->as.decl.declared = init_t;
      s->as.decl.declared_full = ty_own(init_full);
    }

    declare(table, s->as.decl.name, target, s->type == STMT_VAL,
            "Variable redefinition");
    break;
  }

  case STMT_ASSIGN: {
    ht_item_t *var = symtable_lookup_var(table, s->as.assign.name);

    if (!var || var->data.kind == SYMBOL_FUNC) {
      throw_error(SEMANTIC_ERROR_UNDEFINED, "Undefined variable");
    }

    if (var->data.kind == SYMBOL_VAL) {
      throw_error(SEMANTIC_ERROR_MUTABILITY, "Cannot modify a constant");
    }

    semantic_expect(s->as.assign.value, table, var->data.full_type);
    if (!ty_assignable(var->data.full_type, s->as.assign.value->full_type)) {
      throw_error(SEMANTIC_ERROR_DATA_TYPE,
                  "Type does not match the assignment expression");
    }
    retag(s->as.assign.value, var->data.full_type);
    break;
  }

  case STMT_IF: {
    check_bool(s->as.if_.cond, table, "'if' condition must be type Bool");
    semantic_block(&s->as.if_.then_b, table, expected_ret);
    if (s->as.if_.else_b.count > 0) {
      semantic_block(&s->as.if_.else_b, table, expected_ret);
    }
    break;
  }

  case STMT_WHILE: {
    check_bool(s->as.while_.cond, table, "'while' condition must be type Bool");
    semantic_block(&s->as.while_.body, table, expected_ret);
    break;
  }

  case STMT_FOR: {
    // Generator: range of Int or a list
    if (semantic_expr(s->as.for_.source, table) != TYPE_LIST) {
      throw_error(SEMANTIC_ERROR_DATA_TYPE,
                  "'for' generator must be a range or a list");
    }
    type_info *elem = s->as.for_.source->full_type->as.elem_type;
    if (ty_has_unknown(elem)) {
      throw_error(SEMANTIC_ERROR_DATA_TYPE,
                  "Cannot deduce type of the list elements");
    }

    // The control variable is an immutable variable of the body
    symtable_enter_scope(table);
    symtable_insert_var_typed(table, s->as.for_.var, elem->kind, elem, true);

    for (int i = 0; i < s->as.for_.guard_count; ++i) {
      check_bool(s->as.for_.guards[i], table,
                 "'for' guard must be type Boolean");
    }
    semantic_block(&s->as.for_.body, table, expected_ret);
    symtable_exit_scope(table);
    break;
  }

  case STMT_RETURN: {
    if (expected_ret == &lambda_ret_marker) {
      throw_error(SEMANTIC_ERROR_OTHER,
                  "Return is not allowed inside of a lambda function");
    }

    if (!s->as.expr.expr) {
      // Return without a value only in a function with the explicit type Unit
      if (!expected_ret || expected_ret->kind != TYPE_UNIT) {
        throw_error(SEMANTIC_ERROR_DATA_TYPE,
                    "Return without a value is allowed only in Unit function");
      }
      break;
    }

    semantic_expect(s->as.expr.expr, table, expected_ret);
    type_info *ret_t = s->as.expr.expr->full_type;
    if (!expected_ret) {
      throw_error(SEMANTIC_ERROR_DATA_TYPE,
                  "Function with return needs an explicit return type");
    }
    if (expected_ret->kind != TYPE_UNIT) {
      if (!ty_assignable(expected_ret, ret_t)) {
        throw_error(
            SEMANTIC_ERROR_DATA_TYPE,
            "Return type does not match the function's declared return type");
      }
      retag(s->as.expr.expr, expected_ret);
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
static void semantic_block(block *blk, symtable_stack_t *table,
                           type_info *expected_ret) {
  // Use new scope for local variables
  symtable_enter_scope(table);
  for (int i = 0; i < blk->count; ++i) {
    semantic_stmt(blk->items[i], table, expected_ret);
  }
  symtable_exit_scope(table);
}

static void check_arg(expr *arg, const type_info *param_t) {
  if (param_t->kind != TYPE_UNKNOWN &&
      !ty_assignable(param_t, arg->full_type)) {
    throw_error(SEMANTIC_ERROR_FUNC_CALL,
                "Argument type does not match the function's declared "
                "parameter type");
  }
  retag(arg, param_t);
}

static void check_argc(int argc, int expected) {
  if (argc != expected) {
    throw_error(SEMANTIC_ERROR_FUNC_CALL,
                "Wrong number of arguments in function call");
  }
}

// LAMBDAS: returns the function type of the lambda
static type_info *semantic_lambda(expr *e, symtable_stack_t *table,
                                  const type_info *expected) {
  const type_info *ret_expected = (expected && expected->kind == TYPE_FUNC)
                                      ? expected->as.func_t.ret_type
                                      : NULL;
  // Lambda can not use local variables of the surrounding function
  symtable_enter_lambda_scope(table);

  type_info *ft = ty_func(e->as.lambda.param_count);

  for (int i = 0; i < e->as.lambda.param_count; ++i) {
    param *p = &e->as.lambda.params[i];
    resolve_in_place(table, &p->full_type);
    p->type = p->full_type->kind;
    declare(table, p->name, p->full_type, true, "Parameter name redefinition");
    ft->as.func_t.param_types[i] = ty_own(p->full_type);
  }

  type_info *saved_ret = current_ret;
  current_ret = &lambda_ret_marker;
  semantic_stmts(&e->as.lambda.body, table, ret_expected);
  current_ret = saved_ret;

  bool diverges = false;
  ft->as.func_t.ret_type =
      ty_own(block_value(&e->as.lambda.body, ret_expected, &diverges));

  symtable_exit_scope(table);
  return ft;
}

// Sets the kind of the member and checks its arguments, returns its type
static type_info *use_member(expr *e, int kind, int argc,
                             const type_info *arg_t, type_info *result) {
  check_argc(e->as.member.argc, argc);
  for (int i = 0; i < argc; ++i) {
    check_arg(e->as.member.args[i], arg_t);
  }
  e->as.member.kind = kind;
  return result;
}

// VALMEM, LISTS, ALGEBDT attributes: value.member
static type_info *semantic_member(expr *e, symtable_stack_t *table) {
  expr *obj = e->as.member.obj;
  const char *name = e->as.member.name;

  semantic_expr(obj, table);
  type_info *ot = obj->full_type;

  for (int i = 0; i < e->as.member.argc; ++i) {
    semantic_expr(e->as.member.args[i], table);
  }

  // Any value
  if (strcmp(name, "toString") == 0) {
    return use_member(e, MEMBER_TO_STRING, 0, NULL, prim(TYPE_STRING));
  }

  if (ot->kind == TYPE_DOUBLE && strcmp(name, "toInt") == 0) {
    return use_member(e, MEMBER_TO_INT, 0, NULL, prim(TYPE_INT));
  }
  if (ot->kind == TYPE_DOUBLE && strcmp(name, "floor") == 0) {
    return use_member(e, MEMBER_FLOOR, 0, NULL, prim(TYPE_DOUBLE));
  }
  if (ot->kind == TYPE_STRING && strcmp(name, "length") == 0) {
    return use_member(e, MEMBER_LENGTH, 0, NULL, prim(TYPE_INT));
  }

  if (ot->kind == TYPE_CASE_CLASS) {
    ht_item_t *cls = symtable_lookup_case_class(table, ot->as.class_name);
    int idx = symtable_case_class_field_index(cls, name);
    if (idx >= 0) {
      e->as.member.field_index = idx;
      return use_member(e, MEMBER_FIELD, 0, NULL,
                        cls->data.param_full_types[idx]);
    }
  }

  if (ot->kind == TYPE_LIST) {
    type_info *elem = ot->as.elem_type;

    if (strcmp(name, "toList") == 0 && obj->e_type == EXPR_RANGE) {
      return use_member(e, MEMBER_LIST_TO_LIST, 0, NULL, ot);
    }
    if (strcmp(name, "drop") == 0) {
      return use_member(e, MEMBER_LIST_DROP, 1, prim(TYPE_INT), ot);
    }
    if (strcmp(name, "take") == 0) {
      return use_member(e, MEMBER_LIST_TAKE, 1, prim(TYPE_INT), ot);
    }
    if (strcmp(name, "slice") == 0) {
      return use_member(e, MEMBER_LIST_SLICE, 2, prim(TYPE_INT), ot);
    }
    if (strcmp(name, "head") == 0) {
      use_member(e, MEMBER_LIST_HEAD, 0, NULL, elem);
      if (ty_has_unknown(elem)) {
        throw_error(SEMANTIC_ERROR_DATA_TYPE, "Head of a list of unknown type");
      }
      return elem;
    }
    if (strcmp(name, "tail") == 0) {
      return use_member(e, MEMBER_LIST_TAIL, 0, NULL, ot);
    }
    static const struct {
      const char *name;
      member_kind kind;
    } preds[] = {{"filter", MEMBER_LIST_FILTER},
                 {"takeWhile", MEMBER_LIST_TAKE_WHILE},
                 {"dropWhile", MEMBER_LIST_DROP_WHILE}};
    for (size_t i = 0; i < sizeof(preds) / sizeof(*preds); i++) {
      if (strcmp(name, preds[i].name) == 0) {
        // Predicate (T) => Boolean
        type_info *pred_params[] = {elem};
        type_info pred = {.kind = TYPE_FUNC,
                          .as.func_t = {.param_types = pred_params,
                                        .param_count = 1,
                                        .ret_type = prim(TYPE_BOOL)}};
        return use_member(e, preds[i].kind, 1, &pred, ot);
      }
    }
  }

  throw_error(SEMANTIC_ERROR_DATA_TYPE, "Unknown member of the value");
}

// ALGEBDT: returns the type of the result of match
static const type_info *semantic_match(expr *e, symtable_stack_t *table,
                                       const type_info *expected) {
  semantic_expr(e->as.match_.subject, table);
  type_info *st = e->as.match_.subject->full_type;

  int members = (st->kind == TYPE_UNION) ? st->as.union_t.count : 1;
  bool *covered = ast_xcalloc((size_t)members, sizeof(bool));
  bool has_default = false;
  const type_info *result = NULL;

  for (int i = 0; i < e->as.match_.case_count; ++i) {
    match_case *c = &e->as.match_.cases[i];

    // Every branch has its own scope for the bound variables
    symtable_enter_scope(table);

    switch (c->pat) {
    case PAT_WILDCARD:
      if (!c->guard) {
        has_default = true;
      }
      break;

    case PAT_LITERAL: {
      semantic_expr(c->literal, table);
      if (!ty_assignable(st, c->literal->full_type)) {
        throw_error(SEMANTIC_ERROR_DATA_TYPE,
                    "Type of the pattern does not match the matched value");
      }
      break;
    }

    case PAT_TYPED: {
      resolve_in_place(table, &c->bind_type);
      int found = member_index(st, c->bind_type);
      if (found < 0) {
        throw_error(SEMANTIC_ERROR_DATA_TYPE,
                    "Type of the pattern does not match the matched value");
      }
      if (!c->guard) {
        covered[found] = true;
      }
      declare(table, c->bind_name, c->bind_type, true, "Variable redefinition");
      break;
    }

    case PAT_CTOR: {
      ht_item_t *cls = symtable_lookup_case_class(table, c->class_name);
      if (!cls) {
        throw_error(SEMANTIC_ERROR_UNDEFINED, "Pattern of an undefined class");
      }
      if (c->field_count != cls->data.param_count) {
        throw_error(SEMANTIC_ERROR_DATA_TYPE,
                    "Wrong number of attributes in the pattern");
      }
      type_info cls_t = {.kind = TYPE_CASE_CLASS,
                         .as.class_name = c->class_name};
      int found = member_index(st, &cls_t);
      if (found < 0) {
        throw_error(SEMANTIC_ERROR_DATA_TYPE,
                    "Type of the pattern does not match the matched value");
      }
      if (!c->guard) {
        covered[found] = true;
      }
      for (int f = 0; f < c->field_count; ++f) {
        if (strcmp(c->fields[f], "_") == 0) {
          continue;
        }
        declare(table, c->fields[f], cls->data.param_full_types[f], true,
                "Variable redefinition");
      }
      break;
    }
    }

    if (c->guard) {
      check_bool(c->guard, table, "Guard must be type Boolean");
    }

    semantic_stmts(&c->body, table, expected);

    bool diverges = false;
    const type_info *bt = block_value(&c->body, expected, &diverges);
    if (!diverges) {
      result = result ? ty_join(result, bt, expected) : bt;
    }

    symtable_exit_scope(table);
  }

  // Match has to be exhaustive
  for (int m = 0; m < members && !has_default; ++m) {
    if (!covered[m]) {
      throw_error(SEMANTIC_ERROR_OTHER, "Match is not exhaustive");
    }
  }

  free(covered);
  return result ? result : prim(TYPE_UNIT);
}

static data_type semantic_expr(expr *e, symtable_stack_t *table) {
  const type_info *expected = expect_hint;
  expect_hint = NULL;
  if (!e) {
    return TYPE_UNKNOWN;
  }

  data_type result_type = TYPE_UNKNOWN;
  const type_info *full = NULL; // complete type, if not just the primitive
  type_info *made = NULL;       // complete type made here, stored as it is

  switch (e->e_type) {
  // Literal values
  case EXPR_INT:
    result_type = TYPE_INT;
    break;
  case EXPR_DOUBLE:
    result_type = TYPE_DOUBLE;
    break;
  case EXPR_STRING:
    result_type = TYPE_STRING;
    break;
  case EXPR_UNIT:
    result_type = TYPE_UNIT;
    break;

  case EXPR_VAR: {
    ht_item_t *var = symtable_lookup_var(table, e->as.name);

    if (var && var->data.kind != SYMBOL_FUNC) {
      result_type = var->data.type;
      full = var->data.full_type;
      break;
    }

    // LAMBDAS: name of a function used as a value
    ht_item_t *fn = lookup_func(table, e->as.name);
    if (fn) {
      made = func_type_of(fn);
      result_type = TYPE_FUNC;
      break;
    }

    throw_error(SEMANTIC_ERROR_UNDEFINED, "Cannot use undeclared variable");
    break;
  }

  case EXPR_BINARY: {
    // Recursively check the types of the left and right expressions
    data_type l_t = semantic_expr(e->as.binary.left, table);
    data_type r_t = semantic_expr(e->as.binary.right, table);

    op_type op = e->as.binary.op;
    bool num = (l_t == TYPE_INT || l_t == TYPE_DOUBLE) &&
               (r_t == TYPE_INT || r_t == TYPE_DOUBLE);
    bool str = (l_t == TYPE_STRING && r_t == TYPE_STRING);

    // Check for comparison operators, which always return Bool
    if (op == OP_LT || op == OP_LE || op == OP_GT || op == OP_GE) {
      if (!str && !num) {
        throw_error(SEMANTIC_ERROR_DATA_TYPE,
                    "Type does not match in comparison expression");
      }
      result_type = TYPE_BOOL;
    } else if (op == OP_EQ || op == OP_NE) {
      type_info *lf = e->as.binary.left->full_type;
      type_info *rf = e->as.binary.right->full_type;
      if (!str && !num && !ty_assignable(lf, rf) && !ty_assignable(rf, lf)) {
        throw_error(SEMANTIC_ERROR_DATA_TYPE,
                    "Type does not match in equality expression");
      }
      result_type = TYPE_BOOL;
    } else if (op == OP_AND || op == OP_OR) {
      if (l_t != TYPE_BOOL || r_t != TYPE_BOOL) {
        throw_error(SEMANTIC_ERROR_DATA_TYPE,
                    "Logical operators require boolean operands");
      }
      result_type = TYPE_BOOL;
    } else if (op == OP_CONCAT && l_t == TYPE_LIST && r_t == TYPE_LIST) {
      // LISTS: concatenation of two lists
      made = ty_list(ty_join(e->as.binary.left->full_type->as.elem_type,
                             e->as.binary.right->full_type->as.elem_type,
                             (expected && expected->kind == TYPE_LIST)
                                 ? expected->as.elem_type
                                 : NULL));
      result_type = TYPE_LIST;
    } else if (op == OP_ADD && str) {
      result_type = TYPE_STRING;
    } else if (num &&
               (op == OP_ADD || op == OP_SUB || op == OP_MUL || op == OP_DIV)) {
      result_type =
          (l_t == TYPE_DOUBLE || r_t == TYPE_DOUBLE) ? TYPE_DOUBLE : TYPE_INT;
    } else {
      throw_error(SEMANTIC_ERROR_DATA_TYPE,
                  "Type does not match in binary expression");
    }
    break;
  }

  case EXPR_UNARY: {
    data_type inner_t = semantic_expr(e->as.unary.expr, table);

    if (e->as.unary.op == OP_NOT) {
      if (inner_t != TYPE_BOOL) {
        throw_error(SEMANTIC_ERROR_DATA_TYPE,
                    "Unary operator '!' requires boolean operand");
      }
      result_type = TYPE_BOOL;
    } else {
      if (inner_t != TYPE_INT && inner_t != TYPE_DOUBLE) {
        throw_error(SEMANTIC_ERROR_DATA_TYPE,
                    "Unary operator requires numeric operand");
      }
      result_type = inner_t;
    }
    break;
  }

  case EXPR_CALL: {
    const char *cname = e->as.call.name;
    int argc = e->as.call.argc;

    // LISTS: List(a, b, ...), type of the elements is the common type
    if (strcmp(cname, "List") == 0) {
      e->as.call.kind = CALL_LIST_NEW;
      const type_info *elem_expected = (expected && expected->kind == TYPE_LIST)
                                           ? expected->as.elem_type
                                           : NULL;
      const type_info *elem = prim(TYPE_UNKNOWN);
      for (int i = 0; i < argc; ++i) {
        semantic_expect(e->as.call.args[i], table, elem_expected);
        const type_info *t = e->as.call.args[i]->full_type;
        elem = (i == 0) ? t : ty_join(elem, t, elem_expected);
      }
      // List(1, 2) declared as List[Double] holds Doubles
      if (elem_expected && argc > 0 && ty_assignable(elem_expected, elem)) {
        elem = elem_expected;
      }
      made = ty_list(elem);
      result_type = TYPE_LIST;
      break;
    }

    // LISTS: List.range(start, end [, step])
    if (strcmp(cname, "List.range") == 0) {
      e->as.call.kind = CALL_LIST_RANGE;
      if (argc != 2 && argc != 3) {
        throw_error(SEMANTIC_ERROR_FUNC_CALL,
                    "Wrong number of arguments in function call");
      }
      for (int i = 0; i < argc; ++i) {
        if (semantic_expr(e->as.call.args[i], table) != TYPE_INT) {
          throw_error(SEMANTIC_ERROR_FUNC_CALL,
                      "Arguments of List.range must be Int");
        }
      }
      if (argc == 3 && e->as.call.args[2]->e_type == EXPR_INT &&
          e->as.call.args[2]->as.int_val == 0) {
        throw_error(SEMANTIC_ERROR_OTHER, "Step of the range can not be zero");
      }
      made = ty_list(prim(TYPE_INT));
      result_type = TYPE_LIST;
      break;
    }

    // Variable (parameter) with a function type, or a list: f(a), lst(i)
    ht_item_t *var = symtable_lookup_var(table, cname);
    if (var) {
      type_info *vt = var->data.full_type;
      if (vt->kind == TYPE_FUNC) {
        e->as.call.kind = CALL_VALUE;
        check_argc(argc, vt->as.func_t.param_count);
        for (int i = 0; i < argc; ++i) {
          semantic_expect(e->as.call.args[i], table,
                          vt->as.func_t.param_types[i]);
          check_arg(e->as.call.args[i], vt->as.func_t.param_types[i]);
        }
        full = vt->as.func_t.ret_type;
        result_type = full->kind;
        break;
      }
      if (vt->kind == TYPE_LIST) {
        e->as.call.kind = CALL_INDEX;
        check_argc(argc, 1);
        semantic_expr(e->as.call.args[0], table);
        check_arg(e->as.call.args[0], prim(TYPE_INT));
        if (ty_has_unknown(vt->as.elem_type)) {
          throw_error(SEMANTIC_ERROR_DATA_TYPE,
                      "Cannot index a list of unknown type");
        }
        full = vt->as.elem_type;
        result_type = full->kind;
        break;
      }
      throw_error(SEMANTIC_ERROR_FUNC_CALL, "Variable is not callable");
    }

    ht_item_t *fn = lookup_func(table, cname);
    if (fn) {
      e->as.call.kind = CALL_FUNC;
      check_argc(argc, fn->data.param_count);
      // Check argument types
      for (int i = 0; i < argc; ++i) {
        semantic_expect(e->as.call.args[i], table, param_type_of(fn, i));
        check_arg(e->as.call.args[i], param_type_of(fn, i));
      }

      result_type = fn->data.type;
      full = fn->data.full_type;
      break;
    }

    // ALGEBDT: constructor of a case class
    ht_item_t *cls = symtable_lookup_case_class(table, cname);
    if (cls) {
      e->as.call.kind = CALL_CTOR;
      check_argc(argc, cls->data.param_count);
      for (int i = 0; i < argc; ++i) {
        semantic_expect(e->as.call.args[i], table,
                        cls->data.param_full_types[i]);
        check_arg(e->as.call.args[i], cls->data.param_full_types[i]);
      }
      full = cls->data.full_type;
      result_type = TYPE_CASE_CLASS;
      break;
    }

    throw_error(SEMANTIC_ERROR_UNDEFINED, "Call to an undeclared function");
    break;
  }

  case EXPR_BOOL:
    result_type = TYPE_BOOL;
    break;

  // CONTROL: if as an expression, both branches give the result
  case EXPR_IF: {
    check_bool(e->as.if_.cond, table, "'if' condition must be type Bool");
    symtable_enter_scope(table);
    semantic_stmts(&e->as.if_.then_b, table, expected);
    symtable_exit_scope(table);
    symtable_enter_scope(table);
    semantic_stmts(&e->as.if_.else_b, table, expected);
    symtable_exit_scope(table);

    bool diverges = false;
    full = branches_value(&e->as.if_.then_b, &e->as.if_.else_b, expected,
                          &diverges);
    result_type = full->kind;
    break;
  }

  // VALMEM, LISTS, ALGEBDT: expr.member
  case EXPR_MEMBER:
    full = semantic_member(e, table);
    result_type = full->kind;
    break;

  // ALGEBDT: match
  case EXPR_MATCH:
    full = semantic_match(e, table, expected);
    result_type = full->kind;
    break;

  // INTERPOLATEDS: values are converted as by toStr
  case EXPR_INTERP:
    for (int i = 0; i < e->as.interp.count; ++i) {
      semantic_expr(e->as.interp.parts[i], table);
    }
    result_type = TYPE_STRING;
    break;

  // LAMBDAS
  case EXPR_LAMBDA:
    made = semantic_lambda(e, table, expected);
    result_type = TYPE_FUNC;
    break;

  // CONTROL: range from to/until to [by step], it behaves as List[Int]
  case EXPR_RANGE:
    if (semantic_expr(e->as.range.from, table) != TYPE_INT ||
        semantic_expr(e->as.range.to, table) != TYPE_INT ||
        (e->as.range.step &&
         semantic_expr(e->as.range.step, table) != TYPE_INT)) {
      throw_error(SEMANTIC_ERROR_DATA_TYPE, "Range bounds must be type Int");
    }
    if (e->as.range.step && e->as.range.step->e_type == EXPR_INT &&
        e->as.range.step->as.int_val == 0) {
      throw_error(SEMANTIC_ERROR_OTHER, "Step of the range can not be zero");
    }
    made = ty_list(prim(TYPE_INT));
    result_type = TYPE_LIST;
    break;

  default:
    result_type = TYPE_UNKNOWN;
    break;
  }

  if (made) {
    e->full_type = made;
  } else {
    e->full_type = full ? ty_own(full) : prim(result_type);
  }
  e->d_type = result_type;
  return result_type;
}