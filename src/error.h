/**
 * IFJ26 Compiler - Error messages header file
 * Author: Jakub Holub (xholubj00)
 * Date: 7.10.2026
**/

#ifndef ERROR_H
#define ERROR_H

/**
 * Definition of all error codes
**/
typedef enum {
    SUCCESS = 0,
    
    // Error in area of lexical analysis (lexem structure, comments, indentation)
    LEXER_ERROR = 1,

    // Error in area of syntactic analysis (syntax, missing prolog, undefined construction, structure of indent blocks)
    SYNTAX_ERROR = 2,

    // Use of undefined function, variable or type, missingg MAIN function
    SEMANTIC_ERROR_UNDEFINED = 3,

    // Undefined redefinition of function, variable or type
    SEMANTIC_ERROR_REDEFINITION = 4,
    
    // incorrect number/type of args of func. call, use of permitted parameters
    SEMANTIC_ERROR_FUNC_CALL = 5, 

    // Invalid type compatibility or type derivation in expression, assignment, initialization, return value or construction
    SEMANTIC_ERROR_DATA_TYPE = 6,

    // Attempt to write into unmodifiable variable or formal parameter of function 
    SEMANTIC_ERROR_MUTABILITY = 7,

    // Other semantics errors, such as invalid RETURN, zero step size, ...
    SEMANTIC_ERROR_OTHER = 10,

    // Unspecified errors created during runtime
    SEMANTIC_ERROR_RUNTIME = 40,

    // Error of compiler
    INTERNAL_ERROR = 99
} Error_code;

void throw_error(Error_code code, const char *msg);
void cleanup_garbage(void);

#endif
