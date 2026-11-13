/**
 * IFJ Compiler - Abstract Syntax Tree implementation
 * Author: David Pokorák (xpokord00)
 * Date: 10.10.2026
 **/

#include "ast.h"
#include "error.h"

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

/* Free functions */
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
    for (int i = 0; i < e->as.call.argc; i++) {
      ast_free_expr(e->as.call.args[i]);
    }
    free(e->as.call.args);
    free(e->as.call.name);
    break;
  default:
    break;
  }
  free(e);
}

static void free_stmt(stmt *s);

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
  }
  free(s);
}

void ast_free_program(program *p) {
  if (p == NULL) {
    return;
  }
  for (int i = 0; i < p->count; i++) {
    func_def *f = p->funcs[i];
    for (int j = 0; j < f->param_count; j++) {
      free(f->params[j].name);
    }
    free(f->params);
    free(f->name);
    free_block(&f->body);
    free(f);
  }
  free(p->funcs);
  free(p);
}