/**
 * IFJ26 Compiler - IFJcode26 generator
 * Author: David Pokorák (xpokord00)
 * Date: 5.11.2026
 **/

#include "generator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtins.h"
#include "codegen.h"
#include "error.h"

/* ---- Generator state ---- */

/* A variable or a temporary of the function being generated (pass 1) */
typedef struct {
  const void *key; /* AST node that declares it */
  int sub;         /* number of the variable among those of the same node */
  char *name;      /* source name or a name of the temporary */
  int id;          /* unique number -> LF@name$id */
} slot_t;

/* A variable that is visible at the current point of the generation */
typedef struct {
  const char *name;
  int slot;
  const type_info *full; /* complete type, may be NULL */
} vis_t;

typedef struct {
  program *prog;

  slot_t *slots; /* all variables and temporaries of the function */
  int nslots, cap_slots;

  vis_t *vis; /* variables visible now (scope stack) */
  int nvis, cap_vis;

  data_type ret_kind;          /* return type of the function being generated */
  const type_info *ret_target; /* its complete type */
  type_info ret_tmp;           /* used when only the kind is known */

  int var_counter;

  const expr **lambdas; /* lambdas found so far, generated after the function */
  int nlambdas;
  int done_lambdas;
} gen_t;

/* ---- Variables ---- */

/**
 * Pass 1: records a variable of the function being generated. The variable
 * gets a unique id, so it is emitted as LF@name$id.
 *
 * @param key  AST node that declares the variable
 * @param sub  number of the variable among those declared by the same node
 * @param name source name of the variable (or a name of a temporary)
 **/
static void add_slot(gen_t *g, const void *key, int sub, const char *name) {
  if (g->nslots == g->cap_slots) {
    if (g->cap_slots == 0) {
      g->cap_slots = 16;
    } else {
      g->cap_slots *= 2;
    }
    g->slots = ast_xrealloc(g->slots, (size_t)g->cap_slots * sizeof(slot_t));
  }
  g->slots[g->nslots].key = key;
  g->slots[g->nslots].sub = sub;
  g->slots[g->nslots].name = ast_xstrdup(name);
  g->slots[g->nslots].id = ++g->var_counter;
  g->nslots++;
}

/**
 * Pass 2: finds the variable made by pass 1 for the given node.
 *
 * @return index of the slot
 **/
static int find_slot(const gen_t *g, const void *key, int sub) {
  for (int i = 0; i < g->nslots; i++) {
    if (g->slots[i].key == key && g->slots[i].sub == sub) {
      return i;
    }
  }
  throw_error(INTERNAL_ERROR, "Generator: variable collection out of sync.");
}

/**
 * Prints the IFJcode26 name of a slot (LF@name$id).
 **/
static void print_slot(const gen_t *g, int slot) {
  codegen_var(g->slots[slot].name, g->slots[slot].id);
}

/**
 * Pass 2: a declaration was reached, so the variable becomes visible.
 *
 * @param key  AST node that declares the variable
 * @param sub  number of the variable among those declared by the same node
 * @param full complete type of the variable
 * @return     index of the slot of the variable
 **/
static int declare_var(gen_t *g, const void *key, int sub,
                       const type_info *full) {
  int slot = find_slot(g, key, sub);
  if (g->nvis == g->cap_vis) {
    if (g->cap_vis == 0) {
      g->cap_vis = 16;
    } else {
      g->cap_vis *= 2;
    }
    g->vis = ast_xrealloc(g->vis, (size_t)g->cap_vis * sizeof(vis_t));
  }
  g->vis[g->nvis].name = g->slots[slot].name;
  g->vis[g->nvis].slot = slot;
  g->vis[g->nvis].full = full;
  g->nvis++;
  return slot;
}

/**
 * Looks up a visible variable by name, starting from the most recently
 * declared one to respect nested scopes.
 *
 * @return the variable or NULL (the name may be a function)
 **/
static const vis_t *lookup_var(const gen_t *g, const char *name) {
  for (int i = g->nvis - 1; i >= 0; i--) {
    if (strcmp(g->vis[i].name, name) == 0) {
      return &g->vis[i];
    }
  }
  return NULL;
}

/* ---- Functions and types ---- */

typedef struct {
  const char *name;
  int nparams;
  data_type params[3];
} builtin_sig;

static const builtin_sig builtin_sigs[] = {
    {"print", 1, {TYPE_UNKNOWN}},
    {"readLine", 0, {0}},
    {"readInt", 0, {0}},
    {"readDouble", 0, {0}},
    {"readBoolean", 0, {0}},
    {"readIntMaybe", 0, {0}},
    {"readDoubleMaybe", 0, {0}},
    {"toStr", 1, {TYPE_UNKNOWN}},
    {"toInt", 1, {TYPE_DOUBLE}},
    {"strLength", 1, {TYPE_STRING}},
    {"subStr", 3, {TYPE_STRING, TYPE_INT, TYPE_INT}},
    {"ord", 2, {TYPE_STRING, TYPE_INT}},
    {"chr", 1, {TYPE_INT}},
};

/**
 * Looks up a built-in function by name.
 *
 * @return its signature or NULL if there is no such built-in
 **/
static const builtin_sig *find_builtin(const char *name) {
  for (size_t i = 0; i < sizeof builtin_sigs / sizeof builtin_sigs[0]; i++) {
    if (strcmp(builtin_sigs[i].name, name) == 0) {
      return &builtin_sigs[i];
    }
  }
  return NULL;
}

/**
 * Looks up a user-defined function by name.
 *
 * @return its definition or NULL if the program has no such function
 **/
static const func_def *find_func(const gen_t *g, const char *name) {
  for (int i = 0; i < g->prog->count; i++) {
    if (strcmp(g->prog->funcs[i]->name, name) == 0) {
      return g->prog->funcs[i];
    }
  }
  return NULL;
}

/**
 * Looks up the definition of a case class by name.
 *
 * @return the definition or NULL if there is no such class
 **/
static const type_def *find_class(const gen_t *g, const char *name) {
  for (int i = 0; i < g->prog->type_count; i++) {
    if (g->prog->types[i].is_class &&
        strcmp(g->prog->types[i].name, name) == 0) {
      return &g->prog->types[i];
    }
  }
  return NULL;
}

/**
 * Returns the type of an expression. Missing type is an internal error.
 **/
static data_type type_of_expr(const expr *e) {
  if (e->d_type == TYPE_UNKNOWN) {
    throw_error(INTERNAL_ERROR, "Generator: expression without a type ");
  }
  return e->d_type;
}

/**
 * Does an Int have to be converted to Double to be stored as the given type?
 * Yes for Double and for a union that has Double but not Int as a member.
 **/
static bool wants_float(const type_info *target) {
  if (target == NULL) {
    return false;
  }
  if (target->kind == TYPE_DOUBLE) {
    return true;
  }
  if (target->kind != TYPE_UNION) {
    return false;
  }
  bool has_double = false;
  bool has_int = false;
  for (int i = 0; i < target->as.union_t.count; i++) {
    if (target->as.union_t.members[i]->kind == TYPE_DOUBLE) {
      has_double = true;
    }
    if (target->as.union_t.members[i]->kind == TYPE_INT) {
      has_int = true;
    }
  }
  return has_double && !has_int;
}

/**
 * Type of a parameter or an attribute for the conversion of the argument.
 * Uses the complete type if there is one, otherwise only the kind.
 **/
static const type_info *param_target(const param *p, type_info *tmp) {
  if (p->full_type != NULL) {
    return p->full_type;
  }
  tmp->kind = p->type;
  return tmp;
}

/**
 * Prints the type name that the instruction TYPE gives for a value of the
 * given kind.
 **/
static const char *runtime_type_name(data_type kind) {
  switch (kind) {
  case TYPE_INT:
    return "int";
  case TYPE_DOUBLE:
    return "float";
  case TYPE_STRING:
    return "string";
  case TYPE_BOOL:
    return "bool";
  case TYPE_UNIT:
    return "nil";
  default:
    return "";
  }
}

/* ---- Expressions ---- */

static void gen_expr(gen_t *g, const expr *e);
static void gen_stmt(gen_t *g, const stmt *s);
static void gen_value_block(gen_t *g, const block *b, const type_info *target);

/**
 * Generates an expression and converts Int to Double if the surrounding
 * construct needs it.
 *
 * @param target complete type the surrounding construct expects (NULL = any)
 **/
static void gen_expr_to(gen_t *g, const expr *e, const type_info *target) {
  gen_expr(g, e);
  if (type_of_expr(e) == TYPE_INT && wants_float(target)) {
    printf("INT2FLOATS\n");
  }
}

/**
 * Generates an expression and converts Int to Double if the kind is Double.
 **/
static void gen_expr_as(gen_t *g, const expr *e, data_type kind) {
  type_info target = {.kind = kind};
  gen_expr_to(g, e, &target);
}

/**
 * Prints a constant of a literal (used for the patterns of match).
 *
 * @param as_float print an Int literal as a float constant
 **/
static void print_const(const expr *e, bool as_float) {
  switch (e->e_type) {
  case EXPR_INT:
    if (as_float) {
      codegen_float_const((double)e->as.int_val);
    } else {
      codegen_int_const(e->as.int_val);
    }
    break;
  case EXPR_DOUBLE:
    codegen_float_const(e->as.double_val);
    break;
  case EXPR_STRING:
    codegen_string_const(e->as.str_val);
    break;
  case EXPR_BOOL:
    codegen_bool_const(e->as.bool_val);
    break;
  case EXPR_UNIT:
    printf("nil@nil");
    break;
  default:
    throw_error(INTERNAL_ERROR, "Generator: pattern is not a literal.");
  }
}

/**
 * A lambda may return an Int where a Double was declared. The value that
 * comes back from a call of a function value is fixed to a Double.
 **/
static void gen_fix_double(void) {
  int id = codegen_new_label();
  printf("POPS GF@$t1\n");
  printf("TYPE GF@$t2 GF@$t1\n");
  printf("JUMPIFNEQ $dbl%d$end GF@$t2 string@int\n", id);
  printf("INT2FLOAT GF@$t1 GF@$t1\n");
  printf("LABEL $dbl%d$end\n", id);
  printf("PUSHS GF@$t1\n");
}

/**
 * Generates a call of a user-defined or built-in function: arguments left to
 * right, then CALL. The result is left on the stack.
 **/
static void gen_call_func(gen_t *g, const expr *e) {
  const func_def *f = find_func(g, e->as.call.name);
  const builtin_sig *b = NULL;
  if (f == NULL) {
    b = find_builtin(e->as.call.name);
  }
  if (f == NULL && b == NULL) {
    throw_error(INTERNAL_ERROR, "Generator: unknown function");
  }

  for (int i = 0; i < e->as.call.argc; i++) {
    type_info tmp = {.kind = TYPE_UNKNOWN};
    const type_info *target = &tmp;
    if (f != NULL && i < f->param_count) {
      target = param_target(&f->params[i], &tmp);
    } else if (b != NULL && i < b->nparams) {
      tmp.kind = b->params[i];
    }
    gen_expr_to(g, e->as.call.args[i], target);
  }
  codegen_call(e->as.call.name);
}

/**
 * LAMBDAS: calls a function stored in a variable or a parameter. The
 * arguments are converted according to the type of the variable.
 **/
static void gen_call_value(gen_t *g, const expr *e) {
  const vis_t *v = lookup_var(g, e->as.call.name);
  if (v == NULL) {
    throw_error(INTERNAL_ERROR, "Generator: undefined function variable.");
  }
  for (int i = 0; i < e->as.call.argc; i++) {
    const type_info *target = NULL;
    if (v->full != NULL && v->full->kind == TYPE_FUNC &&
        i < v->full->as.func_t.param_count) {
      target = v->full->as.func_t.param_types[i];
    }
    gen_expr_to(g, e->as.call.args[i], target);
  }
  printf("CALL ");
  print_slot(g, v->slot);
  printf("\n");
  if (e->d_type == TYPE_DOUBLE) {
    gen_fix_double();
  }
}

/**
 * LISTS: lst(i), the item at an index (runtime error 40 outside the list).
 **/
static void gen_call_index(gen_t *g, const expr *e) {
  const vis_t *v = lookup_var(g, e->as.call.name);
  if (v == NULL) {
    throw_error(INTERNAL_ERROR, "Generator: undefined list variable.");
  }
  printf("PUSHS ");
  print_slot(g, v->slot);
  printf("\n");
  gen_expr(g, e->as.call.args[0]);
  codegen_call("List$apply");
}

/**
 * ALGEBDT: constructor of a case class. The attributes are popped from the
 * stack into a new array [class name, attribute1, ...].
 **/
static void gen_call_ctor(gen_t *g, const expr *e) {
  const type_def *cls = find_class(g, e->as.call.name);
  if (cls == NULL) {
    throw_error(INTERNAL_ERROR, "Generator: unknown case class.");
  }
  int n = e->as.call.argc;
  for (int i = 0; i < n; i++) {
    type_info tmp = {.kind = TYPE_UNKNOWN};
    gen_expr_to(g, e->as.call.args[i], param_target(&cls->fields[i], &tmp));
  }
  printf("NEWARRAY GF@$arr int@%d\n", n + 1);
  printf("ARRAYSET GF@$arr int@0 ");
  codegen_string_const(cls->name);
  printf("\n");
  for (int i = n; i >= 1; i--) {
    printf("POPS GF@$t1\n");
    printf("ARRAYSET GF@$arr int@%d GF@$t1\n", i);
  }
  printf("PUSHS GF@$arr\n");
}

/**
 * LISTS: List(a, b, ...). The items are pushed left to right followed by
 * their count, List$make builds the array.
 **/
static void gen_call_list_new(gen_t *g, const expr *e) {
  const type_info *elem = NULL;
  if (e->full_type != NULL && e->full_type->kind == TYPE_LIST) {
    elem = e->full_type->as.elem_type;
  }
  for (int i = 0; i < e->as.call.argc; i++) {
    gen_expr_to(g, e->as.call.args[i], elem);
  }
  printf("PUSHS int@%d\n", e->as.call.argc);
  codegen_call("List$make");
}

/**
 * LISTS: List.range(start, end [, step]); a missing step is 1.
 **/
static void gen_call_list_range(gen_t *g, const expr *e) {
  for (int i = 0; i < e->as.call.argc; i++) {
    gen_expr(g, e->as.call.args[i]);
  }
  if (e->as.call.argc == 2) {
    printf("PUSHS int@1\n");
  }
  codegen_call("List$range");
}

/**
 * Generates a call. The semantic analysis says what kind of call it is.
 **/
static void gen_call(gen_t *g, const expr *e) {
  switch (e->as.call.kind) {
  case CALL_FUNC:
    gen_call_func(g, e);
    break;
  case CALL_VALUE:
    gen_call_value(g, e);
    break;
  case CALL_INDEX:
    gen_call_index(g, e);
    break;
  case CALL_CTOR:
    gen_call_ctor(g, e);
    break;
  case CALL_LIST_NEW:
    gen_call_list_new(g, e);
    break;
  case CALL_LIST_RANGE:
    gen_call_list_range(g, e);
    break;
  }
}

/**
 * VALMEM, LISTS, ALGEBDT: generates obj.member, the members are calls of the
 * built-in functions with the object as the first argument.
 **/
static void gen_member(gen_t *g, const expr *e) {
  gen_expr(g, e->as.member.obj);
  for (int i = 0; i < e->as.member.argc; i++) {
    gen_expr(g, e->as.member.args[i]);
  }

  switch (e->as.member.kind) {
  case MEMBER_TO_STRING:
    codegen_call("toStr");
    break;
  case MEMBER_TO_INT:
    codegen_call("toInt");
    break;
  case MEMBER_FLOOR:
    codegen_call("Double$floor");
    break;
  case MEMBER_LENGTH:
    codegen_call("strLength");
    break;
  case MEMBER_FIELD:
    printf("POPS GF@$t1\n");
    printf("ARRAYGET GF@$t1 GF@$t1 int@%d\n", e->as.member.field_index + 1);
    printf("PUSHS GF@$t1\n");
    break;
  case MEMBER_LIST_TO_LIST: /* the range is already a list */
    break;
  case MEMBER_LIST_DROP:
    codegen_call("List$drop");
    break;
  case MEMBER_LIST_TAKE:
    codegen_call("List$take");
    break;
  case MEMBER_LIST_SLICE:
    codegen_call("List$slice");
    break;
  case MEMBER_LIST_HEAD:
    codegen_call("List$head");
    break;
  case MEMBER_LIST_TAIL:
    codegen_call("List$tail");
    break;
  case MEMBER_LIST_FILTER:
    codegen_call("List$filter");
    break;
  case MEMBER_LIST_TAKE_WHILE:
    codegen_call("List$takeWhile");
    break;
  case MEMBER_LIST_DROP_WHILE:
    codegen_call("List$dropWhile");
    break;
  }
}

/**
 * INTERPOLATEDS: every part is converted to a String as by toStr and the
 * parts are concatenated.
 **/
static void gen_interp(gen_t *g, const expr *e) {
  if (e->as.interp.count == 0) {
    printf("PUSHS string@\n");
    return;
  }
  for (int i = 0; i < e->as.interp.count; i++) {
    const expr *part = e->as.interp.parts[i];
    gen_expr(g, part);
    if (type_of_expr(part) != TYPE_STRING) {
      codegen_call("toStr");
    }
    if (i > 0) {
      printf("POPS GF@$t2\n");
      printf("POPS GF@$t1\n");
      printf("CONCAT GF@$t1 GF@$t1 GF@$t2\n");
      printf("PUSHS GF@$t1\n");
    }
  }
}

/**
 * CONTROL, LISTS: a range used as a value is the list of its items. The
 * range is made by List$range (until) or List$rangeTo (to).
 **/
static void gen_range_list(gen_t *g, const expr *e) {
  gen_expr(g, e->as.range.from);
  gen_expr(g, e->as.range.to);
  if (e->as.range.step != NULL) {
    gen_expr(g, e->as.range.step);
  } else {
    printf("PUSHS int@1\n");
  }
  if (e->as.range.inclusive) {
    codegen_call("List$rangeTo");
  } else {
    codegen_call("List$range");
  }
}

/**
 * LAMBDAS: a lambda is remembered and generated as a function after the
 * current function; its value is the label of that function.
 **/
static void gen_lambda_ref(gen_t *g, const expr *e) {
  int id = g->nlambdas;
  APPEND(g->lambdas, g->nlambdas, e);
  printf("PUSHS string@$lambda$%d\n", id);
}

/**
 * Generates a && or ||
 * the right operand is evaluated only if the left one does not decide the
 * result. Leaves boolean on the stack.
 **/
static void gen_logical(gen_t *g, const expr *e) {
  int id = codegen_new_label();
  const char *name = "or";
  const char *decided = "true"; /* a true left operand decides ||  */
  if (e->as.binary.op == OP_AND) {
    name = "and";
    decided = "false"; /* a false left operand decides && */
  }

  gen_expr(g, e->as.binary.left);
  printf("PUSHS bool@%s\n", decided);
  printf("JUMPIFEQS $%s%d$decided\n", name, id);
  gen_expr(g, e->as.binary.right);
  printf("JUMP $%s%d$end\n", name, id);
  printf("LABEL $%s%d$decided\n", name, id);
  printf("PUSHS bool@%s\n", decided);
  printf("LABEL $%s%d$end\n", name, id);
}

/**
 * ALGEBDT: == and != when an operand is a union. The EQ instruction ends with
 * error 53 for values of different types, so the types are compared first:
 * different types are never equal (except Int and Double, which are compared
 * as Doubles). The operands are on the stack.
 *
 * @param negate generate != instead of ==
 **/
static void gen_union_equality(bool negate) {
  int id = codegen_new_label();
  const char *diff = "false"; /* different types: == is false, != is true */
  if (negate) {
    diff = "true";
  }
  printf("POPS GF@$t2\n");
  printf("POPS GF@$t1\n");
  printf("TYPE GF@$t3 GF@$t1\n");
  printf("TYPE GF@$t4 GF@$t2\n");
  printf("JUMPIFEQ $eq%d$same GF@$t3 GF@$t4\n", id);
  printf("JUMPIFNEQ $eq%d$chk GF@$t3 string@int\n", id);
  printf("JUMPIFNEQ $eq%d$diff GF@$t4 string@float\n", id);
  printf("INT2FLOAT GF@$t1 GF@$t1\n");
  printf("JUMP $eq%d$same\n", id);
  printf("LABEL $eq%d$chk\n", id);
  printf("JUMPIFNEQ $eq%d$diff GF@$t3 string@float\n", id);
  printf("JUMPIFNEQ $eq%d$diff GF@$t4 string@int\n", id);
  printf("INT2FLOAT GF@$t2 GF@$t2\n");
  printf("LABEL $eq%d$same\n", id);
  printf("PUSHS GF@$t1\n");
  printf("PUSHS GF@$t2\n");
  printf("EQS\n");
  if (negate) {
    printf("NOTS\n");
  }
  printf("JUMP $eq%d$end\n", id);
  printf("LABEL $eq%d$diff\n", id);
  printf("PUSHS bool@%s\n", diff);
  printf("LABEL $eq%d$end\n", id);
}

/**
 * Generates a binary operation. Both operands are first converted to a common
 * type. The result is left on the stack.
 * String "+" is done by CONCAT, <=, >=, != are built as
 * a negation of the opposite comparison, ++ joins two lists.
 **/
static void gen_binary(gen_t *g, const expr *e) {
  if (e->as.binary.op == OP_AND || e->as.binary.op == OP_OR) {
    gen_logical(g, e);
    return;
  }
  if (e->as.binary.op == OP_CONCAT) {
    gen_expr(g, e->as.binary.left);
    gen_expr(g, e->as.binary.right);
    codegen_call("List$concat");
    return;
  }
  data_type lt = type_of_expr(e->as.binary.left);
  data_type rt = type_of_expr(e->as.binary.right);
  data_type common = lt;
  if (lt == TYPE_DOUBLE || rt == TYPE_DOUBLE) {
    common = TYPE_DOUBLE;
  }

  gen_expr_as(g, e->as.binary.left, common);
  gen_expr_as(g, e->as.binary.right, common);

  bool equality = e->as.binary.op == OP_EQ || e->as.binary.op == OP_NE;
  if (equality && (lt == TYPE_UNION || rt == TYPE_UNION)) {
    gen_union_equality(e->as.binary.op == OP_NE);
    return;
  }

  switch (e->as.binary.op) {
  case OP_ADD:
    if (common == TYPE_STRING) {
      printf("POPS GF@$t2\n");
      printf("POPS GF@$t1\n");
      printf("CONCAT GF@$t1 GF@$t1 GF@$t2\n");
      printf("PUSHS GF@$t1\n");
    } else {
      printf("ADDS\n");
    }
    break;
  case OP_SUB:
    printf("SUBS\n");
    break;
  case OP_MUL:
    printf("MULS\n");
    break;
  case OP_DIV:
    if (common == TYPE_DOUBLE) {
      printf("DIVS\n");
    } else {
      printf("IDIVS\n");
    }
    break;
  case OP_LT:
    printf("LTS\n");
    break;
  case OP_GT:
    printf("GTS\n");
    break;
  case OP_EQ:
    printf("EQS\n");
    break;
  case OP_LE:
    printf("GTS\nNOTS\n");
    break;
  case OP_GE:
    printf("LTS\nNOTS\n");
    break;
  case OP_NE:
    printf("EQS\nNOTS\n");
    break;
  case OP_NEG:
  case OP_NOT:
  case OP_AND:
  case OP_OR:
  case OP_CONCAT:
    break; /* handled above */
  }
}

/**
 * CONTROL: if used as an expression. Both branches are blocks that leave
 * their value on the stack, converted to the type of the whole expression.
 **/
static void gen_if_value(gen_t *g, const expr *cond, const block *then_b,
                         const block *else_b, const type_info *target) {
  int id = codegen_new_label();
  gen_expr(g, cond);
  printf("PUSHS bool@false\n");
  printf("JUMPIFEQS $if%d$else\n", id);
  gen_value_block(g, then_b, target);
  printf("JUMP $if%d$end\n", id);
  printf("LABEL $if%d$else\n", id);
  gen_value_block(g, else_b, target);
  printf("LABEL $if%d$end\n", id);
}

/**
 * ALGEBDT: jumps to the label fail if the value in the slot subj is not of
 * the type t. The type is decided at runtime from the value.
 **/
static void gen_type_test(const gen_t *g, int subj, const type_info *t,
                          const char *fail) {
  switch (t->kind) {
  case TYPE_INT:
  case TYPE_DOUBLE:
  case TYPE_STRING:
  case TYPE_BOOL:
  case TYPE_UNIT:
    printf("TYPE GF@$t1 ");
    print_slot(g, subj);
    printf("\nJUMPIFNEQ %s GF@$t1 string@%s\n", fail,
           runtime_type_name(t->kind));
    break;
  case TYPE_CASE_CLASS:
    /* an array whose first item is the name of the class */
    printf("TYPE GF@$t1 ");
    print_slot(g, subj);
    printf("\nJUMPIFNEQ %s GF@$t1 string@array\n", fail);
    printf("ARRAYGET GF@$t1 ");
    print_slot(g, subj);
    printf(" int@0\n");
    printf("TYPE GF@$t2 GF@$t1\n");
    printf("JUMPIFNEQ %s GF@$t2 string@string\n", fail);
    printf("JUMPIFNEQ %s GF@$t1 ", fail);
    codegen_string_const(t->as.class_name);
    printf("\n");
    break;
  case TYPE_LIST:
    /* an array whose first item is the length */
    printf("TYPE GF@$t1 ");
    print_slot(g, subj);
    printf("\nJUMPIFNEQ %s GF@$t1 string@array\n", fail);
    printf("ARRAYGET GF@$t1 ");
    print_slot(g, subj);
    printf(" int@0\n");
    printf("TYPE GF@$t2 GF@$t1\n");
    printf("JUMPIFNEQ %s GF@$t2 string@int\n", fail);
    break;
  default:
    break;
  }
}

/**
 * ALGEBDT: generates the test and the bindings of one branch of a match. If
 * the pattern does not fit, the code jumps to the label fail.
 *
 * @param subj slot with the matched value
 * @param st   static type of the matched value
 **/
static void gen_pattern(gen_t *g, const match_case *c, int subj,
                        const type_info *st, const char *fail) {
  switch (c->pat) {
  case PAT_WILDCARD:
    break;
  case PAT_LITERAL: {
    bool as_float =
        st != NULL && st->kind == TYPE_DOUBLE && c->literal->e_type == EXPR_INT;
    data_type kind = type_of_expr(c->literal);
    if (as_float) {
      kind = TYPE_DOUBLE;
    }
    printf("TYPE GF@$t1 ");
    print_slot(g, subj);
    printf("\nJUMPIFNEQ %s GF@$t1 string@%s\n", fail, runtime_type_name(kind));
    printf("JUMPIFNEQ %s ", fail);
    print_slot(g, subj);
    printf(" ");
    print_const(c->literal, as_float);
    printf("\n");
    break;
  }
  case PAT_TYPED: {
    gen_type_test(g, subj, c->bind_type, fail);
    int slot = declare_var(g, c, 0, c->bind_type);
    printf("MOVE ");
    print_slot(g, slot);
    printf(" ");
    print_slot(g, subj);
    printf("\n");
    break;
  }
  case PAT_CTOR: {
    const type_def *cls = find_class(g, c->class_name);
    if (cls == NULL) {
      throw_error(INTERNAL_ERROR, "Generator: unknown case class.");
    }
    type_info class_type = {.kind = TYPE_CASE_CLASS};
    class_type.as.class_name = c->class_name;
    gen_type_test(g, subj, &class_type, fail);
    for (int i = 0; i < c->field_count; i++) {
      if (strcmp(c->fields[i], "_") == 0) {
        continue;
      }
      type_info tmp = {.kind = TYPE_UNKNOWN};
      int slot = declare_var(g, c, 1 + i, param_target(&cls->fields[i], &tmp));
      printf("ARRAYGET ");
      print_slot(g, slot);
      printf(" ");
      print_slot(g, subj);
      printf(" int@%d\n", i + 1);
    }
    break;
  }
  }
}

/**
 * ALGEBDT: generates match. The matched value is evaluated once, the branches
 * are tried in order; the value of the first one that fits (and whose guard
 * is true) is the result. No branch fitting is a runtime error 40.
 **/
static void gen_match(gen_t *g, const expr *e) {
  int id = codegen_new_label();
  int subj = find_slot(g, e, 0);
  const type_info *st = e->as.match_.subject->full_type;

  gen_expr(g, e->as.match_.subject);
  printf("POPS ");
  print_slot(g, subj);
  printf("\n");

  for (int i = 0; i < e->as.match_.case_count; i++) {
    const match_case *c = &e->as.match_.cases[i];
    char next[64];
    snprintf(next, sizeof next, "$match%d$next%d", id, i);
    int mark = g->nvis;

    gen_pattern(g, c, subj, st, next);
    if (c->guard != NULL) {
      gen_expr(g, c->guard);
      printf("PUSHS bool@false\n");
      printf("JUMPIFEQS %s\n", next);
    }
    gen_value_block(g, &c->body, e->full_type);
    printf("JUMP $match%d$end\n", id);
    printf("LABEL %s\n", next);
    g->nvis = mark;
  }
  printf("EXIT int@40\n");
  printf("LABEL $match%d$end\n", id);
}

/**
 * Generates code that leaves the value of expression e on top of the stack.
 **/
static void gen_expr(gen_t *g, const expr *e) {
  switch (e->e_type) {
  case EXPR_INT:
  case EXPR_DOUBLE:
  case EXPR_STRING:
  case EXPR_BOOL:
  case EXPR_UNIT:
    printf("PUSHS ");
    print_const(e, false);
    printf("\n");
    break;
  case EXPR_VAR: {
    const vis_t *v = lookup_var(g, e->as.name);
    if (v != NULL) {
      printf("PUSHS ");
      print_slot(g, v->slot);
      printf("\n");
    } else if (find_func(g, e->as.name) != NULL ||
               find_builtin(e->as.name) != NULL) {
      /* LAMBDAS: a function used as a value is its label */
      printf("PUSHS string@$%s\n", e->as.name);
    } else {
      throw_error(
          INTERNAL_ERROR,
          "Generator: undefined variable (semantic analysis missing?).");
    }
    break;
  }
  case EXPR_UNARY: /* unary minus: x * -1, or negation */
    gen_expr(g, e->as.unary.expr);
    if (e->as.unary.op == OP_NOT) {
      printf("NOTS\n");
      break;
    }
    printf("PUSHS ");
    if (type_of_expr(e) == TYPE_DOUBLE) {
      codegen_float_const(-1.0);
    } else {
      codegen_int_const(-1);
    }
    printf("\nMULS\n");
    break;
  case EXPR_BINARY:
    gen_binary(g, e);
    break;
  case EXPR_CALL:
    gen_call(g, e);
    break;
  case EXPR_IF:
    gen_if_value(g, e->as.if_.cond, &e->as.if_.then_b, &e->as.if_.else_b,
                 e->full_type);
    break;
  case EXPR_LAMBDA:
    gen_lambda_ref(g, e);
    break;
  case EXPR_MEMBER:
    gen_member(g, e);
    break;
  case EXPR_MATCH:
    gen_match(g, e);
    break;
  case EXPR_INTERP:
    gen_interp(g, e);
    break;
  case EXPR_RANGE:
    gen_range_list(g, e);
    break;
  }
}

/* ---- Statements ---- */

static void gen_block(gen_t *g, const block *b);

/**
 * Pushes the result of a function (used by return). A Unit
 * function evaluates and drops the value and returns nil@nil; other
 * functions convert the value to the declared return type.
 *
 * @param e returned expression, NULL for a bare return
 **/
static void gen_result(gen_t *g, const expr *e) {
  if (g->ret_kind == TYPE_UNIT) {
    if (e != NULL) {
      gen_expr(g, e);
      printf("POPS GF@$discard\n");
    }
    printf("PUSHS nil@nil\n");
  } else {
    gen_expr_to(g, e, g->ret_target);
  }
}

/**
 * Generates the last statement of a block that has a value: an expression
 * gives its value, an if with else gives the value of the branch, a return
 * leaves the function; any other statement gives Unit.
 **/
static void gen_tail(gen_t *g, const stmt *s, const type_info *target) {
  switch (s->type) {
  case STMT_EXPR:
    gen_expr_to(g, s->as.expr.expr, target);
    break;
  case STMT_RETURN:
    gen_stmt(g, s);
    break;
  case STMT_IF:
    if (s->as.if_.else_b.count > 0) {
      gen_if_value(g, s->as.if_.cond, &s->as.if_.then_b, &s->as.if_.else_b,
                   target);
      break;
    }
    gen_stmt(g, s);
    printf("PUSHS nil@nil\n");
    break;
  default:
    gen_stmt(g, s);
    printf("PUSHS nil@nil\n");
    break;
  }
}

/**
 * Generates a block whose value (the value of its last statement) is left on
 * the stack, converted to the target type. The block has its own scope.
 **/
static void gen_value_block(gen_t *g, const block *b, const type_info *target) {
  int mark = g->nvis;
  if (b->count == 0) {
    printf("PUSHS nil@nil\n");
  }
  for (int i = 0; i < b->count; i++) {
    if (i == b->count - 1) {
      gen_tail(g, b->items[i], target);
    } else {
      gen_stmt(g, b->items[i]);
    }
  }
  g->nvis = mark;
}

/**
 * CONTROL: for over a range. The bounds and the step are evaluated once, the
 * loop counts in a hidden variable, so the body cannot change it.
 **/
static void gen_for_range(gen_t *g, const stmt *s, int id, int var) {
  const expr *range = s->as.for_.source;
  int cnt = find_slot(g, s, 1);
  int end = find_slot(g, s, 2);
  int step = find_slot(g, s, 3);
  int tmp = find_slot(g, s, 4);
  const char *cmp_up = "LT";
  const char *cmp_down = "GT";
  const char *exit_up = "bool@false";
  const char *exit_down = "bool@false";
  if (range->as.range.inclusive) {
    cmp_up = "GT";
    cmp_down = "LT";
    exit_up = "bool@true";
    exit_down = "bool@true";
  }

  gen_expr(g, range->as.range.from);
  printf("POPS ");
  print_slot(g, cnt);
  printf("\n");
  gen_expr(g, range->as.range.to);
  printf("POPS ");
  print_slot(g, end);
  printf("\n");
  if (range->as.range.step != NULL) {
    gen_expr(g, range->as.range.step);
  } else {
    printf("PUSHS int@1\n");
  }
  printf("POPS ");
  print_slot(g, step);
  printf("\n");

  bool check_zero =
      range->as.range.step != NULL && range->as.range.step->e_type != EXPR_INT;
  if (check_zero) {
    printf("JUMPIFEQ $for%d$zero ", id);
    print_slot(g, step);
    printf(" int@0\n");
  }

  printf("LABEL $for%d$start\n", id);
  printf("GT ");
  print_slot(g, tmp);
  printf(" ");
  print_slot(g, step);
  printf(" int@0\n");
  printf("JUMPIFEQ $for%d$down ", id);
  print_slot(g, tmp);
  printf(" bool@false\n");
  /* positive step */
  printf("%s ", cmp_up);
  print_slot(g, tmp);
  printf(" ");
  print_slot(g, cnt);
  printf(" ");
  print_slot(g, end);
  printf("\nJUMPIFEQ $for%d$exit ", id);
  print_slot(g, tmp);
  printf(" %s\n", exit_up);
  printf("JUMP $for%d$body\n", id);
  /* negative step */
  printf("LABEL $for%d$down\n", id);
  printf("%s ", cmp_down);
  print_slot(g, tmp);
  printf(" ");
  print_slot(g, cnt);
  printf(" ");
  print_slot(g, end);
  printf("\nJUMPIFEQ $for%d$exit ", id);
  print_slot(g, tmp);
  printf(" %s\n", exit_down);

  printf("LABEL $for%d$body\n", id);
  printf("MOVE ");
  print_slot(g, var);
  printf(" ");
  print_slot(g, cnt);
  printf("\n");
}

/**
 * CONTROL: for over a list. The list is evaluated once.
 **/
static void gen_for_list(gen_t *g, const stmt *s, int id, int var) {
  int lst = find_slot(g, s, 1);
  int len = find_slot(g, s, 2);
  int idx = find_slot(g, s, 3);
  int tmp = find_slot(g, s, 4);

  gen_expr(g, s->as.for_.source);
  printf("POPS ");
  print_slot(g, lst);
  printf("\nARRAYGET ");
  print_slot(g, len);
  printf(" ");
  print_slot(g, lst);
  printf(" int@0\nMOVE ");
  print_slot(g, idx);
  printf(" int@1\n");

  printf("LABEL $for%d$start\n", id);
  printf("GT ");
  print_slot(g, tmp);
  printf(" ");
  print_slot(g, idx);
  printf(" ");
  print_slot(g, len);
  printf("\nJUMPIFEQ $for%d$exit ", id);
  print_slot(g, tmp);
  printf(" bool@true\n");
  printf("ARRAYGET ");
  print_slot(g, var);
  printf(" ");
  print_slot(g, lst);
  printf(" ");
  print_slot(g, idx);
  printf("\n");
}

/**
 * CONTROL: generates a for loop with one generator and any number of guards.
 * A guard that is false skips to the next item.
 **/
static void gen_for(gen_t *g, const stmt *s) {
  int id = codegen_new_label();
  const expr *source = s->as.for_.source;
  int mark = g->nvis;

  type_info int_type = {.kind = TYPE_INT};
  const type_info *var_type = &int_type;
  bool is_range = source->e_type == EXPR_RANGE;
  if (!is_range && source->full_type != NULL &&
      source->full_type->kind == TYPE_LIST) {
    var_type = source->full_type->as.elem_type;
  }

  /* the source is evaluated before the control variable becomes visible */
  int var = find_slot(g, s, 0);
  if (is_range) {
    gen_for_range(g, s, id, var);
  } else {
    gen_for_list(g, s, id, var);
  }
  declare_var(g, s, 0, var_type);

  for (int i = 0; i < s->as.for_.guard_count; i++) {
    gen_expr(g, s->as.for_.guards[i]);
    printf("PUSHS bool@false\n");
    printf("JUMPIFEQS $for%d$next\n", id);
  }
  gen_block(g, &s->as.for_.body);
  printf("LABEL $for%d$next\n", id);
  if (is_range) {
    int cnt = find_slot(g, s, 1);
    int step = find_slot(g, s, 3);
    printf("ADD ");
    print_slot(g, cnt);
    printf(" ");
    print_slot(g, cnt);
    printf(" ");
    print_slot(g, step);
    printf("\n");
  } else {
    int idx = find_slot(g, s, 3);
    printf("ADD ");
    print_slot(g, idx);
    printf(" ");
    print_slot(g, idx);
    printf(" int@1\n");
  }
  printf("JUMP $for%d$start\n", id);
  printf("LABEL $for%d$exit\n", id);

  bool check_zero = is_range && source->as.range.step != NULL &&
                    source->as.range.step->e_type != EXPR_INT;
  if (check_zero) {
    printf("JUMP $for%d$done\n", id);
    printf("LABEL $for%d$zero\n", id);
    printf("EXIT int@40\n");
    printf("LABEL $for%d$done\n", id);
  }
  g->nvis = mark;
}

/**
 * Generates one statement: val/var, assignment, expression statement, return,
 * if, while or for.
 **/
static void gen_stmt(gen_t *g, const stmt *s) {
  switch (s->type) {
  case STMT_VAL:
  case STMT_VAR: {
    /* the initializer is evaluated BEFORE the new name becomes visible */
    type_info tmp = {.kind = s->as.decl.declared};
    const type_info *t = s->as.decl.declared_full;
    if (t == NULL) {
      t = &tmp;
    }
    if (t->kind == TYPE_UNKNOWN) {
      tmp.kind = type_of_expr(s->as.decl.init);
      t = &tmp;
    }
    gen_expr_to(g, s->as.decl.init, t);
    int slot = declare_var(g, s, 0, t);
    printf("POPS ");
    print_slot(g, slot);
    printf("\n");
    break;
  }
  case STMT_ASSIGN: {
    const vis_t *v = lookup_var(g, s->as.assign.name);
    if (v == NULL) {
      throw_error(
          INTERNAL_ERROR,
          "Generator: undefined variable (semantic analysis missing?).");
    }
    gen_expr_to(g, s->as.assign.value, v->full);
    printf("POPS ");
    print_slot(g, v->slot);
    printf("\n");
    break;
  }
  case STMT_EXPR:
    gen_expr(g, s->as.expr.expr);
    printf("POPS GF@$discard\n");
    break;
  case STMT_RETURN:
    gen_result(g, s->as.expr.expr);
    codegen_function_end();
    break;
  case STMT_IF: {
    int id = codegen_new_label();
    gen_expr(g, s->as.if_.cond);
    printf("PUSHS bool@false\n");
    printf("JUMPIFEQS $if%d$else\n", id);
    gen_block(g, &s->as.if_.then_b);
    printf("JUMP $if%d$end\n", id);
    printf("LABEL $if%d$else\n", id);
    gen_block(g, &s->as.if_.else_b);
    printf("LABEL $if%d$end\n", id);
    break;
  }
  case STMT_WHILE: {
    int id = codegen_new_label();
    printf("LABEL $while%d$start\n", id);
    gen_expr(g, s->as.while_.cond);
    printf("PUSHS bool@false\n");
    printf("JUMPIFEQS $while%d$end\n", id);
    gen_block(g, &s->as.while_.body);
    printf("JUMP $while%d$start\n", id);
    printf("LABEL $while%d$end\n", id);
    break;
  }
  case STMT_FOR:
    gen_for(g, s);
    break;
  }
}

/**
 * Generates all statements of a block. The block has its own scope: variables
 * declared inside stop being visible after it.
 **/
static void gen_block(gen_t *g, const block *b) {
  int mark = g->nvis;
  for (int i = 0; i < b->count; i++) {
    gen_stmt(g, b->items[i]);
  }
  g->nvis = mark;
}

/* ---- Pass 1: variables of a function ---- */

static void collect_expr(gen_t *g, const expr *e);
static void collect_block(gen_t *g, const block *b);

/**
 * Pass 1: collects the variables declared inside a list of expressions.
 **/
static void collect_exprs(gen_t *g, expr *const *list, int count) {
  for (int i = 0; i < count; i++) {
    collect_expr(g, list[i]);
  }
}

/**
 * Pass 1: collects the variables declared inside an expression: the
 * temporaries of a match, the bindings of its patterns and the blocks of an
 * if expression. The body of a lambda is a separate function, so it is not
 * visited.
 **/
static void collect_expr(gen_t *g, const expr *e) {
  if (e == NULL) {
    return;
  }
  switch (e->e_type) {
  case EXPR_UNARY:
    collect_expr(g, e->as.unary.expr);
    break;
  case EXPR_BINARY:
    collect_expr(g, e->as.binary.left);
    collect_expr(g, e->as.binary.right);
    break;
  case EXPR_CALL:
    collect_exprs(g, e->as.call.args, e->as.call.argc);
    break;
  case EXPR_IF:
    collect_expr(g, e->as.if_.cond);
    collect_block(g, &e->as.if_.then_b);
    collect_block(g, &e->as.if_.else_b);
    break;
  case EXPR_MEMBER:
    collect_expr(g, e->as.member.obj);
    collect_exprs(g, e->as.member.args, e->as.member.argc);
    break;
  case EXPR_MATCH:
    collect_expr(g, e->as.match_.subject);
    add_slot(g, e, 0, "$subject");
    for (int i = 0; i < e->as.match_.case_count; i++) {
      const match_case *c = &e->as.match_.cases[i];
      if (c->pat == PAT_TYPED) {
        add_slot(g, c, 0, c->bind_name);
      }
      if (c->pat == PAT_CTOR) {
        for (int f = 0; f < c->field_count; f++) {
          if (strcmp(c->fields[f], "_") != 0) {
            add_slot(g, c, 1 + f, c->fields[f]);
          }
        }
      }
      collect_expr(g, c->guard);
      collect_block(g, &c->body);
    }
    break;
  case EXPR_INTERP:
    collect_exprs(g, e->as.interp.parts, e->as.interp.count);
    break;
  case EXPR_RANGE:
    collect_expr(g, e->as.range.from);
    collect_expr(g, e->as.range.to);
    collect_expr(g, e->as.range.step);
    break;
  default:
    break;
  }
}

/**
 * Pass 1: collects the declarations of one statement, including statements
 * nested in blocks and in expressions.
 **/
static void collect_stmt(gen_t *g, const stmt *s) {
  switch (s->type) {
  case STMT_VAL:
  case STMT_VAR:
    add_slot(g, s, 0, s->as.decl.name);
    collect_expr(g, s->as.decl.init);
    break;
  case STMT_ASSIGN:
    collect_expr(g, s->as.assign.value);
    break;
  case STMT_EXPR:
  case STMT_RETURN:
    collect_expr(g, s->as.expr.expr);
    break;
  case STMT_IF:
    collect_expr(g, s->as.if_.cond);
    collect_block(g, &s->as.if_.then_b);
    collect_block(g, &s->as.if_.else_b);
    break;
  case STMT_WHILE:
    collect_expr(g, s->as.while_.cond);
    collect_block(g, &s->as.while_.body);
    break;
  case STMT_FOR:
    add_slot(g, s, 0, s->as.for_.var);
    add_slot(g, s, 1, "$a");
    add_slot(g, s, 2, "$b");
    add_slot(g, s, 3, "$c");
    add_slot(g, s, 4, "$d");
    collect_expr(g, s->as.for_.source);
    collect_exprs(g, s->as.for_.guards, s->as.for_.guard_count);
    collect_block(g, &s->as.for_.body);
    break;
  }
}

/**
 * Pass 1: collects the declarations of all statements in a block.
 **/
static void collect_block(gen_t *g, const block *b) {
  for (int i = 0; i < b->count; i++) {
    collect_stmt(g, b->items[i]);
  }
}

/* ---- Functions and the whole program ---- */

/**
 * Generates one function or lambda: label and frame, DEFVAR of all its
 * variables, popping of the parameters (last one first) and the body. The
 * value of the last statement of the body is the result of a non-Unit
 * function; a Unit function returns nil@nil.
 *
 * @param label    name of the label without the leading $
 * @param ret_kind kind of the return type
 * @param ret_full complete return type, may be NULL
 **/
static void gen_function_body(gen_t *g, const char *label, param *params,
                              int nparams, const block *body,
                              data_type ret_kind, const type_info *ret_full) {
  if (ret_kind == TYPE_UNKNOWN) {
    throw_error(INTERNAL_ERROR,
                "Generator: unresolved return type (semantics must infer it).");
  }
  g->nslots = 0;
  g->nvis = 0;
  g->ret_kind = ret_kind;
  g->ret_tmp.kind = ret_kind;
  g->ret_target = ret_full;
  if (ret_full == NULL) {
    g->ret_target = &g->ret_tmp;
  }

  /* pass 1: parameters first, then every declaration in the body */
  for (int i = 0; i < nparams; i++) {
    add_slot(g, &params[i], 0, params[i].name);
  }
  collect_block(g, body);

  codegen_function_start(label);
  for (int i = 0; i < g->nslots; i++) {
    printf("DEFVAR ");
    print_slot(g, i);
    printf("\n");
  }

  /* the caller pushed the arguments left to right -> pop the last first */
  int first_param = g->nvis;
  for (int i = 0; i < nparams; i++) {
    type_info tmp = {.kind = TYPE_UNKNOWN};
    declare_var(g, &params[i], 0, param_target(&params[i], &tmp));
    /* declare_var keeps the pointer, so the complete type must be stable */
    if (params[i].full_type == NULL) {
      g->vis[g->nvis - 1].full = NULL;
    }
  }
  for (int i = nparams - 1; i >= 0; i--) {
    printf("POPS ");
    print_slot(g, g->vis[first_param + i].slot);
    printf("\n");
  }

  /* pass 2: the body */
  if (ret_kind == TYPE_UNIT) {
    for (int i = 0; i < body->count; i++) {
      gen_stmt(g, body->items[i]);
    }
    printf("PUSHS nil@nil\n");
  } else {
    gen_value_block(g, body, g->ret_target);
  }
  codegen_function_end();

  for (int i = 0; i < g->nslots; i++) {
    free(g->slots[i].name);
  }
  g->nslots = 0;
  g->nvis = 0;
}

/**
 * Generates one user-defined function.
 **/
static void gen_function(gen_t *g, func_def *f) {
  gen_function_body(g, f->name, f->params, f->param_count, &f->body,
                    f->ret_type, f->ret_full);
}

/**
 * LAMBDAS: generates the lambdas found so far as functions $lambda$N. A
 * lambda may contain another lambda, which is generated in the same loop.
 **/
static void gen_pending_lambdas(gen_t *g) {
  while (g->done_lambdas < g->nlambdas) {
    int id = g->done_lambdas++;
    const expr *e = g->lambdas[id];
    const type_info *ret = NULL;
    data_type kind = TYPE_UNKNOWN;
    if (e->full_type != NULL && e->full_type->kind == TYPE_FUNC) {
      ret = e->full_type->as.func_t.ret_type;
      if (ret != NULL) {
        kind = ret->kind;
      }
    }
    char label[32];
    snprintf(label, sizeof label, "lambda$%d", id);
    gen_function_body(g, label, e->as.lambda.params, e->as.lambda.param_count,
                      &e->as.lambda.body, kind, ret);
  }
}

/**
 * Generates IFJcode26 code for the entire program.
 * Prints the program header, initializes global variables, calls @main,
 * and generates code for all user-defined and built-in functions.
 *
 * @param prog program to generate code for that has passed the semantic
 *analysis
 **/
void generate_code(program *prog) {
  gen_t g = {0};
  g.prog = prog;

  const func_def *main_fn = NULL;
  for (int i = 0; i < prog->count; i++) {
    if (prog->funcs[i]->is_main) {
      main_fn = prog->funcs[i];
    }
  }
  if (main_fn == NULL) {
    throw_error(
        INTERNAL_ERROR,
        "Generator: no @main function (semantic analysis must check it).");
  }

  printf(".IFJcode26\n");
  printf("DEFVAR GF@$discard\n"); /* sink for ignored values */
  printf("DEFVAR GF@$t1\n");      /* temporaries for strings, types, ... */
  printf("DEFVAR GF@$t2\n");
  printf("DEFVAR GF@$t3\n");
  printf("DEFVAR GF@$t4\n");
  printf("DEFVAR GF@$arr\n"); /* a case class being built */
  codegen_call(main_fn->name);
  printf("EXIT int@0\n");

  for (int i = 0; i < prog->count; i++) {
    gen_function(&g, prog->funcs[i]);
    gen_pending_lambdas(&g);
  }
  builtins_generate();

  free(g.vis);
  free(g.slots);
  free(g.lambdas);
}