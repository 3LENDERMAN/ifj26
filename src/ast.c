/**
 * IFJ Compiler - Abstract Syntax Tree implementation
 * Author: David Pokorák (xpokord00)
 * Date: 10.10.2026
 **/

#include "ast.h"
#include "error.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Alocation functions */
void *ast_xcalloc(size_t n, size_t size) {
  void *p = calloc(n, size);
  if (p == NULL) {
    throw_error(INTERNAL_ERROR, "Memory allocation failed.");
  }
  return p;
}

void *ast_xrealloc(void *ptr, size_t size) {
  void *p = realloc(ptr, size);
  if (p == NULL) {
    throw_error(INTERNAL_ERROR, "Memory allocation failed.");
  }
  return p;
}

char *ast_xstrdup(const char *s) {
  size_t len = strlen(s) + 1;
  char *copy = ast_xcalloc(len, 1);
  memcpy(copy, s, len);
  return copy;
}

static type_info prim_types[TYPE_LIST + 1];

type_info *ast_prim_type(data_type kind) {
  prim_types[kind].kind = kind;
  return &prim_types[kind];
}

/* Free functions */
static void free_stmt(stmt *s);
static void free_block(block *b);

/**
 * Frees a type_info tree, the shared primitive types are kept
 **/
void ast_free_type(type_info *t) {
  if (t == NULL || t == &prim_types[t->kind]) {
    return;
  }
  switch (t->kind) {
  case TYPE_CASE_CLASS:
    free(t->as.class_name);
    break;
  case TYPE_UNION:
    for (int i = 0; i < t->as.union_t.count; i++) {
      ast_free_type(t->as.union_t.members[i]);
    }
    free(t->as.union_t.members);
    break;
  case TYPE_FUNC:
    for (int i = 0; i < t->as.func_t.param_count; i++) {
      ast_free_type(t->as.func_t.param_types[i]);
    }
    free(t->as.func_t.param_types);
    ast_free_type(t->as.func_t.ret_type);
    break;
  case TYPE_LIST:
    ast_free_type(t->as.elem_type);
    break;
  default:
    break;
  }
  free(t);
}

static void free_expr_list(expr **list, int count) {
  for (int i = 0; i < count; i++) {
    ast_free_expr(list[i]);
  }
  free(list);
}

static void free_params(param *params, int count) {
  for (int i = 0; i < count; i++) {
    free(params[i].name);
    ast_free_type(params[i].full_type);
  }
  free(params);
}

static void free_match_case(match_case *c) {
  free(c->bind_name);
  ast_free_type(c->bind_type);
  free(c->class_name);
  for (int i = 0; i < c->field_count; i++) {
    free(c->fields[i]);
  }
  free(c->fields);
  ast_free_expr(c->literal);
  ast_free_expr(c->guard);
  free_block(&c->body);
}

void ast_free_expr(expr *e) {
  if (e == NULL) {
    return;
  }
  switch (e->e_type) {
  case EXPR_STRING:
    free(e->as.str_val);
    break;
  case EXPR_VAR:
    free(e->as.name);
    break;
  case EXPR_UNARY:
    ast_free_expr(e->as.unary.expr);
    break;
  case EXPR_BINARY:
    ast_free_expr(e->as.binary.left);
    ast_free_expr(e->as.binary.right);
    break;
  case EXPR_CALL:
    free_expr_list(e->as.call.args, e->as.call.argc);
    free(e->as.call.name);
    break;
  case EXPR_IF:
    ast_free_expr(e->as.if_.cond);
    free_block(&e->as.if_.then_b);
    free_block(&e->as.if_.else_b);
    break;
  case EXPR_LAMBDA:
    free_params(e->as.lambda.params, e->as.lambda.param_count);
    free_block(&e->as.lambda.body);
    break;
  case EXPR_MEMBER:
    ast_free_expr(e->as.member.obj);
    free(e->as.member.name);
    free_expr_list(e->as.member.args, e->as.member.argc);
    break;
  case EXPR_MATCH:
    ast_free_expr(e->as.match_.subject);
    for (int i = 0; i < e->as.match_.case_count; i++) {
      free_match_case(&e->as.match_.cases[i]);
    }
    free(e->as.match_.cases);
    break;
  case EXPR_INTERP:
    free_expr_list(e->as.interp.parts, e->as.interp.count);
    break;
  case EXPR_RANGE:
    ast_free_expr(e->as.range.from);
    ast_free_expr(e->as.range.to);
    ast_free_expr(e->as.range.step);
    break;
  default:
    break;
  }
  ast_free_type(e->full_type);
  free(e);
}

static void free_block(block *b) {
  for (int i = 0; i < b->count; i++) {
    free_stmt(b->items[i]);
  }
  free(b->items);
  b->items = NULL;
  b->count = 0;
}

static void free_stmt(stmt *s) {
  if (s == NULL) {
    return;
  }
  switch (s->type) {
  case STMT_VAL:
  case STMT_VAR:
    free(s->as.decl.name);
    ast_free_type(s->as.decl.declared_full);
    ast_free_expr(s->as.decl.init);
    break;
  case STMT_ASSIGN:
    free(s->as.assign.name);
    ast_free_expr(s->as.assign.value);
    break;
  case STMT_EXPR:
  case STMT_RETURN:
    ast_free_expr(s->as.expr.expr);
    break;
  case STMT_IF:
    ast_free_expr(s->as.if_.cond);
    free_block(&s->as.if_.then_b);
    free_block(&s->as.if_.else_b);
    break;
  case STMT_WHILE:
    ast_free_expr(s->as.while_.cond);
    free_block(&s->as.while_.body);
    break;
  case STMT_FOR:
    free(s->as.for_.var);
    ast_free_expr(s->as.for_.source);
    free_expr_list(s->as.for_.guards, s->as.for_.guard_count);
    free_block(&s->as.for_.body);
    break;
  }
  free(s);
}

void ast_free_program(program *p) {
  if (p == NULL) {
    return;
  }
  for (int i = 0; i < p->count; i++) {
    func_def *f = p->funcs[i];
    free_params(f->params, f->param_count);
    ast_free_type(f->ret_full);
    free(f->name);
    free_block(&f->body);
    free(f);
  }
  free(p->funcs);
  for (int i = 0; i < p->type_count; i++) {
    free(p->types[i].name);
    ast_free_type(p->types[i].alias);
    free_params(p->types[i].fields, p->types[i].field_count);
  }
  free(p->types);
  free(p);
}