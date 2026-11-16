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
typedef struct {
  char *name; /* source name */
  int id;     /* unique number -> LF@name$id */
  data_type type;
} var_info;

typedef struct {
  program *prog;
  func_def *func; /* function being generated */

  var_info *vars; /* variables visible now (scope stack) */
  int nvars, cap_vars;

  var_info *locals; /* all variables of the function, declaration order */
  int nlocals, cap_locals;
  int next_local; /* next entry of `locals` consumed by declare_var */

  int var_counter;
} gen_t;

/* ---- Variables ---- */

/**
 * Pass 1: records a variable declared in the function being generated.
 * The variable gets a unique id, so it is emitted as LF@name$id.
 *
 * @param g    generator state
 * @param name source name of the variable or parameter
 **/
static void add_local(gen_t *g, const char *name) {
  if (g->nlocals == g->cap_locals) {
    if (g->cap_locals == 0) {
      g->cap_locals = 16;
    } else {
      g->cap_locals *= 2;
    }
    g->locals =
        ast_xrealloc(g->locals, (size_t)g->cap_locals * sizeof(var_info));
  }
  g->locals[g->nlocals].name = ast_xstrdup(name);
  g->locals[g->nlocals].id = ++g->var_counter;
  g->locals[g->nlocals].type = TYPE_UNKNOWN;
  g->nlocals++;
}

static void collect_block(gen_t *g, const block *b);

/**
 * Pass 1: collects the declarations of one statement, including statements
 * nested in if/while blocks.
 **/
static void collect_stmt(gen_t *g, const stmt *s) {
  switch (s->type) {
  case STMT_VAL:
  case STMT_VAR:
    add_local(g, s->as.decl.name);
    break;
  case STMT_IF:
    collect_block(g, &s->as.if_.then_b);
    collect_block(g, &s->as.if_.else_b);
    break;
  case STMT_WHILE:
    collect_block(g, &s->as.while_.body);
    break;
  default:
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

/**
 * Pass 2: a declaration was reached, so the variable becomes visible. Takes
 * the next entry prepared by pass 1 and checks that the names agree.
 *
 * @param g    generator state
 * @param name declared name
 * @param type final type of the variable (declared or inferred)
 * @return     the visible variable (valid until the next declare_var)
 **/
static const var_info *declare_var(gen_t *g, const char *name, data_type type) {
  if (g->next_local >= g->nlocals ||
      strcmp(g->locals[g->next_local].name, name) != 0) {
    throw_error(INTERNAL_ERROR, "Generator: variable collection out of sync.");
  }
  var_info v = g->locals[g->next_local++];
  v.type = type;

  if (g->nvars == g->cap_vars) {
    if (g->cap_vars == 0) {
      g->cap_vars = 16;
    } else {
      g->cap_vars *= 2;
    }
    g->vars = ast_xrealloc(g->vars, (size_t)g->cap_vars * sizeof(var_info));
  }
  g->vars[g->nvars] = v;
  return &g->vars[g->nvars++];
}

/**
 * Looks up a variable by name, starting from the most recently declared
 * one to respect nested scopes. An unknown name is an internal error, should
 * have been caught by semantic analysis.
 **/
static const var_info *lookup_var(const gen_t *g, const char *name) {
  for (int i = g->nvars - 1; i >= 0; i--) {
    if (strcmp(g->vars[i].name, name) == 0) {
      return &g->vars[i];
    }
  }
  throw_error(INTERNAL_ERROR,
              "Generator: undefined variable (semantic analysis missing?).");
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
 * Returns the type of an expression. Missing type is an internal error.
 **/
static data_type type_of_expr(const expr *e) {
  if (e->d_type == TYPE_UNKNOWN) {
    throw_error(INTERNAL_ERROR, "Generator: expression without a type ");
  }
  return e->d_type;
}

/* ---- Expressions ---- */

static void gen_expr(gen_t *g, const expr *e);

/**
 * Generates an expression and converts Int to Double.
 *
 * @param target type the surrounding construct expects (TYPE_UNKNOWN = any)
 **/
static void gen_expr_as(gen_t *g, const expr *e, data_type target) {
  gen_expr(g, e);
  if (target == TYPE_DOUBLE && type_of_expr(e) == TYPE_INT) {
    printf("INT2FLOATS\n");
  }
}

/**
 * Generates a function call: arguments left to right, then CALL. The result is
 *left on the stack.
 **/
static void gen_call(gen_t *g, const expr *e) {
  const func_def *f = find_func(g, e->as.call.name);
  const builtin_sig *b = NULL;
  if (f == NULL) {
    b = find_builtin(e->as.call.name);
  }
  if (f == NULL && b == NULL) {
    throw_error(INTERNAL_ERROR, "Generator: unknown function");
  }

  for (int i = 0; i < e->as.call.argc; i++) {
    data_type pt = TYPE_UNKNOWN;
    if (f != NULL && i < f->param_count) {
      pt = f->params[i].type;
    } else if (b != NULL && i < b->nparams) {
      pt = b->params[i];
    }
    gen_expr_as(g, e->as.call.args[i], pt);
  }
  codegen_call(e->as.call.name);
}

/**
 * Generates a binary operation. Both operands are first converted to a common
 * type. The result is left on the stack.
 * String "+" is done by CONCAT, <=, >=, != are built as
 * a negation of the opposite comparison.
 **/
static void gen_binary(gen_t *g, const expr *e) {
  data_type lt = type_of_expr(e->as.binary.left);
  data_type rt = type_of_expr(e->as.binary.right);
  data_type common = lt;
  if (lt == TYPE_DOUBLE || rt == TYPE_DOUBLE) {
    common = TYPE_DOUBLE;
  }

  gen_expr_as(g, e->as.binary.left, common);
  gen_expr_as(g, e->as.binary.right, common);

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
    break; /* not a binary operator */
  }
}

/**
 * Generates code that leaves the value of expression e on top of the stack.
 **/
static void gen_expr(gen_t *g, const expr *e) {
  switch (e->e_type) {
  case EXPR_INT:
    printf("PUSHS ");
    codegen_int_const(e->as.int_val);
    printf("\n");
    break;
  case EXPR_DOUBLE:
    printf("PUSHS ");
    codegen_float_const(e->as.double_val);
    printf("\n");
    break;
  case EXPR_STRING:
    printf("PUSHS ");
    codegen_string_const(e->as.str_val);
    printf("\n");
    break;
  case EXPR_UNIT:
    printf("PUSHS nil@nil\n");
    break;
  case EXPR_VAR: {
    const var_info *v = lookup_var(g, e->as.name);
    printf("PUSHS ");
    codegen_var(v->name, v->id);
    printf("\n");
    break;
  }
  case EXPR_UNARY: /* only unary minus: x * -1 */
    gen_expr(g, e->as.unary.expr);
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
  if (g->func->ret_type == TYPE_UNIT) {
    if (e != NULL) {
      gen_expr(g, e);
      printf("POPS GF@$discard\n");
    }
    printf("PUSHS nil@nil\n");
  } else {
    gen_expr_as(g, e, g->func->ret_type);
  }
}

/**
 * Generates one statement: val/var, assignment, expression statement, return,
 *if or while.
 **/
static void gen_stmt(gen_t *g, const stmt *s) {
  switch (s->type) {
  case STMT_VAL:
  case STMT_VAR: {
    /* the initializer is evaluated BEFORE the new name becomes visible */
    data_type t = s->as.decl.declared;
    if (t == TYPE_UNKNOWN) {
      t = type_of_expr(s->as.decl.init);
    }
    gen_expr_as(g, s->as.decl.init, t);
    const var_info *v = declare_var(g, s->as.decl.name, t);
    printf("POPS ");
    codegen_var(v->name, v->id);
    printf("\n");
    break;
  }
  case STMT_ASSIGN: {
    const var_info *v = lookup_var(g, s->as.assign.name);
    gen_expr_as(g, s->as.assign.value, v->type);
    printf("POPS ");
    codegen_var(v->name, v->id);
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
  }
}

/**
 * Generates all statements of a block. The block has its own scope: variables
 * declared inside stop being visible after it.
 **/
static void gen_block(gen_t *g, const block *b) {
  int mark = g->nvars;
  for (int i = 0; i < b->count; i++) {
    gen_stmt(g, b->items[i]);
  }
  g->nvars = mark;
}

/* ---- Functions and the whole program ---- */

/**
 * Generates one function: label and frame, DEFVAR of all its variables,
 * popping of the parameters (last one first) and the body. The value of the
 * last statement, if it is an expression, is the result of a non-Unit
 * function; a body that falls off the end returns nil@nil.
 **/
static void gen_function(gen_t *g, func_def *f) {
  if (f->ret_type == TYPE_UNKNOWN) {
    throw_error(INTERNAL_ERROR,
                "Generator: unresolved return type (semantics must infer it).");
  }
  g->func = f;
  g->nlocals = 0;
  g->next_local = 0;
  g->nvars = 0;

  /* pass 1: parameters first, then every declaration in the body */
  for (int i = 0; i < f->param_count; i++) {
    add_local(g, f->params[i].name);
  }
  collect_block(g, &f->body);

  codegen_function_start(f->name);
  for (int i = 0; i < g->nlocals; i++) {
    printf("DEFVAR ");
    codegen_var(g->locals[i].name, g->locals[i].id);
    printf("\n");
  }

  /* the caller pushed the arguments left to right -> pop the last first */
  for (int i = 0; i < f->param_count; i++) {
    declare_var(g, f->params[i].name, f->params[i].type);
  }
  for (int i = f->param_count - 1; i >= 0; i--) {
    printf("POPS ");
    codegen_var(g->vars[i].name, g->vars[i].id);
    printf("\n");
  }

  /* pass 2: the body; the value of the last expression is the result */
  bool returned = false;
  for (int i = 0; i < f->body.count; i++) {
    const stmt *s = f->body.items[i];
    bool last = (i == f->body.count - 1);
    if (last && s->type == STMT_EXPR && f->ret_type != TYPE_UNIT) {
      gen_expr_as(g, s->as.expr.expr, f->ret_type);
      codegen_function_end();
      returned = true;
    } else {
      gen_stmt(g, s);
    }
  }
  if (!returned) { /* falls off the end: the result is Unit */
    printf("PUSHS nil@nil\n");
    codegen_function_end();
  }

  for (int i = 0; i < g->nlocals; i++) {
    free(g->locals[i].name);
  }
  g->nvars = 0;
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
  printf("DEFVAR GF@$t1\n");      /* temporaries for string concatenation */
  printf("DEFVAR GF@$t2\n");
  codegen_call(main_fn->name);
  printf("EXIT int@0\n");

  for (int i = 0; i < prog->count; i++) {
    gen_function(&g, prog->funcs[i]);
  }
  builtins_generate();

  free(g.vars);
  free(g.locals);
}