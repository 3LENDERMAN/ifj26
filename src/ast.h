/**
 * IFJ Compiler - Abstract Syntax Tree header file
 * Author: David Pokorák (xpokord00)
 * Date: 10.10.2026
 **/

#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stddef.h> // size_t

/* Definiton of data types */
typedef enum {
  TYPE_UNKNOWN,
  TYPE_INT,
  TYPE_DOUBLE,
  TYPE_STRING,
  TYPE_UNIT,
  TYPE_BOOL,
  TYPE_UNION,
  TYPE_CASE_CLASS,
  TYPE_FUNC,
  TYPE_LIST
} data_type;

/* Definition of expression types */
typedef enum {
  EXPR_INT,
  EXPR_DOUBLE,
  EXPR_STRING,
  EXPR_UNIT,
  EXPR_BOOL,
  EXPR_VAR,
  EXPR_UNARY,
  EXPR_BINARY,
  EXPR_CALL,
  EXPR_IF,     /* CONTROL: if used as an expression */
  EXPR_LAMBDA, /* LAMBDAS: (a: Int) => body */
  EXPR_MEMBER, /* VALMEM, LISTS, ALGEBDT: obj.name or obj.name(args) */
  EXPR_MATCH,  /* ALGEBDT: subject match case ... */
  EXPR_INTERP, /* INTERPOLATEDS: s"..." */
  EXPR_RANGE   /* CONTROL: from to/until to [by step] */
} expr_type;

/* Definition of operations */
typedef enum {
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_NEG,
  OP_LT,
  OP_LE,
  OP_GT,
  OP_GE,
  OP_EQ,
  OP_NE,
  OP_NOT,
  OP_AND,
  OP_OR,
  OP_CONCAT /* LISTS: ++ */
} op_type;

/* Definition of statements */
typedef enum {
  STMT_VAL,
  STMT_VAR,
  STMT_ASSIGN,
  STMT_EXPR,
  STMT_RETURN,
  STMT_IF,
  STMT_WHILE,
  STMT_FOR /* CONTROL */
} stmt_type;

/* What a call expression really is (set by the semantic analysis) */
typedef enum {
  CALL_FUNC,      /* user-defined or built-in function (default) */
  CALL_VALUE,     /* variable holding a function: f(a) */
  CALL_INDEX,     /* list indexing: lst(i) */
  CALL_CTOR,      /* constructor of a case class: Point(1, 2) */
  CALL_LIST_NEW,  /* List(a, b, ...) */
  CALL_LIST_RANGE /* List.range(a, b [, step]) */
} call_kind;

/* What a member expression really is (set by the semantic analysis) */
typedef enum {
  MEMBER_TO_STRING,
  MEMBER_TO_INT,
  MEMBER_FLOOR,
  MEMBER_LENGTH,
  MEMBER_FIELD, /* attribute of a case class */
  MEMBER_LIST_TO_LIST,
  MEMBER_LIST_DROP,
  MEMBER_LIST_TAKE,
  MEMBER_LIST_SLICE,
  MEMBER_LIST_HEAD,
  MEMBER_LIST_TAIL,
  MEMBER_LIST_FILTER,
  MEMBER_LIST_TAKE_WHILE,
  MEMBER_LIST_DROP_WHILE
} member_kind;

/* Kinds of patterns of a match branch */
typedef enum { PAT_WILDCARD, PAT_LITERAL, PAT_TYPED, PAT_CTOR } pattern_kind;

typedef struct expr expr;
typedef struct stmt stmt;
typedef struct block block;
typedef struct type_info type_info;
typedef struct match_case match_case;

struct type_info {
  data_type kind;
  union {
    char *class_name;
    struct {
      type_info **members;
      int count;
    } union_t;
    struct {
      type_info **param_types;
      int param_count;
      type_info *ret_type;
    } func_t;
    type_info *elem_type;
  } as;
};

struct block {
  stmt **items;
  int count;
};

typedef struct {
  char *name;
  data_type type;
  type_info *full_type;
} param;

struct expr {
  expr_type e_type;
  data_type d_type;
  type_info *full_type; /* complete type, set by the semantic analysis */
  int line;
  union {
    long long int_val;
    double double_val;
    char *str_val;
    char *name;
    bool bool_val;
    struct {
      expr *expr;
      op_type op;
    } unary;
    struct {
      expr *left;
      expr *right;
      op_type op;
    } binary;
    struct {
      expr **args;
      int argc;
      char *name;
      call_kind kind;
    } call;
    struct {
      expr *cond;
      block then_b, else_b;
    } if_;
    struct {
      param *params;
      int param_count;
      block body;
    } lambda;
    struct {
      expr *obj;
      char *name;
      expr **args;
      int argc;
      member_kind kind;
      int field_index; /* MEMBER_FIELD: index of the attribute */
    } member;
    struct {
      expr *subject;
      match_case *cases;
      int case_count;
    } match_;
    struct {
      expr **parts; /* literal strings and embedded expressions in order */
      int count;
    } interp;
    struct {
      expr *from;
      expr *to;
      expr *step; /* NULL = 1 */
      bool inclusive;
    } range;
  } as;
};

/* One branch "case pattern [if guard] => body" of a match */
struct match_case {
  pattern_kind pat;
  char *bind_name;      /* PAT_TYPED: name of the bound variable */
  type_info *bind_type; /* PAT_TYPED: type of the pattern */
  char *class_name;     /* PAT_CTOR */
  char **fields;        /* PAT_CTOR: names of the attributes ("_" = ignored) */
  int field_count;
  expr *literal; /* PAT_LITERAL */
  expr *guard;   /* NULL = no guard */
  block body;
};

struct stmt {
  stmt_type type;
  int line;
  union {
    struct {
      char *name;
      data_type declared;
      type_info *declared_full;
      expr *init;
    } decl;
    struct {
      char *name;
      expr *value;
    } assign;
    struct {
      expr *expr;
    } expr;
    struct {
      expr *cond;
      block then_b, else_b;
    } if_;
    struct {
      expr *cond;
      block body;
    } while_;
    struct {
      char *var;
      expr *source;
      expr **guards;
      int guard_count;
      block body;
    } for_;
  } as;
};

/* Top level definition of a type alias or a case class (ALGEBDT) */
typedef struct {
  char *name;
  bool is_class;
  type_info *alias; /* type alias: the aliased type */
  param *fields;    /* case class: attributes */
  int field_count;
} type_def;

typedef struct {
  int line;
  char *name;
  data_type ret_type;
  type_info *ret_full;
  int param_count;
  param *params;
  block body;
  bool is_main;
} func_def;

typedef struct {
  func_def **funcs;
  int count;
  type_def *types;
  int type_count;
} program;

/* Allocation functions */
void *ast_xcalloc(size_t n, size_t size);
void *ast_xrealloc(void *ptr, size_t size);
char *ast_xstrdup(const char *s);

/* Shared type_info of a primitive type, ast_free_type skips it */
type_info *ast_prim_type(data_type kind);

/* Appends item to the dynamic array arr with count elements */
#define APPEND(arr, count, item)                                               \
  do {                                                                         \
    (arr) = ast_xrealloc((arr), sizeof(*(arr)) * ((size_t)(count) + 1));       \
    (arr)[(count)] = (item);                                                   \
    (count)++;                                                                 \
  } while (0)

/* Free functions */
void ast_free_expr(expr *e);
void ast_free_type(type_info *t);
void ast_free_program(program *p);

#endif