/**
 * IFJ Compiler - Abstract Syntax Tree header file
 * Author: David Pokorák (xpokord00)
 * Date: 10.10.2026
 **/

#ifndef AST_H
#define AST_H


/*
    Definiton of data types
*/
typedef enum {
    // Not known yet, filled in by semantic analysis
    TYPE_UNKNOWN,
    TYPE_INT,
    TYPE_DOUBLE,
    TYPE_STRING,
    TYPE_UNIT,
    TYPE_BOOL
} data_type;

/*
    Definition of expression types
*/
typedef enum {
    EXPR_INT,
    EXPR_DOUBLE,
    EXPR_STRING,
    EXPR_UNIT,
    EXPR_VAR,
    EXPR_UNARY,
    EXPR_BINARY,
    // Function call
    EXPR_CALL
} expr_type;

/*
    Definition of operations
*/
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
    OP_NE
} op_type;

/*
    Definition of statements
*/
typedef enum {
    STMT_VAL,
    STMT_VAR,
    STMT_ASSIGN,
    STMT_EXPR,
    STMT_RETURN,
    STMT_IF,
    STMT_WHILE
} stmt_type;

typedef struct expr expr;
struct expr {
    /**/
    };

typedef struct stmt stmt;
struct stmt {
    /**/
};

typedef struct {
    stmt **items;
    int count;
} block;

typedef struct {
    char *name;
    data_type type;
} param;

typedef struct {
    char *name;
} func_def;

typedef struct {
    func_def *funcs;
    int count;
} program;

void ast_free_expr(expr *e);
void ast_free_program(program *p);

#endif