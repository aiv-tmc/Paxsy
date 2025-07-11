#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

typedef enum {
    AST_VARIABLE_DECL,
    AST_PROGRAM
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    void* data;
    struct ASTNode* next;
} ASTNode;

typedef struct {
    char* name;
    char* var_type;
} ASTVariableDecl;

ASTNode* parse(Token* tokens, int token_count);
void print_ast(ASTNode* ast);
void free_ast(ASTNode* ast);

#endif
