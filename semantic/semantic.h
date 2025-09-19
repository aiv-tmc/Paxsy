#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "../parser/parser.h"
#include <stdbool.h>

// Symbol kinds
typedef enum {
    SYMBOL_VARIABLE,
    SYMBOL_FUNCTION,
    SYMBOL_STRUCT,
    SYMBOL_PARAMETER
} SymbolKind;

// Modifier flags
typedef enum {
    MODIFIER_CONST = 1 << 0,
    MODIFIER_STATIC = 1 << 1
} ModifierFlags;

// Symbol structure
typedef struct Symbol {
    char *name;
    SymbolKind kind;
    Type *type;
    unsigned int modifiers;
    struct Symbol *next;
} Symbol;

// Scope kinds
typedef enum {
    SCOPE_GLOBAL,
    SCOPE_FUNCTION,
    SCOPE_BLOCK
} ScopeKind;

// Scope structure
typedef struct Scope {
    struct Scope *parent;
    Symbol *symbols;
    ScopeKind kind;
    bool is_variadic;
} Scope;

// Function declarations
void reset_semantic_analysis(void);
void semantic_analysis(AST *ast);
Scope *get_global_scope(void);
void print_symbol_table(Scope *scope);

#endif

