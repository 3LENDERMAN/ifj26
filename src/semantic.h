/**
 * IFJ26 Compiler - Header file for semantic analysis
 * Author: Vít Kutěj (xkutejv00)
 * Date: 30.10.2026
 **/

#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "error.h"
#include "parser.h"

void semantic_analyze(program *prog);

#endif