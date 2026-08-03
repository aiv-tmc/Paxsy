#ifndef OPTIMIZER_AST_UTILS_H
#define OPTIMIZER_AST_UTILS_H

#include "../parser/parser.h"
#include "../semantic/semantic.h"

/*
 * Traverse the AST and apply a fold function to every node.
 * The fold function receives a node and a semantic context; if it returns a
 * non‑NULL pointer, that pointer replaces the current node, and the old
 * subtree is freed. The traversal is depth‑first, processing children first.
 */
void u__replace_expressions(ASTNode** node,
                            ASTNode* (*fold)(ASTNode*, SemanticContext*),
                            SemanticContext* ctx);

/*
 * Free an AST node and all its children recursively.
 * This is the public interface for memory deallocation.
 */
void u__free_node(ASTNode* node);

#endif
