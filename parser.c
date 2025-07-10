#include "parser.h"
#include "error_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int current_token = 0;
static Token* tokens = NULL;
static int token_count = 0;

static void advance() {
    if (current_token < token_count - 1) current_token++;
}

static Token peek() {
    if (current_token < token_count - 1) return tokens[current_token + 1];
    return tokens[token_count - 1]; // Return last token (EOF)
}

static bool match(TokenType type) {
    return tokens[current_token].type == type;
}

static bool match_next(TokenType type) {
    return peek().type == type;
}

static void consume(TokenType expected, const char* error_message) {
    if (match(expected)) {
        advance();
    } else {
        report_error(tokens[current_token].line, 
                    tokens[current_token].column,
                    "Expected %s, got %s",
                    token_names[expected],
                    token_names[tokens[current_token].type]);
    }
}

static ASTNode* parse_variable_declaration() {
    // Pattern: $identifier : type ;
    ASTVariableDecl* var_decl = malloc(sizeof(ASTVariableDecl));
    
    // Parse variable name
    consume(TOKEN_DOLLAR, "Expected '$' for variable declaration");
    if (!match(TOKEN_ID)) {
        report_error(tokens[current_token].line,
                    tokens[current_token].column,
                    "Expected identifier after '$'");
        free(var_decl);
        return NULL;
    }
    var_decl->name = strdup(tokens[current_token].value);
    advance();
    
    // Parse colon
    consume(TOKEN_COLON, "Expected ':' after variable name");
    
    // Parse type
    if (!match(TOKEN_TYPE)) {
        report_error(tokens[current_token].line,
                    tokens[current_token].column,
                    "Expected type after ':'");
        free(var_decl->name);
        free(var_decl);
        return NULL;
    }
    var_decl->var_type = strdup(tokens[current_token].value);
    advance();
    
    // Parse semicolon
    consume(TOKEN_SEMICOLON, "Expected ';' after variable declaration");
    
    // Create AST node
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = AST_VARIABLE_DECL;
    node->data = var_decl;
    node->next = NULL;
    
    return node;
}

static ASTNode* parse_statement() {
    if (match(TOKEN_DOLLAR)) {
        return parse_variable_declaration();
    }
    
    // Handle other statement types here in future
    report_error(tokens[current_token].line,
                tokens[current_token].column,
                "Unexpected token: %s",
                token_names[tokens[current_token].type]);
    advance();
    return NULL;
}

ASTNode* parse(Token* input_tokens, int input_token_count) {
    tokens = input_tokens;
    token_count = input_token_count;
    current_token = 0;
    
    ASTNode* program = malloc(sizeof(ASTNode));
    program->type = AST_PROGRAM;
    program->data = NULL;
    program->next = NULL;
    
    ASTNode* current = program;
    
    while (current_token < token_count - 1) { // Skip EOF
        if (match(TOKEN_EOF)) break;
        
        ASTNode* stmt = parse_statement();
        if (stmt) {
            current->next = stmt;
            current = stmt;
        }
    }
    
    return program;
}

void print_ast(ASTNode* ast) {
    ASTNode* current = ast->next; // Skip program node
    while (current) {
        switch (current->type) {
            case AST_VARIABLE_DECL: {
                ASTVariableDecl* var = (ASTVariableDecl*)current->data;
                printf("Variable: $%s : %s;\n", var->name, var->var_type);
                break;
            }
            default:
                printf("Unknown AST node type\n");
        }
        current = current->next;
    }
}

void free_ast(ASTNode* ast) {
    ASTNode* current = ast;
    while (current) {
        ASTNode* next = current->next;
        
        if (current->data) {
            switch (current->type) {
                case AST_VARIABLE_DECL: {
                    ASTVariableDecl* var = (ASTVariableDecl*)current->data;
                    free(var->name);
                    free(var->var_type);
                    free(var);
                    break;
                }
                case AST_PROGRAM:
                    // No data to free
                    break;
            }
        }
        
        free(current);
        current = next;
    }
}
