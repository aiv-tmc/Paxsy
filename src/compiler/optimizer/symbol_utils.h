#ifndef OPTIMIZER_SYMBOL_UTILS_H
#define OPTIMIZER_SYMBOL_UTILS_H

#include "../semantic/symbol.h"
#include "../semantic/semantic.h"

/*
 * Helper to retrieve a symbol from the current scope.
 * Returns the symbol if found, otherwise NULL.
 */
Symbol* u__lookup(SemanticContext* ctx, const char* name);

#endif
