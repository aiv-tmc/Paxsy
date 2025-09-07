#include "parser.h"
#include "../lexer/lexer.h"
#include "../error_manager/error_manager.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define MAX_MODIFIERS 8
#define AST_INITIAL_CAPACITY 8
#define MAX_CPU_ARGS 6

static int current_token_position = 0;
static Token *token_stream = NULL;
static int total_tokens = 0;

static TokenType get_current_token_type(void);
static void advance_token(void);
static bool expect_token(TokenType expected_type);
static Token *get_current_token(void);
static ASTNode *parse_statement(void);
static ASTNode *parse_expression(void);
static Type *parse_type_specifier_silent(bool silent);
static Type *parse_type_specifier(void);
static Type *try_parse_type_specifier(void);
static ASTNode *parse_parameter_list(bool *is_variadic);
static ASTNode *parse_array_initializer(void);
static void print_ast_node(ASTNode *node, int indent);
static ASTNode *parse_preprocessor_directive(void);
static ASTNode *parse_postfix_expression(ASTNode *node);
static ASTNode *parse_function_parameter(void);
static ASTNode *parse_function_call_statement(void);

#define CURRENT_TOKEN_TYPE_MATCHES(token) (get_current_token_type() == (token))
#define CONSUME_TOKEN(token) do { if (!expect_token(token)) return NULL; } while(0)
#define ATTEMPT_CONSUME_TOKEN(token) (CURRENT_TOKEN_TYPE_MATCHES(token) ? (advance_token(), 1) : 0)

#define REPORT_PARSE_ERROR(...) do { \
    Token *current = get_current_token(); \
    if (current) report_error(current->line, current->column, __VA_ARGS__); \
    else report_error(0, 0, __VA_ARGS__); \
} while(0)

static TokenType get_current_token_type(void) {
    Token *token = get_current_token();
    return token ? token->type : TOKEN_EOF;
}

static void advance_token(void) {
    if (current_token_position < total_tokens - 1) current_token_position++;
}

static Token *get_current_token(void) {
    if (current_token_position < total_tokens) return &token_stream[current_token_position];
    return NULL;
}

static bool expect_token(TokenType expected_type) {
    if (CURRENT_TOKEN_TYPE_MATCHES(expected_type)) {
        advance_token();
        return true;
    }
    
    TokenType actual = get_current_token_type();
    const char *expected_name = token_names[expected_type];
    const char *actual_name = (actual == TOKEN_EOF) ? "EOF" : token_names[actual];
    
    REPORT_PARSE_ERROR("Expected %s but got %s", expected_name, actual_name);
    return false;
}

static Type *parse_type_specifier_silent(bool silent) {
    Type *type = malloc(sizeof(Type));
    if (!type) {
        if (!silent) REPORT_PARSE_ERROR("Memory allocation failed for type specifier");
        return NULL;
    }
    
    type->pointer_level = 0;
    type->is_reference = 0;
    type->name = NULL;
    type->left_number = 0;
    type->right_number = 0;
    type->modifiers = NULL;
    type->modifier_count = 0;
    type->is_array = 0;
    type->array_size = 0;

    while (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_MODIFIER) && type->modifier_count < MAX_MODIFIERS) {
        Token *modifier_token = get_current_token();
        char **new_modifiers = realloc(type->modifiers, (type->modifier_count + 1) * sizeof(char*));
        if (!new_modifiers) {
            if (!silent) REPORT_PARSE_ERROR("Memory allocation failed for modifiers");
            for (int i = 0; i < type->modifier_count; i++) {
                free(type->modifiers[i]);
            }
            free(type->modifiers);
            free(type);
            return NULL;
        }
        type->modifiers = new_modifiers;
        type->modifiers[type->modifier_count] = strdup(modifier_token->value);
        if (!type->modifiers[type->modifier_count]) {
            if (!silent) REPORT_PARSE_ERROR("Memory allocation failed for modifier string");
            for (int i = 0; i < type->modifier_count; i++) {
                free(type->modifiers[i]);
            }
            free(type->modifiers);
            free(type);
            return NULL;
        }
        type->modifier_count++;
        advance_token();
    }

    while (
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_AT) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_DOUBLE_AT) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_AMPERSAND)
    ) {
        if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_AT) || CURRENT_TOKEN_TYPE_MATCHES(TOKEN_DOUBLE_AT)) type->pointer_level++;
        else if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_AMPERSAND)) type->is_reference = 1;
        advance_token();
    }

    if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_NUMBER)) {
        Token *number_token = get_current_token();
        int value = atoi(number_token->value);
        if (value < 0 || value > 255) {
            if (!silent) REPORT_PARSE_ERROR("Number out of range (0-255)");
            free_type(type);
            return NULL;
        }
        type->left_number = (uint8_t)value;
        advance_token();
    }

    if (get_current_token_type() != TOKEN_TYPE) {
        if (!silent) REPORT_PARSE_ERROR("Expected type specifier");
        free_type(type);
        return NULL;
    }
    
    Token *type_token = get_current_token();
    type->name = strdup(type_token->value);
    advance_token();

    if (ATTEMPT_CONSUME_TOKEN(TOKEN_COLON)) {
        if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_NUMBER)) {
            Token *number_token = get_current_token();
            int value = atoi(number_token->value);
            if (value < 0 || value > 255) {
                if (!silent) REPORT_PARSE_ERROR("Number out of range (0-255)");
                free_type(type);
                return NULL;
            }
            type->right_number = (uint8_t)value;
            advance_token();
        } else {
            if (!silent) REPORT_PARSE_ERROR("Expected number after ':' in type specifier");
            free_type(type);
            return NULL;
        }
    }
    
    return type;
}

static Type *parse_type_specifier(void) {
    return parse_type_specifier_silent(false);
}

static Type *try_parse_type_specifier(void) {
    return parse_type_specifier_silent(true);
}

static ASTNode *create_ast_node(ASTNodeType node_type, TokenType operation_type, 
    char *node_value, ASTNode *left_child, ASTNode *right_child, ASTNode *extra_node) {
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = node_type;
    node->operation_type = operation_type;
    node->value = node_value;
    node->left = left_child;
    node->right = right_child;
    node->extra = extra_node;
    node->variable_type = NULL;
    node->return_type = NULL;
    node->declaration_sigil = TOKEN_ERROR;
    node->is_variadic = 0;
    
    return node;
}

static bool add_ast_node_to_list(AST *ast, ASTNode *node) {
    if (ast->count >= ast->capacity) {
        uint16_t new_capacity = ast->capacity ? ast->capacity * 2 : AST_INITIAL_CAPACITY;
        ASTNode **new_nodes = realloc(ast->nodes, new_capacity * sizeof(ASTNode*));
        if (!new_nodes) return false;
        ast->nodes = new_nodes;
        ast->capacity = new_capacity;
    }
    ast->nodes[ast->count++] = node;
    return true;
}

static ASTNode *parse_assignment_expression(void);
static ASTNode *parse_logical_or_expression(void);
static ASTNode *parse_logical_xor_expression(void);
static ASTNode *parse_logical_and_expression(void);
static ASTNode *parse_bitwise_or_expression(void);
static ASTNode *parse_bitwise_xor_expression(void);
static ASTNode *parse_bitwise_and_expression(void);
static ASTNode *parse_equality_expression(void);
static ASTNode *parse_relational_expression(void);
static ASTNode *parse_shift_expression(void);
static ASTNode *parse_additive_expression(void);
static ASTNode *parse_multiplicative_expression(void);
static ASTNode *parse_exponent_expression(void);
static ASTNode *parse_unary_expression(void);
static ASTNode *parse_primary_expression(void);

static ASTNode *parse_postfix_expression(ASTNode *node) {
    while (1) {
        if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_LPAREN)) {
            int saved_position = current_token_position;
            advance_token();
            Type *conversion_type = parse_type_specifier();
            if (conversion_type != NULL) {
                if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_RPAREN)) {
                    advance_token();
                    node = create_ast_node(AST_CAST, 0, NULL, node, NULL, NULL);
                    node->variable_type = conversion_type;
                    continue;
                } else {
                    free_type(conversion_type);
                }
            }
            current_token_position = saved_position;
            advance_token();
            ASTNode *arguments = NULL;
            if (!CURRENT_TOKEN_TYPE_MATCHES(TOKEN_RPAREN)) {
                arguments = parse_expression();
                if (!arguments) {
                    free_ast_node(node);
                    return NULL;
                }
            }
            CONSUME_TOKEN(TOKEN_RPAREN);
            node = create_ast_node(AST_FUNCTION_CALL, 0, node->value, arguments, NULL, NULL);
        } else if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_LBRACKET)) {
            advance_token();
            ASTNode *index = parse_expression();
            if (!index) {
                free_ast_node(node);
                return NULL;
            }
            CONSUME_TOKEN(TOKEN_RBRACKET);
            node = create_ast_node(AST_ARRAY_ACCESS, 0, NULL, node, index, NULL);
        } else if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_ARROW)) {
            advance_token();
            CONSUME_TOKEN(TOKEN_LPAREN);
            Type *target_type = parse_type_specifier();
            if (!target_type) {
                free_ast_node(node);
                return NULL;
            }
            CONSUME_TOKEN(TOKEN_RPAREN);
            node = create_ast_node(AST_POSTFIX_CAST, 0, NULL, node, NULL, NULL);
            node->variable_type = target_type;
        } else if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_DOUBLE_PLUS)) {
            advance_token();
            node = create_ast_node(AST_POSTFIX_INCREMENT, 0, NULL, node, NULL, NULL);
        } else if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_DOUBLE_MINUS)) {
            advance_token();
            node = create_ast_node(AST_POSTFIX_DECREMENT, 0, NULL, node, NULL, NULL);
        } else break;
    }
    return node;
}

static ASTNode *parse_block_statement(void) {
    if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_LCURLY)) {
        CONSUME_TOKEN(TOKEN_LCURLY);
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
        
        while (get_current_token_type() != TOKEN_RCURLY && get_current_token_type() != TOKEN_EOF) {
            ASTNode *statement = parse_statement();
            if (statement && !add_ast_node_to_list(block_ast, statement)) {
                free_ast_node(statement);
                free(block_ast);
                free_ast_node(block_node);
                return NULL;
            }
        }
        CONSUME_TOKEN(TOKEN_RCURLY);
        
        block_node->extra = (ASTNode*)block_ast;
        return block_node;
    } else if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_ARROW)) {
        advance_token();
        ASTNode *statement = parse_statement();
        return create_ast_node(AST_BLOCK, 0, NULL, statement, NULL, NULL);
    }
    
    ASTNode *single_statement = parse_statement();
    return create_ast_node(AST_BLOCK, 0, NULL, single_statement, NULL, NULL);
}

static ASTNode *parse_array_initializer(void) {
    if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_LCURLY)) {
        CONSUME_TOKEN(TOKEN_LCURLY);
        AST *list = malloc(sizeof(AST));
        if (!list) return NULL;
        list->nodes = NULL;
        list->count = 0;
        list->capacity = 0;

        while (!CURRENT_TOKEN_TYPE_MATCHES(TOKEN_RCURLY) && !CURRENT_TOKEN_TYPE_MATCHES(TOKEN_EOF)) {
            ASTNode *expression = parse_expression();
            if (!expression) {
                free_ast(list);
                return NULL;
            }
            if (!add_ast_node_to_list(list, expression)) {
                free_ast_node(expression);
                free_ast(list);
                return NULL;
            }
            if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_COMMA)) advance_token();
            else break;
        }
        CONSUME_TOKEN(TOKEN_RCURLY);
        return create_ast_node(AST_ARRAY_INITIALIZER, 0, NULL, NULL, NULL, (ASTNode*)list);
    } else {
        REPORT_PARSE_ERROR("Expected '{' for array initializer");
        return NULL;
    }
}

static void print_ast_node(ASTNode *node, int indent) {
    if (!node) return;
    
    for (int i = 0; i < indent; i++) printf("  ");
    
    switch (node->type) {
        case AST_VARIABLE_DECLARATION:
            printf("VARIABLE_DECL:\n");
            for (int i = 0; i < indent+1; i++) printf("  ");
            printf("sigil: %s\n", 
                node->declaration_sigil == TOKEN_DOLLAR ? "$" : 
                node->declaration_sigil == TOKEN_AT ? "@" : 
                node->declaration_sigil == TOKEN_DOUBLE_AT ? "@@" : "&"
            );
            for (int i = 0; i < indent+1; i++) printf("  ");
            printf("name: %s\n", node->value);
            if (node->variable_type) {
                for (int i = 0; i < indent+1; i++) printf("  ");
                printf("modifiers: ");
                for (int j = 0; j < node->variable_type->modifier_count; j++) printf("%s ", node->variable_type->modifiers[j]);
                printf("\n");
                if (node->variable_type->pointer_level > 0) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("PTR\n");
                }
                if (node->variable_type->is_reference) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("REF\n");
                }
                if (node->variable_type->left_number != 0) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("BASE: %d\n", node->variable_type->left_number);
                }
                for (int i = 0; i < indent+1; i++) printf("  ");
                printf("TYPE: %s\n", node->variable_type->name);
                if (node->variable_type->right_number != 0) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("SIZE: %d\n", node->variable_type->right_number);
                }
                if (node->variable_type->is_array) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("ARRAY_SIZE: %d\n", node->variable_type->array_size);
                }
            }
            if (node->left) {
                for (int i = 0; i < indent+1; i++) printf("  ");
                printf("initializer:\n");
                print_ast_node(node->left, indent+2);
            }
            break;
            
        case AST_BINARY_OPERATION:
            printf("BINARY_OP: %s\n", token_names[node->operation_type]);
            print_ast_node(node->left, indent + 1);
            print_ast_node(node->right, indent + 1);
            break;
            
        case AST_UNARY_OPERATION:
            printf("UNARY_OP: %s", token_names[node->operation_type]);
            print_ast_node(node->right, indent + 1);
            break;
            
        case AST_LITERAL_VALUE:
            printf("LITERAL(%s): %s\n", token_names[node->operation_type], node->value);
            break;
            
        case AST_IDENTIFIER:
            printf("ID: %s\n", node->value);
            break;
        
        case AST_VARIABLE:
            printf("VARIABLE: %s\n", node->value);
            break;
        
        case AST_POINTER_VARIABLE:
            printf("POINTER: %s\n", node->value);
            break;
            
        case AST_REFERENCE_VARIABLE:
            printf("REFERENCE: %s\n", node->value);
            break;
            
        case AST_DEREFERENCE:
            printf("DEREFERENCE: ");
            print_ast_node(node->right, 0);
            printf("\n");
            break;
            
        case AST_ADDRESS_OF:
            printf("ADDRESS-OF: ");
            print_ast_node(node->right, 0);
            printf("\n");
            break;
            
        case AST_ASSIGNMENT:
            printf("ASSIGNMENT: %s\n", token_names[node->operation_type]);
            print_ast_node(node->left, indent + 1);
            print_ast_node(node->right, indent + 1);
            break;
            
        case AST_COMPOUND_ASSIGNMENT:
            printf("COMPOUND_ASSIGNMENT: %s\n", token_names[node->operation_type]);
            print_ast_node(node->left, indent + 1);
            print_ast_node(node->right, indent + 1);
            break;
            
        case AST_BLOCK:
            printf("BLOCK\n");
            if (node->extra) {
                AST *block_ast = (AST*)node->extra;
                for (int i = 0; i < block_ast->count; i++) print_ast_node(block_ast->nodes[i], indent + 1);
            } else print_ast_node(node->left, indent + 1);
            break;
                        
        case AST_FUNCTION:
            printf("FUNCTION: %s", node->value);
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

        case AST_FUNCTION_CALL:
            printf("FUNCTION CALL:%s\n", node->value);
            print_ast_node(node->left, indent + 1);
            break;

        case AST_FUNCTION_CALL_STATEMENT:
            printf("FUNCTION CALL STATEMENT:%s\n", node->value);
            print_ast_node(node->left, indent + 1);
            break;

        case AST_NULL:
            printf("NULL\n");
            print_ast_node(node->left, indent + 1);
            break;

        case AST_NULLPTR:
            printf("NULLPTR\n");
            print_ast_node(node->left, indent + 1);
            break;
               
        case AST_IF_STATEMENT:
            printf("IF\n");
            print_ast_node(node->left, indent + 1);
            print_ast_node(node->right, indent + 1);
            if (node->extra) {
                for (int i = 0; i < indent; i++) printf("  ");
                printf("ELSE:\n");
                print_ast_node(node->extra, indent + 1);
            }
            break;

        case AST_ELSE_STATEMENT:
            printf("ELSE\n");
            print_ast_node(node->left, indent + 1);
            break;
            
        case AST_RETURN:
            printf("RETURN\n");
            print_ast_node(node->left, indent + 1);
            break;

        case AST_FREE:
            printf("FREE\n");
            print_ast_node(node->left, indent + 1);
            break;
            
        case AST_BREAK:
            printf("BREAK\n");
            break;

        case AST_CONTINUE:
            printf("CONTINUE\n");
            break;
        
        case AST_SIZEOF:
            printf("SIZEOF\n");
            print_ast_node(node->left, indent + 1);
            break;

        case AST_PARSEOF:
            printf("PARSEOF\n");
            print_ast_node(node->left, indent + 1);
            break;
            
        case AST_REALLOC:
            printf("REALLOC\n");
            print_ast_node(node->left, indent + 1);
            break;

        case AST_ALLOC:
            printf("ALLOC\n");
            print_ast_node(node->left, indent + 1);
            break;

        case AST_STACK:
            printf("STACK\n");
            break;

        case AST_PUSH:
            printf("PUSH\n");
            print_ast_node(node->left, indent + 1);
            break;

        case AST_POP:
            printf("POP\n");
            print_ast_node(node->left, indent + 1);
            break;
            
        case AST_STRUCT_DECLARATION:
            printf("STRUCT_DECL: %s\n", node->value);
            print_ast_node(node->right, indent + 1);
            break;
            
        case AST_STRUCT_INSTANCE:
            printf("STRUCT_INST: %s\n", node->value);
            break;
            
        case AST_CAST:
            printf("CAST TO: ");
            for (int j = 0; j < node->variable_type->modifier_count; j++) printf("%s ", node->variable_type->modifiers[j]);
            printf("%s\n", node->variable_type->name);
            print_ast_node(node->left, indent + 1);
            break;

        case AST_POSTFIX_CAST:
            printf("POSTFIX_CAST TO: ");
            for (int j = 0; j < node->variable_type->modifier_count; j++) printf("%s ", node->variable_type->modifiers[j]);
            printf("%s\n", node->variable_type->name);
            print_ast_node(node->left, indent + 1);
            break;
            
        case AST_CPU:
            printf("CPU: %s", node->value);
            if (node->left) {
                AST *args = (AST*)node->left;
                printf(" with arguments:\n");
                for (int i = 0; i < args->count; i++) {
                    print_ast_node(args->nodes[i], indent + 1);
                }
            } else {
                printf("\n");
            }
            break;
            
        case AST_SELF:
            printf("SELF QUALIFIED VARIABLE:\n");
            print_ast_node(node->left, indent + 1);
            break;
            
        case AST_PUBLIC:
            printf("PUBLIC QUALIFIED VARIABLE:\n");
            print_ast_node(node->left, indent + 1);
            break;

        case AST_MULTI_DECLARATION:
            printf("MULTI_DECL\n");
            if (node->variable_type) {
                for (int i = 0; i < indent+1; i++) printf("  ");
                printf("modifiers: ");
                for (int j = 0; j < node->variable_type->modifier_count; j++) printf("%s ", node->variable_type->modifiers[j]);
                printf("\n");
                if (node->variable_type->pointer_level) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("AT\n");
                }
                if (node->variable_type->is_reference) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("REF\n");
                }
                if (node->variable_type->left_number != 0) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("BASE: %d\n", node->variable_type->left_number);
                }
                for (int i = 0; i < indent+1; i++) printf("  ");
                printf("TYPE: %s\n", node->variable_type->name);
                if (node->variable_type->right_number != 0) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("SIZE: %d\n", node->variable_type->right_number);
                }
                if (node->variable_type->is_array) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("ARRAY_SIZE: %d\n", node->variable_type->array_size);
                }
            }
            if (node->left) {
                AST *variable_list = (AST*)node->left;
                for (int i = 0; i < variable_list->count; i++) {
                    print_ast_node(variable_list->nodes[i], indent + 1);
                }
            }
            if (node->right) {
                AST *initializer_list = (AST*)node->right;
                for (int i = 0; i < initializer_list->count; i++) {
                    print_ast_node(initializer_list->nodes[i], indent + 1);
                }
            }
            break;

        case AST_MULTI_ASSIGNMENT:
            printf("MULTI_ASSIGN\n");
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

        case AST_ARRAY_INITIALIZER:
            printf("ARRAY_INITIALIZER\n");
            if (node->extra) {
                AST *list = (AST*)node->extra;
                for (int i = 0; i < list->count; i++) print_ast_node(list->nodes[i], indent + 1);
            } else print_ast_node(node->left, indent + 1);
            break;

        case AST_PREPROCESSOR_DIRECTIVE:
            printf("PREPROCESSOR DIRECTIVE: %s\n", node->value);
            if (node->left) {
                print_ast_node(node->left, indent + 1);
            }
            break;

        case AST_ARRAY_ACCESS:
            printf("ARRAY_ACCESS\n");
            print_ast_node(node->left, indent + 1);
            print_ast_node(node->right, indent + 1);
            break;

        case AST_LABEL_DECLARATION:
            printf("LABEL DECLARATION: %s\n", node->value);
            break;

        case AST_GOTO:
            printf("GOTO: %s\n", node->value);
            break;

        case AST_VA_START:
            printf("VA_START\n");
            print_ast_node(node->left, indent + 1);
            print_ast_node(node->right, indent + 1);
            break;

        case AST_VA_ARG:
            printf("VA_ARG\n");
            print_ast_node(node->left, indent + 1);
            if (node->variable_type) {
                for (int i = 0; i < indent+1; i++) printf("  ");
                printf("TYPE: %s\n", node->variable_type->name);
            }
            break;

        case AST_VA_END:
            printf("VA_END\n");
            print_ast_node(node->left, indent + 1);
            break;

        case AST_POSTFIX_INCREMENT:
            printf("POSTFIX_INCREMENT\n");
            print_ast_node(node->left, indent + 1);
            break;

        case AST_POSTFIX_DECREMENT:
            printf("POSTFIX_DECREMENT\n");
            print_ast_node(node->left, indent + 1);
            break;
    }
}

static ASTNode *parse_if_statement(void) {
    CONSUME_TOKEN(TOKEN_IF);
    ASTNode *condition = parse_expression();
    if (!condition) return NULL;
    
    ASTNode *if_block = parse_block_statement();
    if (!if_block) {
        free_ast_node(condition);
        return NULL;
    }
    
    ASTNode *else_block = NULL;
    if (ATTEMPT_CONSUME_TOKEN(TOKEN_ELSE)) else_block = parse_block_statement();
    
    return create_ast_node(AST_IF_STATEMENT, 0, NULL, condition, if_block, else_block);
}

static ASTNode *parse_cpu(void) {
    CONSUME_TOKEN(TOKEN_CPU);
    expect_token(TOKEN_DOUBLE_COLON);
    
    if (!CURRENT_TOKEN_TYPE_MATCHES(TOKEN_NUMBER) && !CURRENT_TOKEN_TYPE_MATCHES(TOKEN_ID)) {
        REPORT_PARSE_ERROR("Expected cpu number or variable after '::'");
        return NULL;
    }
    
    Token *token = get_current_token();
    char *cpu_value = strdup(token->value);
    advance_token();

    AST *arguments = NULL;
    if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_LPAREN)) {
        advance_token();
        arguments = malloc(sizeof(AST));
        if (!arguments) {
            free(cpu_value);
            return NULL;
        }
        arguments->nodes = NULL;
        arguments->count = 0;
        arguments->capacity = 0;

        while (!CURRENT_TOKEN_TYPE_MATCHES(TOKEN_RPAREN) && !CURRENT_TOKEN_TYPE_MATCHES(TOKEN_EOF)) {
            if (arguments->count >= MAX_CPU_ARGS) {
                REPORT_PARSE_ERROR("Too many arguments for cpu, maximum is %d", MAX_CPU_ARGS);
                free_ast(arguments);
                free(cpu_value);
                return NULL;
            }
            ASTNode *arg = parse_expression();
            if (!arg) {
                free_ast(arguments);
                free(cpu_value);
                return NULL;
            }
            if (!add_ast_node_to_list(arguments, arg)) {
                free_ast_node(arg);
                free_ast(arguments);
                free(cpu_value);
                return NULL;
            }
            if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_COMMA)) {
                advance_token();
            } else {
                break;
            }
        }
        CONSUME_TOKEN(TOKEN_RPAREN);
    }

    CONSUME_TOKEN(TOKEN_SEMICOLON);
    return create_ast_node(AST_CPU, 0, cpu_value, (ASTNode*)arguments, NULL, NULL);
}

static ASTNode *parse_function_parameter(void) {
    CONSUME_TOKEN(TOKEN_DOT);

    if (!CURRENT_TOKEN_TYPE_MATCHES(TOKEN_ID)) {
        REPORT_PARSE_ERROR("Expected function name after '.' in function parameter");
        return NULL;
    }

    Token *name_token = get_current_token();
    char *func_name = strdup(name_token->value);
    advance_token();

    ASTNode *params = NULL;
    bool is_variadic = false;
    if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_LPAREN)) {
        params = parse_parameter_list(&is_variadic);
        if (!params) {
            free(func_name);
            return NULL;
        }
        if (is_variadic) {
            REPORT_PARSE_ERROR("Variadic parameters are not allowed in function parameter");
            free_ast_node(params);
            free(func_name);
            return NULL;
        }
    } else {
        params = create_ast_node(AST_BLOCK, 0, NULL, NULL, NULL, NULL);
        if (params) {
            AST *empty_params = malloc(sizeof(AST));
            if (empty_params) {
                empty_params->nodes = NULL;
                empty_params->count = 0;
                empty_params->capacity = 0;
                params->extra = (ASTNode*)empty_params;
            }
        }
    }

    Type *return_type = try_parse_type_specifier();

    ASTNode *func_node = create_ast_node(AST_FUNCTION, 0, func_name, params, NULL, NULL);
    if (func_node) {
        func_node->return_type = return_type;
        func_node->is_variadic = is_variadic;
    } else {
        free(func_name);
        free_ast_node(params);
        free_type(return_type);
    }

    return func_node;
}

static ASTNode *parse_parameter_list(bool *is_variadic) {
    AST *parameter_ast = malloc(sizeof(AST));
    if (!parameter_ast) return NULL;
    parameter_ast->nodes = NULL;
    parameter_ast->count = 0;
    parameter_ast->capacity = 0;
    
    *is_variadic = false;
    
    CONSUME_TOKEN(TOKEN_LPAREN);
    
    if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_RPAREN)) {
        CONSUME_TOKEN(TOKEN_RPAREN);
        return create_ast_node(AST_BLOCK, 0, NULL, NULL, NULL, (ASTNode*)parameter_ast);
    }
    
    while (get_current_token_type() != TOKEN_RPAREN && get_current_token_type() != TOKEN_EOF) {
        if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_ELLIPSIS)) {
            advance_token();
            *is_variadic = true;
            if (!CURRENT_TOKEN_TYPE_MATCHES(TOKEN_RPAREN)) {
                REPORT_PARSE_ERROR("Expected ')' after ellipsis");
                free_ast((AST*)parameter_ast);
                return NULL;
            }
            break;
        }
        
        if (!CURRENT_TOKEN_TYPE_MATCHES(TOKEN_DOLLAR) &&
            !CURRENT_TOKEN_TYPE_MATCHES(TOKEN_AT) &&
            !CURRENT_TOKEN_TYPE_MATCHES(TOKEN_DOUBLE_AT) &&
            !CURRENT_TOKEN_TYPE_MATCHES(TOKEN_AMPERSAND) &&
            !CURRENT_TOKEN_TYPE_MATCHES(TOKEN_DOT)) {
            REPORT_PARSE_ERROR("Expected '$', '@', '@@', '&' or '.' for parameter declaration");
            free_ast((AST*)parameter_ast);
            return NULL;
        }

        if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_DOT)) {
            ASTNode *func_param = parse_function_parameter();
            if (!func_param) {
                free_ast((AST*)parameter_ast);
                return NULL;
            }
            if (!add_ast_node_to_list(parameter_ast, func_param)) {
                free_ast_node(func_param);
                free_ast((AST*)parameter_ast);
                return NULL;
            }
        } else {
            TokenType sigil = get_current_token_type();
            advance_token();
            
            Token *identifier_token = get_current_token();
            if (identifier_token->type != TOKEN_ID) {
                REPORT_PARSE_ERROR("Expected identifier for parameter name");
                free_ast((AST*)parameter_ast);
                return NULL;
            }
            char *parameter_name = strdup(identifier_token->value);
            advance_token();

            int array_size = -1;
            if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_LBRACKET)) {
                advance_token();
                if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_NUMBER)) {
                    Token *size_token = get_current_token();
                    array_size = atoi(size_token->value);
                    advance_token();
                } else {
                    REPORT_PARSE_ERROR("Expected array size");
                    free(parameter_name);
                    free_ast((AST*)parameter_ast);
                    return NULL;
                }
                CONSUME_TOKEN(TOKEN_RBRACKET);
            }
            
            Type *parameter_type = parse_type_specifier();
            if (!parameter_type) {
                free(parameter_name);
                free_ast((AST*)parameter_ast);
                return NULL;
            }

            if (array_size != -1) {
                parameter_type->is_array = 1;
                parameter_type->array_size = array_size;
            }
            
            if (sigil == TOKEN_AT || sigil == TOKEN_DOUBLE_AT) {
                parameter_type->pointer_level = 1;
                parameter_type->is_reference = 0;
            } else if (sigil == TOKEN_AMPERSAND) {
                parameter_type->is_reference = 1;
                parameter_type->pointer_level = 0;
            } else if (sigil == TOKEN_DOLLAR) {
                parameter_type->pointer_level = 0;
                parameter_type->is_reference = 0;
            }
            
            ASTNode *parameter_node = create_ast_node(AST_VARIABLE_DECLARATION, 0, parameter_name, NULL, NULL, NULL);
            if (!parameter_node) {
                free(parameter_name);
                free_type(parameter_type);
                free_ast((AST*)parameter_ast);
                return NULL;
            }
            parameter_node->variable_type = parameter_type;
            parameter_node->declaration_sigil = sigil;
            
            if (!add_ast_node_to_list(parameter_ast, parameter_node)) {
                free_ast_node(parameter_node);
                free_ast((AST*)parameter_ast);
                return NULL;
            }
        }
        
        if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_COMMA)) advance_token();
        else if (!CURRENT_TOKEN_TYPE_MATCHES(TOKEN_RPAREN) && !CURRENT_TOKEN_TYPE_MATCHES(TOKEN_ELLIPSIS)) {
            REPORT_PARSE_ERROR("Expected ',' or ')' after parameter");
            free_ast((AST*)parameter_ast);
            return NULL;
        }
    }
    
    CONSUME_TOKEN(TOKEN_RPAREN);
    return create_ast_node(AST_BLOCK, 0, NULL, NULL, NULL, (ASTNode*)parameter_ast);
}

static ASTNode *parse_function_declaration(void) {
    CONSUME_TOKEN(TOKEN_DOT);
    
    if (!CURRENT_TOKEN_TYPE_MATCHES(TOKEN_ID)) {
        REPORT_PARSE_ERROR("Expected function name");
        return NULL;
    }
    Token *token = get_current_token();
    char *function_name = strdup(token->value);
    advance_token();
    
    bool is_variadic = false;
    ASTNode *arguments = NULL;
    
    if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_LPAREN)) arguments = parse_parameter_list(&is_variadic);
    else {
        arguments = create_ast_node(AST_BLOCK, 0, NULL, NULL, NULL, NULL);
        if (arguments) {
            AST *empty_parameters = malloc(sizeof(AST));
            if (empty_parameters) {
                empty_parameters->nodes = NULL;
                empty_parameters->count = 0;
                empty_parameters->capacity = 0;
                arguments->extra = (ASTNode*)empty_parameters;
            }
        }
    }
    
    if (!arguments) {
        free(function_name);
        return NULL;
    }
    
    Type *return_type = NULL;
    return_type = parse_type_specifier();
    if (!return_type) {
        free(function_name);
        free_ast_node(arguments);
        return NULL;
    }
    
    ASTNode *body = NULL;
    if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_LCURLY)) body = parse_block_statement();
    else if (ATTEMPT_CONSUME_TOKEN(TOKEN_ARROW)) {
        ASTNode *statement = parse_statement();
        body = create_ast_node(AST_BLOCK, 0, NULL, statement, NULL, NULL);
    } else if (ATTEMPT_CONSUME_TOKEN(TOKEN_SEMICOLON)) {
    } else {
        REPORT_PARSE_ERROR("Expected '{', ':', '=>' or ';' for function body");
        free(function_name);
        free_type(return_type);
        free_ast_node(arguments);
        return NULL;
    }
    
    if (!body && get_current_token_type() != TOKEN_SEMICOLON) {
        REPORT_PARSE_ERROR("Expected function body");
        free(function_name);
        free_type(return_type);
        free_ast_node(arguments);
        return NULL;
    }
    
    ASTNode *node = create_ast_node(AST_FUNCTION, 0, function_name, arguments, body, NULL);
    if (node) {
        node->return_type = return_type;
        node->is_variadic = is_variadic;
    } else {
        free(function_name);
        free_type(return_type);
        free_ast_node(arguments);
        free_ast_node(body);
    }
    
    return node;
}

static ASTNode *parse_label_declaration(void) {
    Token *token = get_current_token();
    char *label_name = strdup(token->value);
    advance_token();
    
    CONSUME_TOKEN(TOKEN_COLON);
    return create_ast_node(AST_LABEL_DECLARATION, 0, label_name, NULL, NULL, NULL);
}

static ASTNode *parse_goto_statement(void) {
    CONSUME_TOKEN(TOKEN_GOTO);
    
    if (!CURRENT_TOKEN_TYPE_MATCHES(TOKEN_ID)) {
        REPORT_PARSE_ERROR("Expected label name after 'goto'");
        return NULL;
    }
    Token *token = get_current_token();
    char *label_name = strdup(token->value);
    advance_token();
    
    CONSUME_TOKEN(TOKEN_SEMICOLON);
    return create_ast_node(AST_GOTO, 0, label_name, NULL, NULL, NULL);
}

static ASTNode *parse_va_start(void) {
    CONSUME_TOKEN(TOKEN_VA_START);
    CONSUME_TOKEN(TOKEN_LPAREN);
    ASTNode *va_list = parse_expression();
    if (!va_list) return NULL;
    CONSUME_TOKEN(TOKEN_COMMA);
    ASTNode *last_arg = parse_expression();
    if (!last_arg) {
        free_ast_node(va_list);
        return NULL;
    }
    CONSUME_TOKEN(TOKEN_RPAREN);
    CONSUME_TOKEN(TOKEN_SEMICOLON);
    return create_ast_node(AST_VA_START, 0, NULL, va_list, last_arg, NULL);
}

static ASTNode *parse_va_arg_expression(void) {
    CONSUME_TOKEN(TOKEN_VA_ARG);
    CONSUME_TOKEN(TOKEN_LPAREN);
    ASTNode *va_list = parse_expression();
    if (!va_list) return NULL;
    CONSUME_TOKEN(TOKEN_COMMA);
    Type *arg_type = parse_type_specifier();
    if (!arg_type) {
        free_ast_node(va_list);
        return NULL;
    }
    CONSUME_TOKEN(TOKEN_RPAREN);
    ASTNode *node = create_ast_node(AST_VA_ARG, 0, NULL, va_list, NULL, NULL);
    if (node) node->variable_type = arg_type;
    return node;
}

static ASTNode *parse_va_end(void) {
    CONSUME_TOKEN(TOKEN_VA_END);
    CONSUME_TOKEN(TOKEN_LPAREN);
    ASTNode *va_list = parse_expression();
    if (!va_list) return NULL;
    CONSUME_TOKEN(TOKEN_RPAREN);
    CONSUME_TOKEN(TOKEN_SEMICOLON);
    return create_ast_node(AST_VA_END, 0, NULL, va_list, NULL, NULL);
}

static ASTNode *parse_return_statement(void) {
    CONSUME_TOKEN(TOKEN_RETURN);
    ASTNode *expression = NULL;
    if (!CURRENT_TOKEN_TYPE_MATCHES(TOKEN_SEMICOLON)) expression = parse_expression();
    CONSUME_TOKEN(TOKEN_SEMICOLON);
    return create_ast_node(AST_RETURN, 0, NULL, expression, NULL, NULL);
}

static ASTNode *parse_break_statement(void) {
    CONSUME_TOKEN(TOKEN_BREAK);
    CONSUME_TOKEN(TOKEN_SEMICOLON);
    return create_ast_node(AST_BREAK, 0, NULL, NULL, NULL, NULL);
}

static ASTNode *parse_continue_statement(void) {
    CONSUME_TOKEN(TOKEN_CONTINUE);
    CONSUME_TOKEN(TOKEN_SEMICOLON);
    return create_ast_node(AST_CONTINUE, 0, NULL, NULL, NULL, NULL);
}

static ASTNode *parse_free_statement(void) {
    CONSUME_TOKEN(TOKEN_FREE);
    CONSUME_TOKEN(TOKEN_LPAREN);
    ASTNode *expression = parse_expression();
    if (!expression) return NULL;
    CONSUME_TOKEN(TOKEN_RPAREN);
    CONSUME_TOKEN(TOKEN_SEMICOLON);
    return create_ast_node(AST_FREE, 0, NULL, expression, NULL, NULL);
}

static ASTNode *parse_function_call_statement(void) {
    CONSUME_TOKEN(TOKEN_DOT);
    
    if (!CURRENT_TOKEN_TYPE_MATCHES(TOKEN_ID)) {
        REPORT_PARSE_ERROR("Expected function name after '.'");
        return NULL;
    }
    
    Token *func_name_token = get_current_token();
    char *func_name = strdup(func_name_token->value);
    advance_token();
    
    ASTNode *args = NULL;
    if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_LPAREN)) {
        advance_token();
        AST *arg_list = malloc(sizeof(AST));
        if (!arg_list) {
            free(func_name);
            return NULL;
        }
        arg_list->nodes = NULL;
        arg_list->count = 0;
        arg_list->capacity = 0;
        
        while (!CURRENT_TOKEN_TYPE_MATCHES(TOKEN_RPAREN) && !CURRENT_TOKEN_TYPE_MATCHES(TOKEN_EOF)) {
            ASTNode *arg = parse_expression();
            if (!arg) {
                free_ast(arg_list);
                free(func_name);
                return NULL;
            }
            if (!add_ast_node_to_list(arg_list, arg)) {
                free_ast_node(arg);
                free_ast(arg_list);
                free(func_name);
                return NULL;
            }
            if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_COMMA)) {
                advance_token();
            } else {
                break;
            }
        }
        CONSUME_TOKEN(TOKEN_RPAREN);
        args = create_ast_node(AST_BLOCK, 0, NULL, NULL, NULL, (ASTNode*)arg_list);
    }
    
    CONSUME_TOKEN(TOKEN_SEMICOLON);
    return create_ast_node(AST_FUNCTION_CALL_STATEMENT, 0, func_name, args, NULL, NULL);
}

static ASTNode *parse_expression(void) {
    return parse_assignment_expression();
}

static ASTNode *parse_assignment_expression(void) {
    ASTNode *left = parse_logical_or_expression();
    if (!left) return NULL;
    
    if (
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_DOUBLE_PLUS) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_DOUBLE_MINUS) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_EQUAL) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_PLUS_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_MINUS_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_STAR_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_SLASH_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_PERCENT_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_DOUBLE_STAR_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_PIPE_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_AMPERSAND_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_CARET_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_TILDE_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_SHL_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_SHR_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_SAL_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_SAR_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_ROL_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_ROR_EQ)
    ) {
        
        TokenType operation = get_current_token_type();
        advance_token();
        ASTNode *right = parse_assignment_expression();
        if (!right) {
            free_ast_node(left);
            return NULL;
        }
        
        if (operation == TOKEN_EQUAL) {
            return create_ast_node(AST_ASSIGNMENT, operation, NULL, left, right, NULL);
        }
        return create_ast_node(AST_COMPOUND_ASSIGNMENT, operation, NULL, left, right, NULL);
    }
    return left;
}

static ASTNode *parse_binary_operation(ASTNode *(*next_parser)(void), TokenType operation1, TokenType operation2) {
    ASTNode *node = next_parser();
    if (!node) return NULL;
    
    while (CURRENT_TOKEN_TYPE_MATCHES(operation1) || CURRENT_TOKEN_TYPE_MATCHES(operation2)) {
        TokenType operation = get_current_token_type();
        advance_token();
        ASTNode *right = next_parser();
        if (!right) {
            free_ast_node(node);
            return NULL;
        }
        node = create_ast_node(AST_BINARY_OPERATION, operation, NULL, node, right, NULL);
        if (!node) {
            free_ast_node(right);
            return NULL;
        }
    }
    return node;
}

static ASTNode *parse_logical_or_expression(void) {
    return parse_binary_operation(parse_logical_xor_expression, TOKEN_DOUBLE_PIPE, TOKEN_ERROR);
}

static ASTNode *parse_logical_xor_expression(void) {
    return parse_binary_operation(parse_logical_and_expression, TOKEN_DOUBLE_CARET, TOKEN_ERROR);
}

static ASTNode *parse_logical_and_expression(void) {
    return parse_binary_operation(parse_bitwise_or_expression, TOKEN_DOUBLE_AMPERSAND, TOKEN_ERROR);
}

static ASTNode *parse_bitwise_or_expression(void) {
    return parse_binary_operation(parse_bitwise_xor_expression, TOKEN_PIPE, TOKEN_ERROR);
}

static ASTNode *parse_bitwise_xor_expression(void) {
    return parse_binary_operation(parse_bitwise_and_expression, TOKEN_CARET, TOKEN_ERROR);
}

static ASTNode *parse_bitwise_and_expression(void) {
    return parse_binary_operation(parse_equality_expression, TOKEN_AMPERSAND, TOKEN_ERROR);
}

static ASTNode *parse_equality_expression(void) {
    return parse_binary_operation(parse_relational_expression, TOKEN_DOUBLE_EQ, TOKEN_NE);
}

static ASTNode *parse_relational_expression(void) {
    return parse_binary_operation(parse_shift_expression, TOKEN_LT, TOKEN_GT);
}

static ASTNode *parse_shift_expression(void) {
    return parse_binary_operation(parse_additive_expression, TOKEN_SHL, TOKEN_SHR);
}

static ASTNode *parse_additive_expression(void) {
    return parse_binary_operation(parse_multiplicative_expression, TOKEN_PLUS, TOKEN_MINUS);
}

static ASTNode *parse_multiplicative_expression(void) {
    return parse_binary_operation(parse_exponent_expression, TOKEN_STAR, TOKEN_SLASH);
}

static ASTNode *parse_exponent_expression(void) {
    return parse_binary_operation(parse_unary_expression, TOKEN_DOUBLE_STAR, TOKEN_ERROR);
}

static ASTNode *parse_unary_expression(void) {
    if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_PLUS) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_MINUS) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_BANG) || 
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_TILDE) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_AMPERSAND) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_AMPERSAND) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_DOUBLE_PLUS) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_DOUBLE_MINUS) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_DOUBLE_PLUS) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_DOUBLE_MINUS) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_STAR) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_DOUBLE_STAR) ||
        CURRENT_TOKEN_TYPE_MATCHES(TOKEN_SLASH)) {
        TokenType operation = get_current_token_type();
        advance_token();
        ASTNode *operand = parse_unary_expression();
        if (!operand) return NULL;
        return create_ast_node(AST_UNARY_OPERATION, operation, NULL, NULL, operand, NULL);
    }
    ASTNode *primary = parse_primary_expression();
    if (!primary) return NULL;
    return parse_postfix_expression(primary);
}

static ASTNode *parse_primary_expression(void) {
    Token *token = get_current_token();
    if (!token) {
        REPORT_PARSE_ERROR("Unexpected end of input");
        return NULL;
    }
    
    switch (token->type) {
        case TOKEN_SELF:
        case TOKEN_PUBLIC: {
            ASTNodeType qualifier_type = (token->type == TOKEN_SELF) ? AST_SELF : AST_PUBLIC;
            advance_token();
            CONSUME_TOKEN(TOKEN_DOUBLE_COLON);
            ASTNode *target = parse_primary_expression();
            return create_ast_node(qualifier_type, 0, NULL, target, NULL, NULL);
        }
        case TOKEN_STAR: {
            advance_token();
            Token *identifier_token = get_current_token();
            if (identifier_token->type != TOKEN_ID) {
                REPORT_PARSE_ERROR("Expected identifier after '*'");
                return NULL;
            }
            char *variable_name = strdup(identifier_token->value);
            advance_token();
            return create_ast_node(AST_POINTER_VARIABLE, 0, variable_name, NULL, NULL, NULL);
        }
        case TOKEN_ID: {
            char *value = strdup(token->value);
            advance_token();
            
            ASTNode *identifier_node = create_ast_node(AST_IDENTIFIER, 0, value, NULL, NULL, NULL);
            return parse_postfix_expression(identifier_node);
        }
        case TOKEN_DOLLAR: {
            advance_token();
            Token *identifier_token = get_current_token();
            if (identifier_token->type != TOKEN_ID) {
                REPORT_PARSE_ERROR("Expected identifier after '$'");
                return NULL;
            }
            char *variable_name = strdup(identifier_token->value);
            advance_token();
            ASTNode *variable_node = create_ast_node(AST_VARIABLE, 0, variable_name, NULL, NULL, NULL);
            return parse_postfix_expression(variable_node);
        }
        case TOKEN_LPAREN: {
            CONSUME_TOKEN(TOKEN_LPAREN);
            ASTNode *expression = parse_expression();
            if (!expression) return NULL;
            CONSUME_TOKEN(TOKEN_RPAREN);
            return parse_postfix_expression(expression);
        }
        case TOKEN_LCURLY: {
            return parse_array_initializer();
        }
        case TOKEN_ALLOC:
        case TOKEN_REALLOC:
        case TOKEN_PUSH:
        case TOKEN_SIZEOF:
        case TOKEN_PARSEOF: {
            ASTNodeType node_type;
            switch (token->type) {
                case TOKEN_ALLOC: node_type = AST_ALLOC; break;
                case TOKEN_REALLOC: node_type = AST_REALLOC; break;
                case TOKEN_PUSH: node_type = AST_PUSH; break;
                case TOKEN_SIZEOF: node_type = AST_SIZEOF; break;
                case TOKEN_PARSEOF: node_type = AST_PARSEOF; break;
                default: return NULL;
            }
            advance_token();
            CONSUME_TOKEN(TOKEN_LPAREN);
            ASTNode *arguments = parse_expression();
            if (!arguments) return NULL;
            CONSUME_TOKEN(TOKEN_RPAREN);
            return create_ast_node(node_type, 0, NULL, arguments, NULL, NULL);
        }
        case TOKEN_POP: {
            advance_token();
            return create_ast_node(AST_POP, 0, NULL, NULL, NULL, NULL);
        }
        case TOKEN_STACK: {
            advance_token();
            return create_ast_node(AST_STACK, 0, NULL, NULL, NULL, NULL);
        }
        case TOKEN_NUMBER:
        case TOKEN_STRING:
        case TOKEN_CHAR: {
            char *value = strdup(token->value);
            TokenType type = token->type;
            advance_token();
            return create_ast_node(AST_LITERAL_VALUE, type, value, NULL, NULL, NULL);
        }
        case TOKEN_DOUBLE_DOLLAR: {
            advance_token();
            Token *identifier_token = get_current_token();
            if (identifier_token->type != TOKEN_ID) {
                REPORT_PARSE_ERROR("Expected structure name after 'struct'");
                return NULL;
            }
            char *structure_name = strdup(identifier_token->value);
            advance_token();
            return create_ast_node(AST_STRUCT_INSTANCE, 0, structure_name, NULL, NULL, NULL);
        }
        case TOKEN_TYPE: {
            Type *cast_type = parse_type_specifier();
            if (!cast_type) return NULL;
            
            if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_LPAREN)) {
                CONSUME_TOKEN(TOKEN_LPAREN);
                ASTNode *expression = parse_expression();
                if (!expression) {
                    free_type(cast_type);
                    return NULL;
                }
                CONSUME_TOKEN(TOKEN_RPAREN);
                ASTNode *node = create_ast_node(AST_CAST, 0, NULL, expression, NULL, NULL);
                if (node) node->variable_type = cast_type;
                return node;
            }
            REPORT_PARSE_ERROR("Expected '(' for type cast");
            free_type(cast_type);
            return NULL;
        }
        case TOKEN_VA_ARG:
            return parse_va_arg_expression();
        default:
            REPORT_PARSE_ERROR("Unexpected token in expression: %s", token_names[token->type]);
            return NULL;
    }
}

static ASTNode *parse_variable_declaration(void) {
    TokenType sigil = get_current_token_type();
    advance_token();
    
    AST *variable_list = malloc(sizeof(AST));
    if (!variable_list) return NULL;
    variable_list->nodes = NULL;
    variable_list->count = 0;
    variable_list->capacity = 0;
    
    int array_size = -1;
    
    while (1) {
        if (!CURRENT_TOKEN_TYPE_MATCHES(TOKEN_ID)) {
            REPORT_PARSE_ERROR("Expected identifier");
            free_ast(variable_list);
            return NULL;
        }
        
        Token *identifier_token = get_current_token();
        char *variable_name = strdup(identifier_token->value);
        advance_token();
        
        if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_LBRACKET)) {
            if (array_size != -1) {
                REPORT_PARSE_ERROR("Multiple array declarations in single statement");
                free(variable_name);
                free_ast(variable_list);
                return NULL;
            }
            advance_token();
            if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_NUMBER)) {
                Token *size_token = get_current_token();
                array_size = atoi(size_token->value);
                advance_token();
            } else if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_RBRACKET)) {
                array_size = 0;
            } else {
                REPORT_PARSE_ERROR("Expected number or ']' after '['");
                free(variable_name);
                free_ast(variable_list);
                return NULL;
            }
            CONSUME_TOKEN(TOKEN_RBRACKET);
        }
        
        ASTNode *variable_node = create_ast_node(AST_IDENTIFIER, 0, variable_name, NULL, NULL, NULL);
        if (!add_ast_node_to_list(variable_list, variable_node)) {
            free(variable_name);
            free_ast(variable_list);
            return NULL;
        }
        
        if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_COMMA)) advance_token();
        else break;
    }
    
    if (array_size != -1 && variable_list->count > 1) {
        REPORT_PARSE_ERROR("Array declaration in multiple variable declaration is not allowed");
        free_ast(variable_list);
        return NULL;
    }
    
    Type *variable_type = parse_type_specifier();
    if (!variable_type) {
        free_ast(variable_list);
        return NULL;
    }
    
    if (array_size != -1) {
        variable_type->is_array = 1;
        variable_type->array_size = array_size;
    }

    if (sigil == TOKEN_AT || sigil == TOKEN_DOUBLE_AT) {
        variable_type->pointer_level = 1;
        variable_type->is_reference = 0;
    } else if (sigil == TOKEN_AMPERSAND) {
        variable_type->is_reference = 1;
        variable_type->pointer_level = 0;
    } else if (sigil == TOKEN_DOLLAR) {
        variable_type->pointer_level = 0;
        variable_type->is_reference = 0;
    }
    
    AST *initializer_list = NULL;
    if (ATTEMPT_CONSUME_TOKEN(TOKEN_EQUAL)) {
        initializer_list = malloc(sizeof(AST));
        if (!initializer_list) {
            free_type(variable_type);
            free_ast(variable_list);
            return NULL;
        }
        initializer_list->nodes = NULL;
        initializer_list->count = 0;
        initializer_list->capacity = 0;
        
        if (variable_type->is_array) {
            ASTNode *initializer = parse_array_initializer();
            if (!initializer) {
                free_ast(initializer_list);
                free_type(variable_type);
                free_ast(variable_list);
                return NULL;
            }
            if (!add_ast_node_to_list(initializer_list, initializer)) {
                free_ast_node(initializer);
                free_ast(initializer_list);
                free_type(variable_type);
                free_ast(variable_list);
                return NULL;
            }
        } else {
            while (1) {
                ASTNode *expression = parse_expression();
                if (!expression) {
                    free_ast(initializer_list);
                    free_type(variable_type);
                    free_ast(variable_list);
                    return NULL;
                }
                
                if (!add_ast_node_to_list(initializer_list, expression)) {
                    free_ast_node(expression);
                    free_ast(initializer_list);
                    free_type(variable_type);
                    free_ast(variable_list);
                    return NULL;
                }
                
                if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_COMMA)) advance_token();
                else break;
            }
        }
    }
    
    CONSUME_TOKEN(TOKEN_SEMICOLON);
    
    ASTNode *multi_declaration = create_ast_node(AST_MULTI_DECLARATION, 0, NULL, (ASTNode*)variable_list, (ASTNode*)initializer_list, NULL);
    if (!multi_declaration) {
        free_ast(variable_list);
        if (initializer_list) free_ast(initializer_list);
        free_type(variable_type);
        return NULL;
    }
    multi_declaration->variable_type = variable_type;
    multi_declaration->declaration_sigil = sigil;
    return multi_declaration;
}

static ASTNode *parse_struct_declaration(void) {
    CONSUME_TOKEN(TOKEN_DOUBLE_DOLLAR);
    Token *identifier_token = get_current_token();
    if (identifier_token->type != TOKEN_ID) {
        REPORT_PARSE_ERROR("Expected structure name after 'struct'");
        return NULL;
    }
    char *structure_name = strdup(identifier_token->value);
    advance_token();
    
    ASTNode *body = parse_block_statement();
    if (!body) {
        free(structure_name);
        return NULL;
    }
    return create_ast_node(AST_STRUCT_DECLARATION, 0, structure_name, NULL, body, NULL);
}

static ASTNode *parse_multi_assignment(void) {
    AST *lvalue_list = malloc(sizeof(AST));
    if (!lvalue_list) return NULL;
    lvalue_list->nodes = NULL;
    lvalue_list->count = 0;
    lvalue_list->capacity = 0;

    while (1) {
        ASTNode *lvalue = parse_primary_expression();
        if (!lvalue) {
            free_ast(lvalue_list);
            return NULL;
        }

        if (!add_ast_node_to_list(lvalue_list, lvalue)) {
            free_ast_node(lvalue);
            free_ast(lvalue_list);
            return NULL;
        }

        if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_COMMA)) advance_token();
        else break;
    }

    if (!CURRENT_TOKEN_TYPE_MATCHES(TOKEN_EQUAL)) {
        REPORT_PARSE_ERROR("Expected '=' after lvalue list");
        free_ast(lvalue_list);
        return NULL;
    }
    advance_token();

    AST *rvalue_list = malloc(sizeof(AST));
    if (!rvalue_list) {
        free_ast(lvalue_list);
        return NULL;
    }
    rvalue_list->nodes = NULL;
    rvalue_list->count = 0;
    rvalue_list->capacity = 0;

    while (1) {
        ASTNode *expression = parse_expression();
        if (!expression) {
            free_ast(rvalue_list);
            free_ast(lvalue_list);
            return NULL;
        }

        if (!add_ast_node_to_list(rvalue_list, expression)) {
            free_ast_node(expression);
            free_ast(rvalue_list);
            free_ast(lvalue_list);
            return NULL;
        }

        if (CURRENT_TOKEN_TYPE_MATCHES(TOKEN_COMMA)) advance_token();
        else break;
    }

    CONSUME_TOKEN(TOKEN_SEMICOLON);
    return create_ast_node(AST_MULTI_ASSIGNMENT, 0, NULL, (ASTNode*)lvalue_list, (ASTNode*)rvalue_list, NULL);
}

static ASTNode *parse_preprocessor_directive(void) {
    CONSUME_TOKEN(TOKEN_PERCENT);
    
    if (get_current_token() == NULL) {
        REPORT_PARSE_ERROR("Expected token after '%'");
        return NULL;
    }
    
    Token *directive_token = get_current_token();
    char *directive_name = strdup(directive_token->value);
    advance_token();
    
    char *arguments_string = NULL;
    size_t arguments_length = 0;
    int current_line = directive_token->line;
    
    while (get_current_token() != NULL && get_current_token()->line == current_line) {
        Token *token = get_current_token();
        if (arguments_string == NULL) {
            arguments_string = strdup(token->value);
            arguments_length = strlen(token->value);
        } else {
            arguments_string = realloc(arguments_string, arguments_length + 1 + strlen(token->value) + 1);
            strcat(arguments_string, " ");
            strcat(arguments_string, token->value);
            arguments_length += 1 + strlen(token->value);
        }
        advance_token();
    }
    
    ASTNode *node = create_ast_node(AST_PREPROCESSOR_DIRECTIVE, 0, directive_name, NULL, NULL, NULL);
    if (arguments_string) {
        ASTNode *arguments_node = create_ast_node(AST_LITERAL_VALUE, TOKEN_STRING, arguments_string, NULL, NULL, NULL);
        node->left = arguments_node;
    }
    
    return node;
}

static ASTNode *parse_statement(void) {
    Token *token = get_current_token();
    if (!token) {
        REPORT_PARSE_ERROR("Unexpected end of input");
        return NULL;
    }
    
    switch (token->type) {
        case TOKEN_PERCENT:
            return parse_preprocessor_directive();
        case TOKEN_DOLLAR:
        case TOKEN_AT:
        case TOKEN_DOUBLE_AT:
        case TOKEN_AMPERSAND:
            return parse_variable_declaration();
        case TOKEN_IF:
            return parse_if_statement();
        case TOKEN_CPU:
            return parse_cpu();
        case TOKEN_DOT: {
            int saved_pos = current_token_position;
            ASTNode *func_call = parse_function_call_statement();
            if (func_call) return func_call;
            current_token_position = saved_pos;
            return parse_function_declaration();
        }
        case TOKEN_DOUBLE_DOLLAR:
            return parse_struct_declaration();
        case TOKEN_ID: {
            if (current_token_position + 1 < total_tokens && 
                token_stream[current_token_position + 1].type == TOKEN_COLON) {
                return parse_label_declaration();
            }
            ASTNode *expression = parse_expression();
            if (expression) {
                if (!expect_token(TOKEN_SEMICOLON)) {
                    free_ast_node(expression);
                    return NULL;
                }
            }
            return expression;
        }
        case TOKEN_RETURN:
            return parse_return_statement();
        case TOKEN_BREAK:
            return parse_break_statement();
        case TOKEN_CONTINUE:
            return parse_continue_statement();
        case TOKEN_FREE:
            return parse_free_statement();
        case TOKEN_GOTO:
            return parse_goto_statement();
        case TOKEN_VA_START:
            return parse_va_start();
        case TOKEN_VA_ARG: {
            ASTNode *node = parse_va_arg_expression();
            if (!node) return NULL;
            CONSUME_TOKEN(TOKEN_SEMICOLON);
            return node;
        }
        case TOKEN_VA_END:
            return parse_va_end();
        case TOKEN_LCURLY:
            return parse_block_statement();
        default: {
            ASTNode *expression = parse_expression();
            if (expression) {
                if (!expect_token(TOKEN_SEMICOLON)) {
                    free_ast_node(expression);
                    return NULL;
                }
            }
            return expression;
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
        case AST_IF_STATEMENT:
            free_ast_node(node->left);
            free_ast_node(node->right);
            if (node->extra) free_ast_node(node->extra);
            break;
        case AST_FUNCTION:
            free_ast_node(node->left);
            free_ast_node(node->right);
            break;
        case AST_FUNCTION_CALL:
        case AST_FUNCTION_CALL_STATEMENT:
        case AST_ALLOC:
        case AST_REALLOC:
        case AST_FREE:
        case AST_RETURN:
        case AST_POP:
        case AST_PUSH:
        case AST_SIZEOF:
        case AST_PARSEOF:
        case AST_STRUCT_DECLARATION:
        case AST_VA_START:
        case AST_VA_END:
            free_ast_node(node->left);
            free_ast_node(node->right);
            break;
        case AST_VA_ARG:
            free_ast_node(node->left);
            if (node->variable_type) free_type(node->variable_type);
            break;
        case AST_SELF:
        case AST_PUBLIC:
            free_ast_node(node->left);
            break;
        case AST_CPU:
            if (node->left) {
                AST *args = (AST*)node->left;
                for (int i = 0; i < args->count; i++) free_ast_node(args->nodes[i]);
                free(args->nodes);
                free(args);
            }
            free(node->value);
            break;
        case AST_MULTI_DECLARATION:
            if (node->left) {
                AST *variable_list = (AST*)node->left;
                for (int i = 0; i < variable_list->count; i++) free_ast_node(variable_list->nodes[i]);
                free(variable_list->nodes);
                free(variable_list);
            }
            if (node->right) {
                AST *initializer_list = (AST*)node->right;
                for (int i = 0; i < initializer_list->count; i++) free_ast_node(initializer_list->nodes[i]);
                free(initializer_list->nodes);
                free(initializer_list);
            }
            break;
        case AST_MULTI_ASSIGNMENT:
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
        case AST_ARRAY_INITIALIZER:
            if (node->extra) {
                AST *list = (AST*)node->extra;
                for (int i = 0; i < list->count; i++) free_ast_node(list->nodes[i]);
                free(list->nodes);
                free(list);
            }
            break;
        case AST_PREPROCESSOR_DIRECTIVE:
            free_ast_node(node->left);
            break;
        case AST_ARRAY_ACCESS:
            free_ast_node(node->left);
            free_ast_node(node->right);
            break;
        case AST_LABEL_DECLARATION:
        case AST_GOTO:
            free(node->value);
            break;
        case AST_POSTFIX_CAST:
            free_ast_node(node->left);
            if (node->variable_type) free_type(node->variable_type);
            break;
        case AST_POSTFIX_INCREMENT:
        case AST_POSTFIX_DECREMENT:
            free_ast_node(node->left);
            break;
        default:
            free_ast_node(node->left);
            free_ast_node(node->right);
            free_ast_node(node->extra);
            break;
    }
    
    free(node->value);
    if (node->variable_type) free_type(node->variable_type);
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
    token_stream = input_tokens;
    total_tokens = input_token_count;
    current_token_position = 0;

    if (total_tokens == 0 || (total_tokens == 1 && token_stream[0].type == TOKEN_EOF)) {
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
    
    while (get_current_token_type() != TOKEN_EOF && !has_errors()) {
        ASTNode *node = parse_statement();
        if (node) {
            if (!add_ast_node_to_list(ast, node)) {
                free_ast_node(node);
                free_ast(ast);
                return NULL;
            }
        } else if (has_errors()) break;
        else advance_token();
    }
    
    if (has_errors()) {
        free_ast(ast);
        return NULL;
    }
    
    return ast;
}

