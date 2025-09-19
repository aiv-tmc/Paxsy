#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>
#include "../lexer/lexer.h"
#include "../error_manager/error_manager.h"

/* AST node types for different language constructs */
typedef enum {
    AST_VARIABLE_DECLARATION,
    AST_BINARY_OPERATION,
    AST_UNARY_OPERATION,
    AST_LITERAL_VALUE,
    AST_IDENTIFIER,
    AST_VARIABLE,
    AST_POINTER_VARIABLE,
    AST_REFERENCE_VARIABLE,
    AST_DEREFERENCE,
    AST_ADDRESS_OF,
    AST_ASSIGNMENT,
    AST_COMPOUND_ASSIGNMENT,
    AST_BLOCK,
    AST_FUNCTION,
    AST_FUNCTION_CALL,
    AST_NULL,
    AST_NULLPTR,
    AST_IF_STATEMENT,
    AST_ELSE_STATEMENT,
    AST_RETURN,
    AST_FREE,
    AST_BREAK,
    AST_CONTINUE,
    AST_SIZEOF,
    AST_PARSEOF,
    AST_REALLOC,
    AST_ALLOC,
    AST_STACK,
    AST_PUSH,
    AST_POP,
    AST_STRUCT_DECLARATION,
    AST_STRUCT_INSTANCE,
    AST_CAST,
    AST_SYSCALL,
    AST_CPU,
    AST_MULTI_DECLARATION,
    AST_MULTI_ASSIGNMENT,
    AST_ARRAY_INITIALIZER,
    AST_ARRAY_ACCESS,
    AST_LABEL_DECLARATION,
    AST_JUMP,
    AST_VA_START,
    AST_VA_ARG,
    AST_VA_END,
    AST_POSTFIX_CAST,
    AST_POSTFIX_INCREMENT,
    AST_POSTFIX_DECREMENT,
    AST_FUNCTION_CALL_STATEMENT,
    AST_ORG,
    AST_USE_OPTION,
    AST_USE_MULTI,
    AST_STRUCT_OBJECT_DECLARATION,
    AST_STRUCT_OBJECT_CALL,
    AST_FIELD_ACCESS,
    AST_DOUBLE_COLON,
    AST_TYPE_COUNT // Keep last for counting
} ASTNodeType;

/* Type specification for variables and functions */
typedef struct Type {
    char *name;
    uint8_t left_number;
    uint8_t right_number;
    char *right_id;
    uint8_t has_right_id;
    uint16_t array_size;
    char **modifiers;
    uint8_t modifier_count;
    uint8_t pointer_level;
    uint8_t is_reference;
    uint8_t is_array;
} Type;

/* Abstract Syntax Tree node structure */
typedef struct ASTNode {
    ASTNodeType type;
    TokenType operation_type;
    char *value;
    struct ASTNode *left;
    struct ASTNode *right;
    struct ASTNode *extra;
    Type *variable_type;
    Type *return_type;
    TokenType declaration_sigil;
    uint8_t is_variadic;
} ASTNode;

/* Abstract Syntax Tree container */
typedef struct AST {
    ASTNode **nodes;
    uint16_t count;
    uint16_t capacity;
} AST;

/* Parser state tracking */
typedef struct ParserState {
    int current_token_position;
    Token *token_stream;
    int total_tokens;
} ParserState;

/* Public interface functions */
AST *parse(Token *tokens, int token_count);
void free_ast(AST *ast);
void print_ast(AST *ast);
void free_type(Type *type);
void free_ast_node(ASTNode *node);

#endif

