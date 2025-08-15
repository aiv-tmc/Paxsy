// parser.c
#include "parser.h"
#include "../lexer/lexer.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Global parser state
static int current_token_index = 0;
static Token *tokens = NULL;
static int token_count = 0;

// Forward declarations
static TokenType current_token_type(void);
static void advance(void);
static void expect(TokenType expected_type);
static Token *current_token(void);
static void error(const char *message);
static ASTNode *parse_statement(void);
static ASTNode *parse_expression(void);
static Type *parse_type_spec(void);

// Token handling utilities
#define MATCH(token) (current_token_type() == (token))
#define CONSUME(token) do { expect(token); } while(0)
#define TRY_CONSUME(token) (MATCH(token) ? (advance(), 1) : 0)

// Get current token type
static TokenType current_token_type(void) {
    Token *token = current_token();
    return token ? token->type : TOKEN_EOF;
}

// Advance to next token
static void advance(void) {
    if (current_token_index < token_count - 1) current_token_index++;
}

// Get current token
static Token *current_token(void) {
    if (current_token_index < token_count) return &tokens[current_token_index];
    return NULL;
}

// Report error and exit
static void error(const char *message) {
    if (current_token_index < token_count) {
        Token *token = &tokens[current_token_index];
        fprintf(stderr, "ERROR: %d:%d: %s\n", token->line, token->column, message);
    } else {
        fprintf(stderr, "ERROR: %s\n", message);
    }
    exit(EXIT_FAILURE);
}

// Expect specific token type
static void expect(TokenType expected_type) {
    if (MATCH(expected_type)) {
        advance();
        return;
    }
    
    TokenType actual = current_token_type();
    const char *expected_name = token_names[expected_type];
    const char *actual_name = (actual == TOKEN_EOF) ? "EOF" : token_names[actual];
    
    fprintf(stderr, "Expected %s but got %s\n", expected_name, actual_name);
    error("Unexpected token");
}

// Parse type specification with optional base and size
static Type *parse_type_spec(void) {
    Type *type = malloc(sizeof(Type));
    if (!type) error("Memory allocation failed");
    
    type->base = 0;
    type->size = 0;
    type->is_pointer = false;
    type->is_reference = false;
    
    // Parse optional base
    if (current_token_type() == TOKEN_NUMBER) {
        Token *base_token = current_token();
        char *endptr;
        type->base = (int)strtol(base_token->value, &endptr, 10);
        if (*endptr != '\0') {
            error("Invalid base number");
        }
        if (type->base < 2 || type->base > 36) {
            error("Base must be between 2 and 36");
        }
        advance();
    }
    
    // Parse pointer/reference modifiers
    while (MATCH(TOKEN_STAR) || MATCH(TOKEN_AMPERSAND)) {
        if (MATCH(TOKEN_STAR)) {
            type->is_pointer = true;
        } else if (MATCH(TOKEN_AMPERSAND)) {
            type->is_reference = true;
        }
        advance();
    }
    
    // Parse type name
    if (current_token_type() != TOKEN_TYPE) {
        error("Expected type specifier");
    }
    Token *type_token = current_token();
    type->name = strdup(type_token->value);
    advance();
    
    // Handle void type - only valid for function return types
    if (strcmp(type->name, "void") == 0) {
        return type;
    }
    
    // Parse optional size
    if (current_token_type() == TOKEN_NUMBER) {
        Token *size_token = current_token();
        char *endptr;
        type->size = (int)strtol(size_token->value, &endptr, 10);
        if (*endptr != '\0') {
            error("Invalid size number");
        }
        advance();
    }
    
    // Set default sizes if not specified
    if (type->size == 0) {
        if (strcmp(type->name, "char") == 0) type->size = 8;
        else if (strcmp(type->name, "int") == 0) type->size = 16;
        else if (strcmp(type->name, "real") == 0) type->size = 32;
    }
    
    return type;
}

// Create AST node
static ASTNode *create_ast_node(ASTNodeType type, TokenType op_type, 
    char *value, ASTNode *left, ASTNode *right, ASTNode *extra) {
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node) error("Memory allocation failed");
    
    node->type = type;
    node->op_type = op_type;
    node->value = value ? strdup(value) : NULL;
    node->left = left;
    node->right = right;
    node->extra = extra;
    node->var_type = NULL;
    node->return_type = NULL;
    node->decl_sigil = TOKEN_ERROR;
    return node;
}

// Add node to AST
static void add_ast_node(AST *ast, ASTNode *node) {
    if (ast->count >= ast->capacity) {
        ast->capacity = ast->capacity ? ast->capacity * 2 : 4;
        ast->nodes = realloc(ast->nodes, ast->capacity * sizeof(ASTNode*));
        if (!ast->nodes) error("Memory allocation failed");
    }
    ast->nodes[ast->count++] = node;
}

// Expression parsing functions
static ASTNode *parse_assignment(void);
static ASTNode *parse_bitwise_or(void);
static ASTNode *parse_bitwise_xor(void);
static ASTNode *parse_bitwise_and(void);
static ASTNode *parse_equality(void);
static ASTNode *parse_relational(void);
static ASTNode *parse_shift(void);
static ASTNode *parse_additive(void);
static ASTNode *parse_multiplicative(void);
static ASTNode *parse_exponent(void);
static ASTNode *parse_unary(void);
static ASTNode *parse_primary(void);

// Parse code block (single or multi-line)
static ASTNode *parse_block(void) {
    if (MATCH(TOKEN_LCURLY)) {
        CONSUME(TOKEN_LCURLY);
        ASTNode *block_node = create_ast_node(AST_BLOCK, 0, NULL, NULL, NULL, NULL);
        AST *block_ast = malloc(sizeof(AST));
        if (!block_ast) error("Memory allocation failed");
        block_ast->nodes = NULL;
        block_ast->count = 0;
        block_ast->capacity = 0;
        
        while (!MATCH(TOKEN_RCURLY) && !MATCH(TOKEN_EOF)) {
            ASTNode *stmt = parse_statement();
            add_ast_node(block_ast, stmt);
        }
        CONSUME(TOKEN_RCURLY);
        
        block_node->extra = (ASTNode*)block_ast;
        return block_node;
    }
    
    // Single statement block
    ASTNode *single_stmt = parse_statement();
    return create_ast_node(AST_BLOCK, 0, NULL, single_stmt, NULL, NULL);
}

// Print AST node recursively
void print_ast_node(ASTNode *node, int indent) {
    if (!node) return;
    
    for (int i = 0; i < indent; i++) printf("  ");
    
    switch (node->type) {
        case AST_VAR_DECL:
            printf("VariableDecl:\n");
            for (int i = 0; i < indent+1; i++) printf("  ");
            printf("sigil: %c\n", 
                node->decl_sigil == TOKEN_DOLLAR ? '$' : 
                node->decl_sigil == TOKEN_AT ? '@' : '&');
            for (int i = 0; i < indent+1; i++) printf("  ");
            printf("name: %s\n", node->value);
            if (node->var_type) {
                for (int i = 0; i < indent+1; i++) printf("  ");
                printf("type: %s", node->var_type->name);
                if (node->var_type->base > 0) printf(" (base %d)", node->var_type->base);
                if (node->var_type->size > 0) printf(" (size %d)", node->var_type->size);
                if (node->var_type->is_pointer) printf(" (pointer)");
                if (node->var_type->is_reference) printf(" (reference)");
                printf("\n");
            }
            if (node->left) {
                for (int i = 0; i < indent+1; i++) printf("  ");
                printf("initializer:\n");
                print_ast_node(node->left, indent+2);
            }
            break;
            
        case AST_BINARY_OP:
            printf("BinaryOp: %s\n", token_names[node->op_type]);
            print_ast_node(node->left, indent + 1);
            print_ast_node(node->right, indent + 1);
            break;
            
        case AST_UNARY_OP:
            printf("UnaryOp: %s\n", token_names[node->op_type]);
            print_ast_node(node->right, indent + 1);
            break;
            
        case AST_LITERAL:
            printf("Literal(%s): %s\n", token_names[node->op_type], node->value);
            break;
            
        case AST_ID:
            printf("ID: %s\n", node->value);
            break;
        
        case AST_VAR:
            printf("Variable: $%s\n", node->value);
            break;
        
        case AST_PTR_VAR:
            printf("Pointer: @%s\n", node->value);
            break;
            
        case AST_REF_VAR:
            printf("Reference: &%s\n", node->value);
            break;
            
        case AST_DEREF:
            printf("Dereference: $");
            print_ast_node(node->right, 0);
            printf("\n");
            break;
            
        case AST_ADDR:
            printf("Address-of: &");
            print_ast_node(node->right, 0);
            printf("\n");
            break;
            
        case AST_ASSIGNMENT:
            printf("Assignment: %s\n", token_names[node->op_type]);
            print_ast_node(node->left, indent + 1);
            print_ast_node(node->right, indent + 1);
            break;
            
        case AST_COMPOUND_ASSIGN:
            printf("Compound Assignment: %s\n", token_names[node->op_type]);
            print_ast_node(node->left, indent + 1);
            print_ast_node(node->right, indent + 1);
            break;
            
        case AST_BLOCK:
            printf("Block\n");
            if (node->extra) {
                AST *block_ast = (AST*)node->extra;
                for (int i = 0; i < block_ast->count; i++) {
                    print_ast_node(block_ast->nodes[i], indent + 1);
                }
            } else {
                print_ast_node(node->left, indent + 1);
            }
            break;
            
        case AST_FUNC:
            printf("Function: %s", node->value);
            if (node->return_type) {
                printf(" -> %s", node->return_type->name);
                if (node->return_type->base > 0) printf("%d", node->return_type->base);
                if (node->return_type->size > 0) printf("%d", node->return_type->size);
            }
            printf("\n");
            print_ast_node(node->left, indent + 1);  // Arguments
            print_ast_node(node->right, indent + 1); // Body
            break;
            
        case AST_STARTFUNC:
            printf("Start Function: %s", node->value);
            if (node->return_type) {
                printf(" -> %s", node->return_type->name);
                if (node->return_type->base > 0) printf("%d", node->return_type->base);
                if (node->return_type->size > 0) printf("%d", node->return_type->size);
            }
            printf("\n");
            print_ast_node(node->left, indent + 1);  // Arguments
            print_ast_node(node->right, indent + 1); // Body
            break;
            
        case AST_FUNC_CALL:
            printf("Function Call: _%s\n", node->value);
            print_ast_node(node->left, indent + 1);  // Arguments
            break;

        case AST_STARTFUNC_CALL:
            printf("Start Function Call: __%s\n", node->value);
            print_ast_node(node->left, indent + 1);  // Arguments
            break;

        case AST_NONE:
            printf("NONE\n");
            print_ast_node(node->left, indent + 1);
            break;

        case AST_NULL:
            printf("NULL\n");
            print_ast_node(node->left, indent + 1);
            break;
               
        case AST_IF:
            printf("If\n");
            print_ast_node(node->left, indent + 1);   // Condition
            print_ast_node(node->right, indent + 1);  // If block
            if (node->extra) {
                for (int i = 0; i < indent; i++) printf("  ");
                printf("Else:\n");
                print_ast_node(node->extra, indent + 1);
            }
            break;

        case AST_ELSE:
            printf("Else\n");
            print_ast_node(node->left, indent + 1);
            break;
            
        case AST_RETURN:
            printf("Return\n");
            print_ast_node(node->left, indent + 1);
            break;

        case AST_FREE:
            printf("Free\n");
            print_ast_node(node->left, indent + 1);
            break;
            
        case AST_BREAK:
            printf("Break\n");
            break;
             
        case AST_CONTINUE:
            printf("Continue\n");
            break;
            
        case AST_BUILD:
            printf("Build\n");
            print_ast_node(node->left, indent + 1);  // Arguments
            print_ast_node(node->right, indent + 1); // Body
            break;

        case AST_SIZE:
            printf("Size\n");
            print_ast_node(node->left, indent + 1);
            break;

        case AST_PARSER:
            printf("Parser\n");
            print_ast_node(node->left, indent + 1);
            break;
            
        case AST_REALLOC:
            printf("Realloc\n");
            print_ast_node(node->left, indent + 1);
            break;

        case AST_ALLOC:
            printf("Alloc\n");
            print_ast_node(node->left, indent + 1);
            break;

        case AST_STACK:
            printf("Stack\n");
            break;

        case AST_PUSH:
            printf("Push\n");
            print_ast_node(node->left, indent + 1);
            break;

        case AST_POP:
            printf("Pop\n");
            print_ast_node(node->left, indent + 1);
            break;
            
        case AST_STRUCT_DECL:
            printf("StructDecl: ~%s\n", node->value);
            print_ast_node(node->right, indent + 1);  // Body
            break;
            
        case AST_STRUCT_INST:
            printf("StructInst: ~%s\n", node->value);
            break;
            
        case AST_CAST:
            printf("Cast to: %s", node->var_type->name);
            if (node->var_type->base > 0) printf("%d", node->var_type->base);
            if (node->var_type->size > 0) printf("%d", node->var_type->size);
            printf("\n");
            print_ast_node(node->left, indent + 1);
            break;
            
        case AST_SYSCALL:
            printf("Syscall: %s\n", node->value);
            break;
            
        // Handle self and public qualifiers
        case AST_SELF:
            printf("Self Qualified Variable:\n");
            print_ast_node(node->left, indent + 1);
            break;
            
        case AST_PUBLIC:
            printf("Public Qualified Variable:\n");
            print_ast_node(node->left, indent + 1);
            break;
    }
}

// Parse if statement
static ASTNode *parse_if_statement(void) {
    CONSUME(TOKEN_IF);
    ASTNode *cond = parse_expression();
    ASTNode *if_block = parse_block();
    ASTNode *else_block = NULL;
    
    if (TRY_CONSUME(TOKEN_ELSE)) {
        else_block = parse_block();
    }
    
    return create_ast_node(AST_IF, 0, NULL, cond, if_block, else_block);
}

// Parse syscall statement: syscall::0x...;
static ASTNode *parse_syscall(void) {
    CONSUME(TOKEN_SYSCALL); // Consume 'syscall' keyword
    expect(TOKEN_DOUBLE_COLON); // Expect '::'
    
    // Validate and parse syscall number
    if (!MATCH(TOKEN_NUMBER)) {
        error("Expected syscall number after '::'");
    }
    
    Token *num_token = current_token();
    char *syscall_num = strdup(num_token->value);
    advance(); // Consume number token
    CONSUME(TOKEN_SEMICOLON); // Consume semicolon
    
    return create_ast_node(AST_SYSCALL, 0, syscall_num, NULL, NULL, NULL);
}

// Parse function declaration
static ASTNode *parse_function(void) {
    bool is_start = TRY_CONSUME(TOKEN_DOUBLE_UNDERSCORE);
    
    if (!MATCH(TOKEN_ID)) error("Expected function name");
    Token *token = current_token();
    char *func_name = strdup(token->value);
    advance();
    
    // Parse return type if exists
    Type *return_type = NULL;
    if (TRY_CONSUME(TOKEN_COLON)) {
        return_type = parse_type_spec();
    }
    
    // Parse arguments
    ASTNode *args = NULL;
    // Check if arguments exist (with or without parentheses)
    if (MATCH(TOKEN_LPAREN)) {
        CONSUME(TOKEN_LPAREN);
        if (!MATCH(TOKEN_RPAREN)) {
            args = parse_expression();
        }
        CONSUME(TOKEN_RPAREN);
    } else if (MATCH(TOKEN_ID) || MATCH(TOKEN_DOLLAR) || MATCH(TOKEN_AT) || MATCH(TOKEN_AMPERSAND)) {
        // Arguments without parentheses
        args = parse_expression();
    }
    
    // Parse function body
    ASTNode *body = NULL;
    if (MATCH(TOKEN_LCURLY)) {
        // Block body with curly braces
        body = parse_block();
    } else if (TRY_CONSUME(TOKEN_COLON)) {
        // Single-statement body with colon
        ASTNode *stmt = parse_statement();
        body = create_ast_node(AST_BLOCK, 0, NULL, stmt, NULL, NULL);
    } else {
        error("Expected '{' or ':' for function body");
    }
    
    ASTNode *node;
    if (is_start) {
        node = create_ast_node(AST_STARTFUNC, 0, func_name, args, body, NULL);
    } else {
        node = create_ast_node(AST_FUNC, 0, func_name, args, body, NULL);
    }
    node->return_type = return_type;
    return node;
}

// Parse return statement
static ASTNode *parse_return(void) {
    CONSUME(TOKEN_RETURN);
    ASTNode *expr = MATCH(TOKEN_SEMICOLON) ? NULL : parse_expression();
    CONSUME(TOKEN_SEMICOLON);
    return create_ast_node(AST_RETURN, 0, NULL, expr, NULL, NULL);
}

// Parse break statement
static ASTNode *parse_break(void) {
    CONSUME(TOKEN_BREAK);
    CONSUME(TOKEN_SEMICOLON);
    return create_ast_node(AST_BREAK, 0, NULL, NULL, NULL, NULL);
}

// Parse continue statement
static ASTNode *parse_continue(void) {
    CONSUME(TOKEN_CONTINUE);
    CONSUME(TOKEN_SEMICOLON);
    return create_ast_node(AST_CONTINUE, 0, NULL, NULL, NULL, NULL);
}

// Parse free statement
static ASTNode *parse_free(void) {
    CONSUME(TOKEN_FREE);
    CONSUME(TOKEN_LPAREN);
    ASTNode *expr = parse_expression();
    CONSUME(TOKEN_RPAREN);
    CONSUME(TOKEN_SEMICOLON);
    return create_ast_node(AST_FREE, 0, NULL, expr, NULL, NULL);
}

// Expression parsing entry point
static ASTNode *parse_expression(void) {
    return parse_assignment();
}

// Parse assignment expressions
static ASTNode *parse_assignment(void) {
    ASTNode *left = parse_bitwise_or();
    
    if (MATCH(TOKEN_EQUAL) || 
        MATCH(TOKEN_PLUS_EQ) || MATCH(TOKEN_MINUS_EQ) ||
        MATCH(TOKEN_STAR_EQ) || MATCH(TOKEN_SLASH_EQ) ||
        MATCH(TOKEN_PERCENT_EQ) || MATCH(TOKEN_DOUBLE_STAR_EQ) ||
        MATCH(TOKEN_PIPE_EQ) || MATCH(TOKEN_AMPERSAND_EQ) ||
        MATCH(TOKEN_CARET_EQ) || MATCH(TOKEN_TILDE_EQ) ||
        MATCH(TOKEN_SHL_EQ) || MATCH(TOKEN_SHR_EQ) ||
        MATCH(TOKEN_SAL_EQ) || MATCH(TOKEN_SAR_EQ) ||
        MATCH(TOKEN_ROL_EQ) || MATCH(TOKEN_ROR_EQ)) {
        
        TokenType op = current_token_type();
        advance();
        ASTNode *right = parse_assignment();
        
        if (op == TOKEN_EQUAL) {
            return create_ast_node(AST_ASSIGNMENT, op, NULL, left, right, NULL);
        }
        return create_ast_node(AST_COMPOUND_ASSIGN, op, NULL, left, right, NULL);
    }
    return left;
}

// Parse bitwise OR
static ASTNode *parse_bitwise_or(void) {
    ASTNode *node = parse_bitwise_xor();
    while (MATCH(TOKEN_PIPE)) {
        TokenType op = current_token_type();
        advance();
        ASTNode *right = parse_bitwise_xor();
        node = create_ast_node(AST_BINARY_OP, op, NULL, node, right, NULL);
    }
    return node;
}

// Parse bitwise XOR
static ASTNode *parse_bitwise_xor(void) {
    ASTNode *node = parse_bitwise_and();
    while (MATCH(TOKEN_CARET)) {
        TokenType op = current_token_type();
        advance();
        ASTNode *right = parse_bitwise_and();
        node = create_ast_node(AST_BINARY_OP, op, NULL, node, right, NULL);
    }
    return node;
}

// Parse bitwise AND
static ASTNode *parse_bitwise_and(void) {
    ASTNode *node = parse_equality();
    while (MATCH(TOKEN_AMPERSAND)) {
        TokenType op = current_token_type();
        advance();
        ASTNode *right = parse_equality();
        node = create_ast_node(AST_BINARY_OP, op, NULL, node, right, NULL);
    }
    return node;
}

// Parse equality operators (==, !=)
static ASTNode *parse_equality(void) {
    ASTNode *node = parse_relational();
    while (MATCH(TOKEN_DOUBLE_EQ) || MATCH(TOKEN_NE)) {
        TokenType op = current_token_type();
        advance();
        ASTNode *right = parse_relational();
        node = create_ast_node(AST_BINARY_OP, op, NULL, node, right, NULL);
    }
    return node;
}

// Parse relational operators (<, >, <=, >=)
static ASTNode *parse_relational(void) {
    ASTNode *node = parse_shift();
    while (MATCH(TOKEN_LT) || MATCH(TOKEN_GT) || MATCH(TOKEN_LE) || MATCH(TOKEN_GE)) {
        TokenType op = current_token_type();
        advance();
        ASTNode *right = parse_shift();
        node = create_ast_node(AST_BINARY_OP, op, NULL, node, right, NULL);
    }
    return node;
}

// Parse shift operators
static ASTNode *parse_shift(void) {
    ASTNode *node = parse_additive();
    while (MATCH(TOKEN_SHL) || MATCH(TOKEN_SHR) || 
           MATCH(TOKEN_SAL) || MATCH(TOKEN_SAR) || 
           MATCH(TOKEN_ROL) || MATCH(TOKEN_ROR)) {
        TokenType op = current_token_type();
        advance();
        ASTNode *right = parse_additive();
        node = create_ast_node(AST_BINARY_OP, op, NULL, node, right, NULL);
    }
    return node;
}

// Parse additive operators (+, -)
static ASTNode *parse_additive(void) {
    ASTNode *node = parse_multiplicative();
    while (MATCH(TOKEN_PLUS) || MATCH(TOKEN_MINUS)) {
        TokenType op = current_token_type();
        advance();
        ASTNode *right = parse_multiplicative();
        node = create_ast_node(AST_BINARY_OP, op, NULL, node, right, NULL);
    }
    return node;
}

// Parse multiplicative operators (*, /, %)
static ASTNode *parse_multiplicative(void) {
    ASTNode *node = parse_exponent();
    while (MATCH(TOKEN_STAR) || MATCH(TOKEN_SLASH) || MATCH(TOKEN_PERCENT)) {
        TokenType op = current_token_type();
        advance();
        ASTNode *right = parse_exponent();
        node = create_ast_node(AST_BINARY_OP, op, NULL, node, right, NULL);
    }
    return node;
}

// Parse exponent operator (**)
static ASTNode *parse_exponent(void) {
    ASTNode *node = parse_unary();
    while (MATCH(TOKEN_DOUBLE_STAR)) {
        TokenType op = current_token_type();
        advance();
        ASTNode *right = parse_unary();
        node = create_ast_node(AST_BINARY_OP, op, NULL, node, right, NULL);
    }
    return node;
}

// Parse unary operators
static ASTNode *parse_unary(void) {
    if (MATCH(TOKEN_PLUS) || MATCH(TOKEN_MINUS) || 
        MATCH(TOKEN_BANG) || MATCH(TOKEN_TILDE) || MATCH(TOKEN_AMPERSAND) || MATCH(TOKEN_DOLLAR)) {
        TokenType op = current_token_type();
        advance();
        ASTNode *operand = parse_unary();
        // Handle address-of and dereference
        if (op == TOKEN_AMPERSAND) {
            return create_ast_node(AST_ADDR, op, NULL, NULL, operand, NULL);
        } else if (op == TOKEN_DOLLAR) {
            return create_ast_node(AST_DEREF, op, NULL, NULL, operand, NULL);
        }
        return create_ast_node(AST_UNARY_OP, op, NULL, NULL, operand, NULL);
    }
    return parse_primary();
}

// Parse self and public qualifiers
static ASTNode *parse_qualifier(ASTNodeType qualifier_type) {
    advance(); // Consume qualifier token
    CONSUME(TOKEN_DOUBLE_COLON); // Expect '::'
    
    // Parse the variable or function call
    ASTNode *target = parse_primary();
    
    // Create qualifier node wrapping the target
    return create_ast_node(qualifier_type, 0, NULL, target, NULL, NULL);
}

// Parse primary expressions
static ASTNode *parse_primary(void) {
    Token *t = current_token();
    if (!t) error("Unexpected end of input");
    
    switch (t->type) {
        case TOKEN_SELF:    // Handle self qualifier
            return parse_qualifier(AST_SELF);
            
        case TOKEN_PUBLIC:  // Handle public qualifier
            return parse_qualifier(AST_PUBLIC);
            
        case TOKEN_AT: {  // Pointer variable
            advance(); // Consume '@'
            Token *id_token = current_token();
            if (id_token->type != TOKEN_ID) {
                error("Expected identifier after '@'");
            }
            char *var_name = strdup(id_token->value);
            advance();
            return create_ast_node(AST_PTR_VAR, 0, var_name, NULL, NULL, NULL);
        }
        case TOKEN_ID: {
            char *value = strdup(t->value);
            advance();
            
            // Handle function calls without parentheses
            if (value[0] == '_' && !MATCH(TOKEN_LPAREN)) {
                // Function call without arguments
                return create_ast_node(AST_FUNC_CALL, 0, value, NULL, NULL, NULL);
            }
            
            if (MATCH(TOKEN_LPAREN)) {
                CONSUME(TOKEN_LPAREN);
                ASTNode *args = NULL;
                if (!MATCH(TOKEN_RPAREN)) {
                    args = parse_expression();
                }
                CONSUME(TOKEN_RPAREN);
                return create_ast_node(AST_FUNC_CALL, 0, value, args, NULL, NULL);
            }
            return create_ast_node(AST_ID, TOKEN_ID, value, NULL, NULL, NULL);
        }
        case TOKEN_DOLLAR: {
            advance(); // Consume '$'
            Token *id_token = current_token();
            if (id_token->type != TOKEN_ID) {
                error("Expected identifier after '$'");
            }
            char *var_name = strdup(id_token->value);
            advance(); // Consume identifier
            
            return create_ast_node(AST_VAR, 0, var_name, NULL, NULL, NULL);
        }
        case TOKEN_LPAREN: {
            CONSUME(TOKEN_LPAREN);
            ASTNode *expr = parse_expression();
            CONSUME(TOKEN_RPAREN);
            return expr;
        }
        case TOKEN_ALLOC: {
            advance();
            CONSUME(TOKEN_LPAREN);
            ASTNode *args = parse_expression();
            CONSUME(TOKEN_RPAREN);
            return create_ast_node(AST_ALLOC, 0, "alloc", args, NULL, NULL);
        }
        case TOKEN_REALLOC: {
            advance();
            CONSUME(TOKEN_LPAREN);
            ASTNode *args = parse_expression();
            CONSUME(TOKEN_RPAREN);
            return create_ast_node(AST_REALLOC, 0, "realloc", args, NULL, NULL);
        }
        case TOKEN_POP: {
            advance();
            CONSUME(TOKEN_LPAREN);
            ASTNode *args = parse_expression();
            CONSUME(TOKEN_RPAREN);
            return create_ast_node(AST_POP, 0, "pop", args, NULL, NULL);
        }
        case TOKEN_PUSH: {
            advance();
            CONSUME(TOKEN_LPAREN);
            ASTNode *args = parse_expression();
            CONSUME(TOKEN_RPAREN);
            return create_ast_node(AST_PUSH, 0, "push", args, NULL, NULL);
        }
        case TOKEN_STACK: {
            advance();
            return create_ast_node(AST_STACK, 0, "stack", NULL, NULL, NULL);
        }
        case TOKEN_SIZE: {
            advance();
            CONSUME(TOKEN_LPAREN);
            ASTNode *args = parse_expression();
            CONSUME(TOKEN_RPAREN);
            return create_ast_node(AST_SIZE, 0, "size", args, NULL, NULL);
        }
        case TOKEN_PARSE: {
            advance();
            CONSUME(TOKEN_LPAREN);
            ASTNode *args = parse_expression();
            CONSUME(TOKEN_RPAREN);
            return create_ast_node(AST_PARSER, 0, "parse", args, NULL, NULL);
        }
        case TOKEN_NUMBER:
        case TOKEN_STRING:
        case TOKEN_CHAR: {
            char *value = strdup(t->value);
            TokenType type = t->type;
            advance();
            return create_ast_node(AST_LITERAL, type, value, NULL, NULL, NULL);
        }
        case TOKEN_TILDE: {  // Structure reference
            advance(); // Consume '~'
            Token *id_token = current_token();
            if (id_token->type != TOKEN_ID) {
                error("Expected structure name after '~'");
            }
            char *struct_name = strdup(id_token->value);
            advance();
            return create_ast_node(AST_STRUCT_INST, 0, struct_name, NULL, NULL, NULL);
        }
        case TOKEN_TYPE: {  // Type casting
            Type *cast_type = parse_type_spec();
            if (MATCH(TOKEN_LPAREN)) {
                CONSUME(TOKEN_LPAREN);
                ASTNode *expr = parse_expression();
                CONSUME(TOKEN_RPAREN);
                ASTNode *node = create_ast_node(AST_CAST, 0, NULL, expr, NULL, NULL);
                node->var_type = cast_type;
                return node;
            }
            error("Expected '(' for type cast");
            return NULL;
        }
        default:
            error("Unexpected token in expression");
            return NULL;
    }
}

// Parse variable declaration
static ASTNode *parse_variable_decl(void) {
    TokenType sigil = current_token_type();
    advance(); // Consume '$', '@', or '&'
    
    Token *id_token = current_token();
    if (!MATCH(TOKEN_ID)) error("Expected identifier");
    char *var_name = strdup(id_token->value);
    advance();
    
    CONSUME(TOKEN_COLON);
    
    // Parse type specification
    Type *var_type = parse_type_spec();
    
    // Disallow void variables
    if (strcmp(var_type->name, "void") == 0) {
        error("Cannot declare void variable");
    }
    
    // Handle initialization
    ASTNode *init = NULL;
    if (TRY_CONSUME(TOKEN_EQUAL)) {
        init = parse_expression();
    }
    
    CONSUME(TOKEN_SEMICOLON);
    
    ASTNode *node = create_ast_node(AST_VAR_DECL, 0, NULL, init, NULL, NULL);
    node->value = var_name;
    node->var_type = var_type;
    node->decl_sigil = sigil;
    return node;
}

// Parse build statement
static ASTNode *parse_build(void) {
    CONSUME(TOKEN_BUILD);
    CONSUME(TOKEN_LPAREN);
    ASTNode *args = NULL;
    if (!MATCH(TOKEN_RPAREN)) {
        args = parse_expression();
    }
    CONSUME(TOKEN_RPAREN);
    ASTNode *body = parse_block();
    return create_ast_node(AST_BUILD, 0, "build", args, body, NULL);
}

// Parse structure declaration
static ASTNode *parse_struct_decl(void) {
    CONSUME(TOKEN_TILDE);
    Token *id_token = current_token();
    if (id_token->type != TOKEN_ID) {
        error("Expected structure name after '~'");
    }
    char *struct_name = strdup(id_token->value);
    advance();
    
    ASTNode *body = parse_block();
    return create_ast_node(AST_STRUCT_DECL, 0, struct_name, NULL, body, NULL);
}

// Parse statements
static ASTNode *parse_statement(void) {
    Token *t = current_token();
    if (!t) error("Unexpected end of input");
    
    switch (t->type) {
        case TOKEN_DOLLAR:
        case TOKEN_AT:
        case TOKEN_AMPERSAND:
            return parse_variable_decl();
            
        case TOKEN_IF:
            return parse_if_statement();
            
        case TOKEN_SYSCALL:  // Handle syscall operator
            return parse_syscall();
            
        case TOKEN_DOUBLE_UNDERSCORE:
            return parse_function();
            
        case TOKEN_ID:
            // Handle regular functions (starting with underscore)
            if (t->value[0] == '_') {
                return parse_function();
            }
            // Fall through to default for other identifiers
        case TOKEN_RETURN:
            return parse_return();
            
        case TOKEN_BREAK:
            return parse_break();
            
        case TOKEN_CONTINUE:
            return parse_continue();
            
        case TOKEN_FREE:
            return parse_free();
            
        case TOKEN_BUILD:
            return parse_build();
            
        case TOKEN_TILDE:  // Structure declaration
            return parse_struct_decl();
            
        default: {
            ASTNode *expr = parse_expression();
            CONSUME(TOKEN_SEMICOLON);
            return expr;
        }
    }
}

// Main parsing function
AST *parse(Token *input_tokens, int input_token_count) {
    tokens = input_tokens;
    token_count = input_token_count;
    current_token_index = 0;

    // Handle empty input
    if (token_count == 0 || (token_count == 1 && tokens[0].type == TOKEN_EOF)) {
        AST *ast = malloc(sizeof(AST));
        if (!ast) error("Memory allocation failed");
        ast->nodes = NULL;
        ast->count = 0;
        ast->capacity = 0;
        return ast;
    }

    AST *ast = malloc(sizeof(AST));
    if (!ast) error("Memory allocation failed");
    ast->nodes = NULL;
    ast->count = 0;
    ast->capacity = 0;
    
    while (!MATCH(TOKEN_EOF)) {
        ASTNode *node = parse_statement();
        add_ast_node(ast, node);
    }
    
    return ast;
}

// Free type structure
static void free_type(Type *type) {
    if (!type) return;
    free(type->name);
    free(type);
}

// Free AST node recursively
void free_ast_node(ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_BLOCK:
            if (node->extra) {
                AST *block_ast = (AST*)node->extra;
                for (int i = 0; i < block_ast->count; i++) {
                    free_ast_node(block_ast->nodes[i]);
                }
                free(block_ast->nodes);
                free(block_ast);
            } else {
                free_ast_node(node->left);
            }
            break;
            
        case AST_IF:
            free_ast_node(node->left);
            free_ast_node(node->right);
            if (node->extra) {
                free_ast_node(node->extra);
            }
            break;

        case AST_FUNC:
        case AST_STARTFUNC:
        case AST_BUILD:
            free_ast_node(node->left);
            free_ast_node(node->right);
            break;
            
        case AST_FUNC_CALL:
        case AST_STARTFUNC_CALL:
        case AST_ALLOC:
        case AST_REALLOC:
        case AST_FREE:
        case AST_RETURN:
        case AST_POP:
        case AST_PUSH:
        case AST_SIZE:
        case AST_PARSER:
        case AST_STRUCT_DECL:
            free_ast_node(node->left);
            free_ast_node(node->right);
            break;
            
        // Handle self and public qualifiers
        case AST_SELF:
        case AST_PUBLIC:
            free_ast_node(node->left);
            break;
            
        default:
            free_ast_node(node->left);
            free_ast_node(node->right);
            free_ast_node(node->extra);
            break;
    }
    
    if (node->value) free(node->value);
    if (node->var_type) free_type(node->var_type);
    if (node->return_type) free_type(node->return_type);
    free(node);
}

// Print entire AST
void print_ast(AST *ast) {
    for (int i = 0; i < ast->count; i++) {
        printf("Statement %d:\n", i + 1);
        print_ast_node(ast->nodes[i], 1);
    }
}

// Free entire AST
void free_ast(AST *ast) {
    if (!ast) return;
    
    for (int i = 0; i < ast->count; i++) {
        free_ast_node(ast->nodes[i]);
    }
    free(ast->nodes);
    free(ast);
}

