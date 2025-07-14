#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "error_manager.h"

typedef enum {
    AST_VARIABLE_DECL,
    AST_ASSIGNMENT,
    AST_COMPOUND_ASSIGN,
    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_LITERAL,
    AST_IDENTIFIER,
    AST_IF,
    AST_ELIF,
    AST_ELSE,
    AST_BLOCK,
    AST_FUNCTION,
    AST_FUNCTION_CALL,
    AST_START_FUNCTION
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    TokenType op_type;
    char *value;
    struct ASTNode *left;
    struct ASTNode *right;
    struct ASTNode *extra;
    int base;
    int size;
    char *var_type;
} ASTNode;

typedef struct {
    ASTNode **nodes;
    int count;
    int capacity;
} AST;

AST *parse(Token *tokens, int token_count);
void free_ast(AST *ast);
void print_ast(AST *ast);

#endif

