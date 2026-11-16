/**
 * IFJ26 Compiler - IFJcode26 generator
 * Author: David Pokorák (xpokord00)
 * Date: 5.11.2026
 **/

#ifndef IFJ26_GENERATOR_H
#define IFJ26_GENERATOR_H

#include "ast.h"

/**
 * Generates the whole IFJcode26 program for the given AST and prints it to
 * stdout. Must be called only after the semantic analysis has succeeded.
 *
 * * @param prog AST of the program
 **/
void generate_code(program *prog);

#endif