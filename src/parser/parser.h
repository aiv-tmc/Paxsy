#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>
#include "../lexer/lexer.h"
#include "../error_manager/error_manager.h"

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
    AST_FUNC_CALL,
    AST_NONE,
    AST_NULL,
    AST_IF,
    AST_ELSE,
    AST_RETURN,
    AST_FREE,
    AST_BREAK,
    AST_BUILD,
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
    AST_SELF,
    AST_PUBLIC,
    AST_MULTI_DECL,
    AST_MULTI_ASSIGN,
    AST_ARRAY_INIT
} ASTNodeType;

typedef struct {
    char *name;
    uint8_t is_pointer : 1;
    uint8_t is_reference : 1;
    int16_t left_number;
    int16_t right_number;
    char **modifiers;
    uint8_t modifier_count;
    uint8_t is_array : 1;
    int16_t array_size;
} Type;

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
    uint8_t is_variadic : 1;
} ASTNode;

typedef struct {
    ASTNode **nodes;
    uint16_t count;
    uint16_t capacity;
} AST;

AST *parse(Token *tokens, int token_count);
void free_ast(AST *ast);
void print_ast(AST *ast);
void free_type(Type *type);
void free_ast_node(ASTNode *node);

#endif

