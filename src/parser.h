#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "error_manager.h"

// AST Node Types
typedef enum {
    AST_VAR_DECL,
    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_LITERAL,
    AST_ID,
    AST_VAR,
    AST_PTR_VAR,
    AST_REF_VAR,
    AST_DEREF,
    AST_ADDR,
    AST_ASSIGNMENT,
    AST_COMPOUND_ASSIGN,
    AST_BLOCK,
    AST_FUNC,
    AST_STARTFUNC,
    AST_FUNC_CALL,
    AST_STARTFUNC_CALL,
    AST_NONE,
    AST_NULL,
    AST_IF,
    AST_ELSE,
    AST_RETURN,
    AST_FREE,
    AST_BREAK,
    AST_CONTINUE,
    AST_COMPILE,
    AST_SIZE,
    AST_PARSER,
    AST_REALLOC,
    AST_ALLOC,
    AST_STACK,
    AST_PUSH,
    AST_POP,
    AST_STRUCT_DECL,
    AST_STRUCT_INST,
    AST_CAST,
    AST_SYSCALL,
} ASTNodeType;

// Type information structure with modifiers
typedef struct {
    char *name;
    int base;
    int size;
    bool is_pointer;
    bool is_reference;
    char **modifiers;
    int modifier_count;
    int modifier_capacity;
} Type;

// AST Node Structure
typedef struct ASTNode {
    ASTNodeType type;
    TokenType op_type;
    char *value;
    struct ASTNode *left;
    struct ASTNode *right;
    struct ASTNode *extra;
    Type *var_type;
    Type *return_type;
    TokenType decl_sigil;
} ASTNode;

// Abstract Syntax Tree
typedef struct {
    ASTNode **nodes;
    int count;
    int capacity;
} AST;

// Public interface
AST *parse(Token *tokens, int token_count);
void free_ast(AST *ast);
void print_ast(AST *ast);

#endif

