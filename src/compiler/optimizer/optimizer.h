#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include "../parser/parser.h"
#include "../semantic/semantic.h"   /* for struct Scope */

/*
 * Perform optimization on the entire AST.
 * ast: root of the AST (Program node)
 * global_scope: global scope for symbol resolution (currently unused)
 * Returns 0 on success, non-zero on error.
 */
int optimizer(ASTNode* ast, struct Scope* global_scope);

/*
 * Set the optimisation level (0 = none, 1 = basic, 2 = full).
 */
void optimizer_set_level(int level);

/*
 * Enable or disable debug output.
 */
void optimizer_set_debug(bool debug);

#endif
