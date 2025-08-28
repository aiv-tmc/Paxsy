#include "parser.h"
#include "../lexer/lexer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#define MAX_MODIFIERS 8
#define AST_INIT_CAPACITY 8

static int current_token_index = 0;
static Token *tokens = NULL;
static int token_count = 0;

static TokenType current_token_type(void);
static void advance(void);
static bool expect(TokenType expected_type);
static Token *current_token(void);
static ASTNode *parse_statement(void);
static ASTNode *parse_expression(void);
static Type *parse_type_spec(void);
static ASTNode *parse_parameter_list(bool *is_variadic);
static ASTNode *parse_array_initializer(void);
static void print_ast_node(ASTNode *node, int indent);

#define MATCH(token) (current_token_type() == (token))
#define CONSUME(token) do { if (!expect(token)) return NULL; } while(0)
#define TRY_CONSUME(token) (MATCH(token) ? (advance(), 1) : 0)

#define REPORT_ERROR(...) do { \
    Token *t = current_token(); \
    if (t) report_error(t->line, t->column, __VA_ARGS__); \
    else report_error(0, 0, __VA_ARGS__); \
} while(0)

static TokenType current_token_type(void) {
    Token *token = current_token();
    return token ? token->type : TOKEN_EOF;
}

static void advance(void) {
    if (current_token_index < token_count - 1) current_token_index++;
}

static Token *current_token(void) {
    if (current_token_index < token_count) return &tokens[current_token_index];
    return NULL;
}

static bool expect(TokenType expected_type) {
    if (MATCH(expected_type)) {
        advance();
        return true;
    }
    
    TokenType actual = current_token_type();
    const char *expected_name = token_names[expected_type];
    const char *actual_name = (actual == TOKEN_EOF) ? "EOF" : token_names[actual];
    
    REPORT_ERROR("Expected %s but got %s", expected_name, actual_name);
    return false;
}

static Type *parse_type_spec(void) {
    Type *type = malloc(sizeof(Type));
    if (!type) {
        REPORT_ERROR("Memory allocation failed for type specifier");
        return NULL;
    }
    
    type->is_pointer = false;
    type->is_reference = false;
    type->name = NULL;
    type->left_number = -1;
    type->right_number = -1;
    type->modifiers = NULL;
    type->modifier_count = 0;
    type->is_array = false;
    type->array_size = 0;

    while (MATCH(TOKEN_MODIFIER) && type->modifier_count < MAX_MODIFIERS) {
        Token *mod_token = current_token();
        type->modifiers = realloc(type->modifiers, (type->modifier_count + 1) * sizeof(char*));
        if (!type->modifiers) {
            REPORT_ERROR("Memory allocation failed for modifiers");
            free(type);
            return NULL;
        }
        type->modifiers[type->modifier_count++] = strdup(mod_token->value);
        advance();
    }

    while (MATCH(TOKEN_AT) || MATCH(TOKEN_AMPERSAND)) {
        if (MATCH(TOKEN_AT)) type->is_pointer = true;
        else if (MATCH(TOKEN_AMPERSAND)) type->is_reference = true;
        advance();
    }

    if (MATCH(TOKEN_NUMBER)) {
        Token *num_token = current_token();
        type->left_number = atoi(num_token->value);
        advance();
    }

    if (current_token_type() != TOKEN_TYPE) {
        REPORT_ERROR("Expected type specifier");
        free_type(type);
        return NULL;
    }
    
    Token *type_token = current_token();
    type->name = strdup(type_token->value);
    advance();

    if (TRY_CONSUME(TOKEN_COLON)) {
        if (MATCH(TOKEN_NUMBER)) {
            Token *num_token = current_token();
            type->right_number = atoi(num_token->value);
            advance();
        }
    }

    if (MATCH(TOKEN_LBRACKET)) {
        advance();
        type->is_array = true;
        
        if (MATCH(TOKEN_NUMBER)) {
            Token *num_token = current_token();
            type->array_size = atoi(num_token->value);
            advance();
        } else if (MATCH(TOKEN_QUESTION)) {
            type->array_size = -1;
            advance();
        } else {
            REPORT_ERROR("Expected number or '?' for array size");
            free_type(type);
            return NULL;
        }
        
        CONSUME(TOKEN_RBRACKET);
    }
    
    return type;
}

static ASTNode *create_ast_node(ASTNodeType type, TokenType op_type, 
    char *value, ASTNode *left, ASTNode *right, ASTNode *extra) {
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = type;
    node->op_type = op_type;
    node->value = value;
    node->left = left;
    node->right = right;
    node->extra = extra;
    node->var_type = NULL;
    node->return_type = NULL;
    node->decl_sigil = TOKEN_ERROR;
    node->is_variadic = false;
    
    return node;
}

static bool add_ast_node(AST *ast, ASTNode *node) {
    if (ast->count >= ast->capacity) {
        int new_capacity = ast->capacity ? ast->capacity * 2 : AST_INIT_CAPACITY;
        ASTNode **new_nodes = realloc(ast->nodes, new_capacity * sizeof(ASTNode*));
        if (!new_nodes) return false;
        ast->nodes = new_nodes;
        ast->capacity = new_capacity;
    }
    ast->nodes[ast->count++] = node;
    return true;
}

static ASTNode *parse_assignment(void);
static ASTNode *parse_logical_or(void);
static ASTNode *parse_logical_xor(void);
static ASTNode *parse_logical_and(void);
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

static ASTNode *parse_block(void) {
    if (MATCH(TOKEN_LCURLY)) {
        CONSUME(TOKEN_LCURLY);
        ASTNode *block_node = create_ast_node(AST_BLOCK, 0, NULL, NULL, NULL, NULL);
        if (!block_node) return NULL;
        
        AST *block_ast = malloc(sizeof(AST));
        if (!block_ast) {
            free_ast_node(block_node);
            return NULL;
        }
        block_ast->nodes = NULL;
        block_ast->count = 0;
        block_ast->capacity = 0;
        
        while (current_token_type() != TOKEN_RCURLY && current_token_type() != TOKEN_EOF) {
            ASTNode *stmt = parse_statement();
            if (stmt && !add_ast_node(block_ast, stmt)) {
                free_ast_node(stmt);
                free(block_ast);
                free_ast_node(block_node);
                return NULL;
            }
        }
        CONSUME(TOKEN_RCURLY);
        
        block_node->extra = (ASTNode*)block_ast;
        return block_node;
    } else if (MATCH(TOKEN_ARROW)) {
        advance();
        ASTNode *stmt = parse_statement();
        return create_ast_node(AST_BLOCK, 0, NULL, stmt, NULL, NULL);
    }
    
    ASTNode *single_stmt = parse_statement();
    return create_ast_node(AST_BLOCK, 0, NULL, single_stmt, NULL, NULL);
}

static ASTNode *parse_array_initializer(void) {
    if (MATCH(TOKEN_LCURLY)) {
        CONSUME(TOKEN_LCURLY);
        AST *list = malloc(sizeof(AST));
        if (!list) return NULL;
        list->nodes = NULL;
        list->count = 0;
        list->capacity = 0;

        while (!MATCH(TOKEN_RCURLY) && !MATCH(TOKEN_EOF)) {
            ASTNode *expr = parse_expression();
            if (!expr) {
                free_ast(list);
                return NULL;
            }
            if (!add_ast_node(list, expr)) {
                free_ast_node(expr);
                free_ast(list);
                return NULL;
            }
            if (MATCH(TOKEN_COMMA)) advance();
            else break;
        }
        CONSUME(TOKEN_RCURLY);
        return create_ast_node(AST_ARRAY_INIT, 0, NULL, NULL, NULL, (ASTNode*)list);
    } else if (MATCH(TOKEN_STRING)) {
        return parse_primary();
    } else {
        REPORT_ERROR("Expected '{' or string literal for array initializer");
        return NULL;
    }
}

static void print_ast_node(ASTNode *node, int indent) {
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
                printf("modifiers: ");
                for (int j = 0; j < node->var_type->modifier_count; j++) printf("%s ", node->var_type->modifiers[j]);
                printf("\n");
                if (node->var_type->is_pointer) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("PTR\n");
                }
                if (node->var_type->is_reference) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("REF\n");
                }
                if (node->var_type->left_number != -1) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("Base: %d\n", node->var_type->left_number);
                }
                for (int i = 0; i < indent+1; i++) printf("  ");
                printf("Type: %s\n", node->var_type->name);
                if (node->var_type->right_number != -1) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("Size: %d\n", node->var_type->right_number);
                }
                if (node->var_type->is_array) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("ArraySize: ");
                    if (node->var_type->array_size == -1) printf("?");
                    else printf("%d", node->var_type->array_size);
                    printf("\n");
                }
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
                for (int i = 0; i < block_ast->count; i++) print_ast_node(block_ast->nodes[i], indent + 1);
            } else print_ast_node(node->left, indent + 1);
            break;
                        
        case AST_FUNC:
            printf("Function: %s", node->value);
            if (node->return_type) {
                printf(" -> ");
                for (int j = 0; j < node->return_type->modifier_count; j++) printf("%s ", node->return_type->modifiers[j]);
                printf("%s", node->return_type->name);
            }
            if (node->is_variadic) printf(" (variadic)");
            if (!node->right) printf(" (prototype)");
            printf("\n");
            print_ast_node(node->left, indent + 1);
            print_ast_node(node->right, indent + 1);
            break;

        case AST_FUNC_CALL:
            printf("Function Call:%s\n", node->value);
            print_ast_node(node->left, indent + 1);
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
            print_ast_node(node->left, indent + 1);
            print_ast_node(node->right, indent + 1);
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
             
        case AST_BUILD:
            printf("Build\n");
            print_ast_node(node->left, indent + 1);
            print_ast_node(node->right, indent + 1);
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
            print_ast_node(node->right, indent + 1);
            break;
            
        case AST_STRUCT_INST:
            printf("StructInst: ~%s\n", node->value);
            break;
            
        case AST_CAST:
            printf("Cast to: ");
            for (int j = 0; j < node->var_type->modifier_count; j++) printf("%s ", node->var_type->modifiers[j]);
            printf("%s\n", node->var_type->name);
            print_ast_node(node->left, indent + 1);
            break;
            
        case AST_SYSCALL:
            printf("Syscall: %s\n", node->value);
            break;
            
        case AST_SELF:
            printf("Self Qualified Variable:\n");
            print_ast_node(node->left, indent + 1);
            break;
            
        case AST_PUBLIC:
            printf("Public Qualified Variable:\n");
            print_ast_node(node->left, indent + 1);
            break;

        case AST_MULTI_DECL:
            printf("MultiDecl\n");
            if (node->var_type) {
                for (int i = 0; i < indent+1; i++) printf("  ");
                printf("modifiers: ");
                for (int j = 0; j < node->var_type->modifier_count; j++) printf("%s ", node->var_type->modifiers[j]);
                printf("\n");
                if (node->var_type->is_pointer) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("AT\n");
                }
                if (node->var_type->is_reference) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("REF\n");
                }
                if (node->var_type->left_number != -1) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("Base: %d\n", node->var_type->left_number);
                }
                for (int i = 0; i < indent+1; i++) printf("  ");
                printf("Type: %s\n", node->var_type->name);
                if (node->var_type->right_number != -1) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("Size: %d\n", node->var_type->right_number);
                }
                if (node->var_type->is_array) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("ArraySize: ");
                    if (node->var_type->array_size == -1) printf("?");
                    else printf("%d", node->var_type->array_size);
                    printf("\n");
                }
            }
            if (node->left) {
                AST *var_list = (AST*)node->left;
                for (int i = 0; i < var_list->count; i++) {
                    print_ast_node(var_list->nodes[i], indent + 1);
                }
            }
            if (node->right) {
                AST *init_list = (AST*)node->right;
                for (int i = 0; i < init_list->count; i++) {
                    print_ast_node(init_list->nodes[i], indent + 1);
                }
            }
            break;

        case AST_MULTI_ASSIGN:
            printf("MultiAssign\n");
            if (node->left) {
                AST *lvalue_list = (AST*)node->left;
                for (int i = 0; i < lvalue_list->count; i++) {
                    print_ast_node(lvalue_list->nodes[i], indent + 1);
                }
            }
            if (node->right) {
                AST *rvalue_list = (AST*)node->right;
                for (int i = 0; i < rvalue_list->count; i++) {
                    print_ast_node(rvalue_list->nodes[i], indent + 1);
                }
            }
            break;

        case AST_ARRAY_INIT:
            printf("ArrayInitializer\n");
            if (node->extra) {
                AST *list = (AST*)node->extra;
                for (int i = 0; i < list->count; i++) print_ast_node(list->nodes[i], indent + 1);
            } else print_ast_node(node->left, indent + 1);
            break;
    }
}

static ASTNode *parse_if_statement(void) {
    CONSUME(TOKEN_IF);
    ASTNode *cond = parse_expression();
    if (!cond) return NULL;
    
    ASTNode *if_block = parse_block();
    if (!if_block) {
        free_ast_node(cond);
        return NULL;
    }
    
    ASTNode *else_block = NULL;
    if (TRY_CONSUME(TOKEN_ELSE)) else_block = parse_block();
    
    return create_ast_node(AST_IF, 0, NULL, cond, if_block, else_block);
}

static ASTNode *parse_syscall(void) {
    CONSUME(TOKEN_SYSCALL);
    expect(TOKEN_DOUBLE_COLON);
    
    if (!MATCH(TOKEN_NUMBER)) {
        REPORT_ERROR("Expected syscall number after '::'");
        return NULL;
    }
    
    Token *num_token = current_token();
    char *syscall_num = strdup(num_token->value);
    advance();
    CONSUME(TOKEN_SEMICOLON);
    
    return create_ast_node(AST_SYSCALL, 0, syscall_num, NULL, NULL, NULL);
}

static ASTNode *parse_parameter_list(bool *is_variadic) {
    AST *param_ast = malloc(sizeof(AST));
    if (!param_ast) return NULL;
    param_ast->nodes = NULL;
    param_ast->count = 0;
    param_ast->capacity = 0;
    
    *is_variadic = false;
    
    CONSUME(TOKEN_LPAREN);
    
    if (MATCH(TOKEN_RPAREN)) {
        CONSUME(TOKEN_RPAREN);
        return create_ast_node(AST_BLOCK, 0, NULL, NULL, NULL, (ASTNode*)param_ast);
    }
    
    while (current_token_type() != TOKEN_RPAREN && current_token_type() != TOKEN_EOF) {
        if (MATCH(TOKEN_ELLIPSIS)) {
            advance();
            *is_variadic = true;
            break;
        }
        
        Type *param_type = parse_type_spec();
        if (!param_type) {
            free_ast((AST*)param_ast);
            return NULL;
        }
        
        Token *id_token = current_token();
        if (id_token->type != TOKEN_ID) {
            REPORT_ERROR("Expected identifier for parameter name");
            free_type(param_type);
            free_ast((AST*)param_ast);
            return NULL;
        }
        char *param_name = strdup(id_token->value);
        advance();
        
        ASTNode *param_node = create_ast_node(AST_VAR_DECL, 0, param_name, NULL, NULL, NULL);
        if (!param_node) {
            free(param_name);
            free_type(param_type);
            free_ast((AST*)param_ast);
            return NULL;
        }
        param_node->var_type = param_type;
        param_node->decl_sigil = 0;
        
        if (!add_ast_node(param_ast, param_node)) {
            free_ast_node(param_node);
            free_ast((AST*)param_ast);
            return NULL;
        }
        
        if (MATCH(TOKEN_COMMA)) advance();
        else if (!MATCH(TOKEN_RPAREN) && !MATCH(TOKEN_ELLIPSIS)) {
            REPORT_ERROR("Expected ',' or ')' after parameter");
            free_ast((AST*)param_ast);
            return NULL;
        }
    }
    
    CONSUME(TOKEN_RPAREN);
    return create_ast_node(AST_BLOCK, 0, NULL, NULL, NULL, (ASTNode*)param_ast);
}

static ASTNode *parse_function(void) {
    if (!MATCH(TOKEN_ID)) {
        REPORT_ERROR("Expected function name");
        return NULL;
    }
    Token *token = current_token();
    char *func_name = strdup(token->value);
    advance();
    
    bool is_variadic = false;
    ASTNode *args = NULL;
    
    if (MATCH(TOKEN_LPAREN)) args = parse_parameter_list(&is_variadic);
    else {
        args = create_ast_node(AST_BLOCK, 0, NULL, NULL, NULL, NULL);
        if (args) {
            AST *empty_params = malloc(sizeof(AST));
            if (empty_params) {
                empty_params->nodes = NULL;
                empty_params->count = 0;
                empty_params->capacity = 0;
                args->extra = (ASTNode*)empty_params;
            }
        }
    }
    
    if (!args) {
        free(func_name);
        return NULL;
    }
    
    Type *return_type = NULL;
    if (TRY_CONSUME(TOKEN_COLON)) {
        return_type = parse_type_spec();
        if (!return_type) {
            free(func_name);
            free_ast_node(args);
            return NULL;
        }
    }
    
    ASTNode *body = NULL;
    if (MATCH(TOKEN_LCURLY)) body = parse_block();
    else if (TRY_CONSUME(TOKEN_COLON)) {
        ASTNode *stmt = parse_statement();
        body = create_ast_node(AST_BLOCK, 0, NULL, stmt, NULL, NULL);
    } else if (TRY_CONSUME(TOKEN_ARROW)) {
        ASTNode *stmt = parse_statement();
        body = create_ast_node(AST_BLOCK, 0, NULL, stmt, NULL, NULL);
    } else if (TRY_CONSUME(TOKEN_SEMICOLON)) {
    } else {
        REPORT_ERROR("Expected '{', ':', '->' or ';' for function body");
        free(func_name);
        free_type(return_type);
        free_ast_node(args);
        return NULL;
    }
    
    if (!body && current_token_type() != TOKEN_SEMICOLON) {
        REPORT_ERROR("Expected function body");
        free(func_name);
        free_type(return_type);
        free_ast_node(args);
        return NULL;
    }
    
    ASTNode *node = create_ast_node(AST_FUNC, 0, func_name, args, body, NULL);
    if (node) {
        node->return_type = return_type;
        node->is_variadic = is_variadic;
    } else {
        free(func_name);
        free_type(return_type);
        free_ast_node(args);
        free_ast_node(body);
    }
    
    return node;
}

static ASTNode *parse_return(void) {
    CONSUME(TOKEN_RETURN);
    ASTNode *expr = NULL;
    if (!MATCH(TOKEN_SEMICOLON)) expr = parse_expression();
    CONSUME(TOKEN_SEMICOLON);
    return create_ast_node(AST_RETURN, 0, NULL, expr, NULL, NULL);
}

static ASTNode *parse_break(void) {
    CONSUME(TOKEN_BREAK);
    CONSUME(TOKEN_SEMICOLON);
    return create_ast_node(AST_BREAK, 0, NULL, NULL, NULL, NULL);
}

static ASTNode *parse_free(void) {
    CONSUME(TOKEN_FREE);
    CONSUME(TOKEN_LPAREN);
    ASTNode *expr = parse_expression();
    if (!expr) return NULL;
    CONSUME(TOKEN_RPAREN);
    CONSUME(TOKEN_SEMICOLON);
    return create_ast_node(AST_FREE, 0, NULL, expr, NULL, NULL);
}

static ASTNode *parse_expression(void) {
    return parse_assignment();
}

static ASTNode *parse_assignment(void) {
    ASTNode *left = parse_logical_or();
    if (!left) return NULL;
    
    if (MATCH(TOKEN_EQUAL) || MATCH(TOKEN_PLUS_EQ) || MATCH(TOKEN_MINUS_EQ) ||
        MATCH(TOKEN_STAR_EQ) || MATCH(TOKEN_SLASH_EQ) || MATCH(TOKEN_PERCENT_EQ) ||
        MATCH(TOKEN_DOUBLE_STAR_EQ) || MATCH(TOKEN_PIPE_EQ) || MATCH(TOKEN_AMPERSAND_EQ) ||
        MATCH(TOKEN_CARET_EQ) || MATCH(TOKEN_TILDE_EQ) || MATCH(TOKEN_SHL_EQ) ||
        MATCH(TOKEN_SHR_EQ) || MATCH(TOKEN_SAL_EQ) || MATCH(TOKEN_SAR_EQ) ||
        MATCH(TOKEN_ROL_EQ) || MATCH(TOKEN_ROR_EQ)) {
        
        TokenType op = current_token_type();
        advance();
        ASTNode *right = parse_assignment();
        if (!right) {
            free_ast_node(left);
            return NULL;
        }
        
        if (op == TOKEN_EQUAL) {
            return create_ast_node(AST_ASSIGNMENT, op, NULL, left, right, NULL);
        }
        return create_ast_node(AST_COMPOUND_ASSIGN, op, NULL, left, right, NULL);
    }
    return left;
}

static ASTNode *parse_binary_op(ASTNode *(*next_parser)(void), TokenType op1, TokenType op2) {
    ASTNode *node = next_parser();
    if (!node) return NULL;
    
    while (MATCH(op1) || MATCH(op2)) {
        TokenType op = current_token_type();
        advance();
        ASTNode *right = next_parser();
        if (!right) {
            free_ast_node(node);
            return NULL;
        }
        node = create_ast_node(AST_BINARY_OP, op, NULL, node, right, NULL);
        if (!node) {
            free_ast_node(right);
            return NULL;
        }
    }
    return node;
}

static ASTNode *parse_logical_or(void) {
    return parse_binary_op(parse_logical_xor, TOKEN_DOUBLE_PIPE, TOKEN_ERROR);
}

static ASTNode *parse_logical_xor(void) {
    return parse_binary_op(parse_logical_and, TOKEN_DOUBLE_CARET, TOKEN_ERROR);
}

static ASTNode *parse_logical_and(void) {
    return parse_binary_op(parse_bitwise_or, TOKEN_DOUBLE_AMPERSAND, TOKEN_ERROR);
}

static ASTNode *parse_bitwise_or(void) {
    return parse_binary_op(parse_bitwise_xor, TOKEN_PIPE, TOKEN_ERROR);
}

static ASTNode *parse_bitwise_xor(void) {
    return parse_binary_op(parse_bitwise_and, TOKEN_CARET, TOKEN_ERROR);
}

static ASTNode *parse_bitwise_and(void) {
    return parse_binary_op(parse_equality, TOKEN_AMPERSAND, TOKEN_ERROR);
}

static ASTNode *parse_equality(void) {
    return parse_binary_op(parse_relational, TOKEN_DOUBLE_EQ, TOKEN_NE);
}

static ASTNode *parse_relational(void) {
    return parse_binary_op(parse_shift, TOKEN_LT, TOKEN_GT);
}

static ASTNode *parse_shift(void) {
    return parse_binary_op(parse_additive, TOKEN_SHL, TOKEN_SHR);
}

static ASTNode *parse_additive(void) {
    return parse_binary_op(parse_multiplicative, TOKEN_PLUS, TOKEN_MINUS);
}

static ASTNode *parse_multiplicative(void) {
    return parse_binary_op(parse_exponent, TOKEN_STAR, TOKEN_SLASH);
}

static ASTNode *parse_exponent(void) {
    return parse_binary_op(parse_unary, TOKEN_DOUBLE_STAR, TOKEN_ERROR);
}

static ASTNode *parse_unary(void) {
    if (MATCH(TOKEN_PLUS) || MATCH(TOKEN_MINUS) || MATCH(TOKEN_BANG) || 
        MATCH(TOKEN_TILDE) || MATCH(TOKEN_AMPERSAND) || MATCH(TOKEN_DOLLAR) ||
        MATCH(TOKEN_DOUBLE_PLUS) || MATCH(TOKEN_DOUBLE_MINUS)) {
        TokenType op = current_token_type();
        advance();
        ASTNode *operand = parse_unary();
        if (!operand) return NULL;
        return create_ast_node(AST_UNARY_OP, op, NULL, NULL, operand, NULL);
    }
    return parse_primary();
}

static ASTNode *parse_primary(void) {
    Token *t = current_token();
    if (!t) {
        REPORT_ERROR("Unexpected end of input");
        return NULL;
    }
    
    switch (t->type) {
        case TOKEN_SELF:
        case TOKEN_PUBLIC: {
            ASTNodeType qualifier_type = (t->type == TOKEN_SELF) ? AST_SELF : AST_PUBLIC;
            advance();
            CONSUME(TOKEN_DOUBLE_COLON);
            ASTNode *target = parse_primary();
            return create_ast_node(qualifier_type, 0, NULL, target, NULL, NULL);
        }
        case TOKEN_AT: {
            advance();
            Token *id_token = current_token();
            if (id_token->type != TOKEN_ID) {
                REPORT_ERROR("Expected identifier after '@'");
                return NULL;
            }
            char *var_name = strdup(id_token->value);
            advance();
            return create_ast_node(AST_PTR_VAR, 0, var_name, NULL, NULL, NULL);
        }
        case TOKEN_ID: {
            char *value = strdup(t->value);
            advance();
            
            if (value[0] == '_' && !MATCH(TOKEN_LPAREN)) {
                return create_ast_node(AST_FUNC_CALL, 0, value, NULL, NULL, NULL);
            }
            
            if (MATCH(TOKEN_LPAREN)) {
                CONSUME(TOKEN_LPAREN);
                ASTNode *args = NULL;
                if (!MATCH(TOKEN_RPAREN)) {
                    args = parse_expression();
                    if (!args) {
                        free(value);
                        return NULL;
                    }
                }
                CONSUME(TOKEN_RPAREN);
                return create_ast_node(AST_FUNC_CALL, 0, value, args, NULL, NULL);
            }
            return create_ast_node(AST_ID, 0, value, NULL, NULL, NULL);
        }
        case TOKEN_DOLLAR: {
            advance();
            Token *id_token = current_token();
            if (id_token->type != TOKEN_ID) {
                REPORT_ERROR("Expected identifier after '$'");
                return NULL;
            }
            char *var_name = strdup(id_token->value);
            advance();
            return create_ast_node(AST_VAR, 0, var_name, NULL, NULL, NULL);
        }
        case TOKEN_LPAREN: {
            CONSUME(TOKEN_LPAREN);
            ASTNode *expr = parse_expression();
            if (!expr) return NULL;
            CONSUME(TOKEN_RPAREN);
            return expr;
        }
        case TOKEN_ALLOC:
        case TOKEN_REALLOC:
        case TOKEN_PUSH:
        case TOKEN_SIZE:
        case TOKEN_PARSE: {
            ASTNodeType node_type;
            switch (t->type) {
                case TOKEN_ALLOC: node_type = AST_ALLOC; break;
                case TOKEN_REALLOC: node_type = AST_REALLOC; break;
                case TOKEN_PUSH: node_type = AST_PUSH; break;
                case TOKEN_SIZE: node_type = AST_SIZE; break;
                case TOKEN_PARSE: node_type = AST_PARSER; break;
                default: return NULL;
            }
            advance();
            CONSUME(TOKEN_LPAREN);
            ASTNode *args = parse_expression();
            if (!args) return NULL;
            CONSUME(TOKEN_RPAREN);
            return create_ast_node(node_type, 0, NULL, args, NULL, NULL);
        }
        case TOKEN_POP: {
            advance();
            return create_ast_node(AST_POP, 0, NULL, NULL, NULL, NULL);
        }
        case TOKEN_STACK: {
            advance();
            return create_ast_node(AST_STACK, 0, NULL, NULL, NULL, NULL);
        }
        case TOKEN_NUMBER:
        case TOKEN_STRING:
        case TOKEN_CHAR: {
            char *value = strdup(t->value);
            TokenType type = t->type;
            advance();
            return create_ast_node(AST_LITERAL, type, value, NULL, NULL, NULL);
        }
        case TOKEN_TILDE: {
            advance();
            Token *id_token = current_token();
            if (id_token->type != TOKEN_ID) {
                REPORT_ERROR("Expected structure name after '~'");
                return NULL;
            }
            char *struct_name = strdup(id_token->value);
            advance();
            return create_ast_node(AST_STRUCT_INST, 0, struct_name, NULL, NULL, NULL);
        }
        case TOKEN_TYPE: {
            Type *cast_type = parse_type_spec();
            if (!cast_type) return NULL;
            
            if (MATCH(TOKEN_LPAREN)) {
                CONSUME(TOKEN_LPAREN);
                ASTNode *expr = parse_expression();
                if (!expr) {
                    free_type(cast_type);
                    return NULL;
                }
                CONSUME(TOKEN_RPAREN);
                ASTNode *node = create_ast_node(AST_CAST, 0, NULL, expr, NULL, NULL);
                if (node) node->var_type = cast_type;
                return node;
            }
            REPORT_ERROR("Expected '(' for type cast");
            free_type(cast_type);
            return NULL;
        }
        default:
            REPORT_ERROR("Unexpected token in expression: %s", token_names[t->type]);
            return NULL;
    }
}

static ASTNode *parse_variable_decl(void) {
    TokenType sigil = current_token_type();
    advance();
    
    AST *var_list = malloc(sizeof(AST));
    if (!var_list) return NULL;
    var_list->nodes = NULL;
    var_list->count = 0;
    var_list->capacity = 0;
    
    while (1) {
        if (!MATCH(TOKEN_ID)) {
            REPORT_ERROR("Expected identifier");
            free_ast(var_list);
            return NULL;
        }
        
        Token *id_token = current_token();
        char *var_name = strdup(id_token->value);
        advance();
        
        ASTNode *var_node = create_ast_node(AST_ID, 0, var_name, NULL, NULL, NULL);
        if (!add_ast_node(var_list, var_node)) {
            free(var_name);
            free_ast(var_list);
            return NULL;
        }
        
        if (MATCH(TOKEN_COMMA)) advance();
        else if (MATCH(TOKEN_COLON)) break;
        else {
            REPORT_ERROR("Expected ',' or ':' after variable name");
            free_ast(var_list);
            return NULL;
        }
    }
    
    CONSUME(TOKEN_COLON);
    
    Type *var_type = parse_type_spec();
    if (!var_type) {
        free_ast(var_list);
        return NULL;
    }
    
    AST *init_list = NULL;
    if (TRY_CONSUME(TOKEN_EQUAL)) {
        init_list = malloc(sizeof(AST));
        if (!init_list) {
            free_type(var_type);
            free_ast(var_list);
            return NULL;
        }
        init_list->nodes = NULL;
        init_list->count = 0;
        init_list->capacity = 0;
        
        if (var_list->count == 1 && var_type->is_array) {
            ASTNode *init = parse_array_initializer();
            if (!init) {
                free_ast(init_list);
                free_type(var_type);
                free_ast(var_list);
                return NULL;
            }
            if (!add_ast_node(init_list, init)) {
                free_ast_node(init);
                free_ast(init_list);
                free_type(var_type);
                free_ast(var_list);
                return NULL;
            }
        } else {
            while (1) {
                ASTNode *expr = parse_expression();
                if (!expr) {
                    free_ast(init_list);
                    free_type(var_type);
                    free_ast(var_list);
                    return NULL;
                }
                
                if (!add_ast_node(init_list, expr)) {
                    free_ast_node(expr);
                    free_ast(init_list);
                    free_type(var_type);
                    free_ast(var_list);
                    return NULL;
                }
                
                if (MATCH(TOKEN_COMMA)) advance();
                else break;
            }
        }
    }
    
    CONSUME(TOKEN_SEMICOLON);
    
    ASTNode *multi_decl = create_ast_node(AST_MULTI_DECL, 0, NULL, (ASTNode*)var_list, (ASTNode*)init_list, NULL);
    if (!multi_decl) {
        free_ast(var_list);
        if (init_list) free_ast(init_list);
        free_type(var_type);
        return NULL;
    }
    multi_decl->var_type = var_type;
    multi_decl->decl_sigil = sigil;
    return multi_decl;
}

static ASTNode *parse_build(void) {
    CONSUME(TOKEN_BUILD);
    CONSUME(TOKEN_LPAREN);
    ASTNode *args = NULL;
    if (!MATCH(TOKEN_RPAREN)) args = parse_expression();
    CONSUME(TOKEN_RPAREN);
    ASTNode *body = parse_block();
    if (!body) {
        free_ast_node(args);
        return NULL;
    }
    return create_ast_node(AST_BUILD, 0, NULL, args, body, NULL);
}

static ASTNode *parse_struct_decl(void) {
    CONSUME(TOKEN_TILDE);
    Token *id_token = current_token();
    if (id_token->type != TOKEN_ID) {
        REPORT_ERROR("Expected structure name after '~'");
        return NULL;
    }
    char *struct_name = strdup(id_token->value);
    advance();
    
    ASTNode *body = parse_block();
    if (!body) {
        free(struct_name);
        return NULL;
    }
    return create_ast_node(AST_STRUCT_DECL, 0, struct_name, NULL, body, NULL);
}

static ASTNode *parse_multi_assignment(void) {
    AST *lvalue_list = malloc(sizeof(AST));
    if (!lvalue_list) return NULL;
    lvalue_list->nodes = NULL;
    lvalue_list->count = 0;
    lvalue_list->capacity = 0;

    while (1) {
        ASTNode *lvalue = parse_primary();
        if (!lvalue) {
            free_ast(lvalue_list);
            return NULL;
        }

        if (!add_ast_node(lvalue_list, lvalue)) {
            free_ast_node(lvalue);
            free_ast(lvalue_list);
            return NULL;
        }

        if (MATCH(TOKEN_COMMA)) advance();
        else break;
    }

    if (!MATCH(TOKEN_EQUAL)) {
        REPORT_ERROR("Expected '=' after lvalue list");
        free_ast(lvalue_list);
        return NULL;
    }
    advance();

    AST *rvalue_list = malloc(sizeof(AST));
    if (!rvalue_list) {
        free_ast(lvalue_list);
        return NULL;
    }
    rvalue_list->nodes = NULL;
    rvalue_list->count = 0;
    rvalue_list->capacity = 0;

    while (1) {
        ASTNode *expr = parse_expression();
        if (!expr) {
            free_ast(rvalue_list);
            free_ast(lvalue_list);
            return NULL;
        }

        if (!add_ast_node(rvalue_list, expr)) {
            free_ast_node(expr);
            free_ast(rvalue_list);
            free_ast(lvalue_list);
            return NULL;
        }

        if (MATCH(TOKEN_COMMA)) advance();
        else break;
    }

    CONSUME(TOKEN_SEMICOLON);
    return create_ast_node(AST_MULTI_ASSIGN, 0, NULL, (ASTNode*)lvalue_list, (ASTNode*)rvalue_list, NULL);
}

static ASTNode *parse_statement(void) {
    Token *t = current_token();
    if (!t) {
        REPORT_ERROR("Unexpected end of input");
        return NULL;
    }
    
    switch (t->type) {
        case TOKEN_DOLLAR:
        case TOKEN_AT:
        case TOKEN_AMPERSAND:
            return parse_variable_decl();
        case TOKEN_IF:
            return parse_if_statement();
        case TOKEN_SYSCALL:
            return parse_syscall();
        case TOKEN_ID:
            if (t->value[0] == '_') return parse_function();
            if (MATCH(TOKEN_ID)) {
                Token *next = current_token() + 1;
                if (next->type == TOKEN_COMMA || next->type == TOKEN_EQUAL) {
                    return parse_multi_assignment();
                }
            }
        case TOKEN_RETURN:
            return parse_return();
        case TOKEN_BREAK:
            return parse_break();
        case TOKEN_FREE:
            return parse_free();
        case TOKEN_BUILD:
            return parse_build();
        case TOKEN_TILDE:
            return parse_struct_decl();
        case TOKEN_LCURLY:
            return parse_block();
        default: {
            ASTNode *expr = parse_expression();
            if (expr) CONSUME(TOKEN_SEMICOLON);
            return expr;
        }
    }
}

void free_type(Type *type) {
    if (!type) return;
    free(type->name);
    for (int i = 0; i < type->modifier_count; i++) free(type->modifiers[i]);
    free(type->modifiers);
    free(type);
}

void free_ast_node(ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_BLOCK:
            if (node->extra) {
                AST *block_ast = (AST*)node->extra;
                for (int i = 0; i < block_ast->count; i++) free_ast_node(block_ast->nodes[i]);
                free(block_ast->nodes);
                free(block_ast);
            } else free_ast_node(node->left);
            break;
        case AST_IF:
            free_ast_node(node->left);
            free_ast_node(node->right);
            if (node->extra) free_ast_node(node->extra);
            break;
        case AST_FUNC:
        case AST_BUILD:
            free_ast_node(node->left);
            free_ast_node(node->right);
            break;
        case AST_FUNC_CALL:
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
        case AST_SELF:
        case AST_PUBLIC:
            free_ast_node(node->left);
            break;
        case AST_MULTI_DECL:
            if (node->left) {
                AST *var_list = (AST*)node->left;
                for (int i = 0; i < var_list->count; i++) free_ast_node(var_list->nodes[i]);
                free(var_list->nodes);
                free(var_list);
            }
            if (node->right) {
                AST *init_list = (AST*)node->right;
                for (int i = 0; i < init_list->count; i++) free_ast_node(init_list->nodes[i]);
                free(init_list->nodes);
                free(init_list);
            }
            break;
        case AST_MULTI_ASSIGN:
            if (node->left) {
                AST *lvalue_list = (AST*)node->left;
                for (int i = 0; i < lvalue_list->count; i++) free_ast_node(lvalue_list->nodes[i]);
                free(lvalue_list->nodes);
                free(lvalue_list);
            }
            if (node->right) {
                AST *rvalue_list = (AST*)node->right;
                for (int i = 0; i < rvalue_list->count; i++) free_ast_node(rvalue_list->nodes[i]);
                free(rvalue_list->nodes);
                free(rvalue_list);
            }
            break;
        case AST_ARRAY_INIT:
            if (node->extra) {
                AST *list = (AST*)node->extra;
                for (int i = 0; i < list->count; i++) free_ast_node(list->nodes[i]);
                free(list->nodes);
                free(list);
            }
            break;
        default:
            free_ast_node(node->left);
            free_ast_node(node->right);
            free_ast_node(node->extra);
            break;
    }
    
    free(node->value);
    if (node->var_type) free_type(node->var_type);
    if (node->return_type) free_type(node->return_type);
    free(node);
}

void print_ast(AST *ast) {
    for (int i = 0; i < ast->count; i++) {
        printf("Statement %d:\n", i + 1);
        print_ast_node(ast->nodes[i], 1);
    }
}

void free_ast(AST *ast) {
    if (!ast) return;
    for (int i = 0; i < ast->count; i++) free_ast_node(ast->nodes[i]);
    free(ast->nodes);
    free(ast);
}

AST *parse(Token *input_tokens, int input_token_count) {
    tokens = input_tokens;
    token_count = input_token_count;
    current_token_index = 0;

    if (token_count == 0 || (token_count == 1 && tokens[0].type == TOKEN_EOF)) {
        AST *ast = malloc(sizeof(AST));
        if (!ast) return NULL;
        ast->nodes = NULL;
        ast->count = 0;
        ast->capacity = 0;
        return ast;
    }

    AST *ast = malloc(sizeof(AST));
    if (!ast) return NULL;
    ast->nodes = NULL;
    ast->count = 0;
    ast->capacity = 0;
    
    while (current_token_type() != TOKEN_EOF && !has_errors()) {
        ASTNode *node = parse_statement();
        if (node) {
            if (!add_ast_node(ast, node)) {
                free_ast_node(node);
                free_ast(ast);
                return NULL;
            }
        } else if (has_errors()) break;
        else advance();
    }
    
    if (has_errors()) {
        free_ast(ast);
        return NULL;
    }
    
    return ast;
}

