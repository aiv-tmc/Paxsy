#include "parser.h"
#include "../lexer/lexer.h"
#include "../error_manager/error_manager.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#define MAX_MODIFIERS 8
#define AST_INITIAL_CAPACITY 8
#define MAX_REGIS 6

/* Forward declarations for internal functions */
static TokenType get_current_token_type(ParserState *state);
static void advance_token(ParserState *state);
static bool expect_token(ParserState *state, TokenType expected_type);
static Token *get_current_token(ParserState *state);
static ASTNode *parse_statement(ParserState *state);
static ASTNode *parse_expression(ParserState *state);
static Type *parse_type_specifier_silent(ParserState *state, bool silent, bool parse_prefixes);
static Type *parse_type_specifier(ParserState *state, bool parse_prefixes);
static Type *try_parse_type_specifier(ParserState *state, bool parse_prefixes);
static ASTNode *parse_parameter_list(ParserState *state, bool *is_variadic);
static ASTNode *parse_array_initializer(ParserState *state);
static void print_ast_node(ASTNode *node, int indent);
static ASTNode *parse_postfix_expression(ParserState *state, ASTNode *node);
static ASTNode *parse_function_parameter(ParserState *state);
static ASTNode *parse_function_call(ParserState *state);
static ASTNode *parse_org_statement(ParserState *state);
static ASTNode *parse_use_expression(ParserState *state);
static ASTNode *parse_parseof_statement(ParserState *state);
static ASTNode *parse_push_statement(ParserState *state);
static ASTNode *parse_struct_object_declaration(ParserState *state);
static ASTNode *parse_struct_object_access(ParserState *state);
static char *parse_qualified_name(ParserState *state);
static void apply_prefixes_to_type(Type *type, uint8_t pointer_level, uint8_t is_reference);

/* Helper macros for cleaner code */
#define CURRENT_TOKEN_TYPE_MATCHES(state, token) (get_current_token_type(state) == (token))
#define CONSUME_TOKEN(state, token) do { if (!expect_token(state, token)) return NULL; } while(0)
#define ATTEMPT_CONSUME_TOKEN(state, token) (CURRENT_TOKEN_TYPE_MATCHES(state, token) ? (advance_token(state), 1) : 0)

#define REPORT_PARSE_ERROR(state, ...) do { \
    Token *current = get_current_token(state); \
    if (current) report_error(current->line, current->column, __VA_ARGS__); \
    else report_error(0, 0, __VA_ARGS__); \
} while(0)

/* Get current token type */
static TokenType get_current_token_type(ParserState *state) {
    Token *token = get_current_token(state);
    return token ? token->type : TOKEN_EOF;
}

/* Advance to next token */
static void advance_token(ParserState *state) {
    if (state->current_token_position < state->total_tokens - 1) {
        state->current_token_position++;
    }
}

/* Get current token pointer */
static Token *get_current_token(ParserState *state) {
    if (state->current_token_position < state->total_tokens) {
        return &state->token_stream[state->current_token_position];
    }
    return NULL;
}

/* Expect specific token type and advance, report error if mismatch */
static bool expect_token(ParserState *state, TokenType expected_type) {
    if (CURRENT_TOKEN_TYPE_MATCHES(state, expected_type)) {
        advance_token(state);
        return true;
    }
    
    TokenType actual = get_current_token_type(state);
    const char *expected_name = token_names[expected_type];
    const char *actual_name = (actual == TOKEN_EOF) ? "EOF" : token_names[actual];
    
    REPORT_PARSE_ERROR(state, "Expected %s but got %s", expected_name, actual_name);
    return false;
}

/* Parse qualified name with :: separator */
static char *parse_qualified_name(ParserState *state) {
    if (!CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_ID)) {
        return NULL;
    }

    Token *first_token = get_current_token(state);
    char *name = strdup(first_token->value);
    if (!name) return NULL;
    
    advance_token(state);

    while (ATTEMPT_CONSUME_TOKEN(state, TOKEN_DOUBLE_COLON)) {
        if (!CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_ID)) {
            free(name);
            return NULL;
        }
        Token *next_token = get_current_token(state);
        size_t name_len = strlen(name);
        size_t next_len = strlen(next_token->value);
        char *new_name = malloc(name_len + 2 + next_len + 1);
        if (!new_name) {
            free(name);
            return NULL;
        }
        memcpy(new_name, name, name_len);
        memcpy(new_name + name_len, "::", 2);
        memcpy(new_name + name_len + 2, next_token->value, next_len + 1);
        free(name);
        name = new_name;
        advance_token(state);
    }

    return name;
}

/* Apply pointer/reference prefixes to type */
static void apply_prefixes_to_type(Type *type, uint8_t pointer_level, uint8_t is_reference) {
    type->pointer_level += pointer_level;
    if (is_reference) type->is_reference = is_reference;
}

/* Parse type specifier with optional silent mode */
static Type *parse_type_specifier_silent(ParserState *state, bool silent, bool parse_prefixes) {
    Type *type = malloc(sizeof(Type));
    if (!type) {
        if (!silent) REPORT_PARSE_ERROR(state, "Memory allocation failed for type specifier");
        return NULL;
    }
    
    // Initialize type structure
    memset(type, 0, sizeof(Type));
    
    // Parse modifiers
    while (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_MODIFIER) && type->modifier_count < MAX_MODIFIERS) {
        Token *modifier_token = get_current_token(state);
        char **new_modifiers = realloc(type->modifiers, (type->modifier_count + 1) * sizeof(char*));
        if (!new_modifiers) {
            if (!silent) REPORT_PARSE_ERROR(state, "Memory allocation failed for modifiers");
            free_type(type);
            return NULL;
        }
        type->modifiers = new_modifiers;
        type->modifiers[type->modifier_count] = strdup(modifier_token->value);
        if (!type->modifiers[type->modifier_count]) {
            if (!silent) REPORT_PARSE_ERROR(state, "Memory allocation failed for modifier string");
            free_type(type);
            return NULL;
        }
        type->modifier_count++;
        advance_token(state);
    }

    // Parse left number if present
    if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_NUMBER)) {
        Token *number_token = get_current_token(state);
        int value = atoi(number_token->value);
        if (value < 0 || value > 255) {
            if (!silent) REPORT_PARSE_ERROR(state, "Number out of range (0-255)");
            free_type(type);
            return NULL;
        }
        type->left_number = (uint8_t)value;
        advance_token(state);
    }

    // Parse type name or default to int
    if (get_current_token_type(state) != TOKEN_TYPE) {
        if (!silent) {
            free_type(type);
            return NULL;
        }
        type->name = strdup("int");
    } else {
        Token *type_token = get_current_token(state);
        type->name = strdup(type_token->value);
        advance_token(state);
    }

    // Parse template parameters if present
    if (ATTEMPT_CONSUME_TOKEN(state, TOKEN_LT)) {
        if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_NUMBER)) {
            Token *number_token = get_current_token(state);
            int value = atoi(number_token->value);
            if (value < 0 || value > 255) {
                if (!silent) REPORT_PARSE_ERROR(state, "Number out of range (0-255)");
                free_type(type);
                return NULL;
            }
            type->right_number = (uint8_t)value;
            type->has_right_id = 0;
            advance_token(state);
        } else if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_ID)) {
            Token *id_token = get_current_token(state);
            type->right_id = strdup(id_token->value);
            type->has_right_id = 1;
            advance_token(state);
        } else {
            if (!silent) REPORT_PARSE_ERROR(state, "Expected number or identifier after '<' in type specifier");
            free_type(type);
            return NULL;
        }
        CONSUME_TOKEN(state, TOKEN_GT);
    }
    
    // Parse pointer/reference prefixes after type and template parameters
    if (parse_prefixes) {
        while (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_AT) ||
               CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_DOUBLE_AT) ||
               CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_AMPERSAND)) {
            if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_AT) || CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_DOUBLE_AT)) {
                type->pointer_level++;
            } else if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_AMPERSAND)) {
                type->is_reference = 1;
            }
            advance_token(state);
        }
    }
    
    return type;
}

/* Parse type specifier with error reporting */
static Type *parse_type_specifier(ParserState *state, bool parse_prefixes) {
    return parse_type_specifier_silent(state, false, parse_prefixes);
}

/* Try to parse type specifier without error reporting */
static Type *try_parse_type_specifier(ParserState *state, bool parse_prefixes) {
    return parse_type_specifier_silent(state, true, parse_prefixes);
}

/* Create new AST node */
static ASTNode *create_ast_node(ASTNodeType node_type, TokenType operation_type, 
    char *node_value, ASTNode *left_child, ASTNode *right_child, ASTNode *extra_node) {
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    // Initialize node with zeros
    memset(node, 0, sizeof(ASTNode));
    node->type = node_type;
    node->operation_type = operation_type;
    node->value = node_value;
    node->left = left_child;
    node->right = right_child;
    node->extra = extra_node;
    node->declaration_sigil = TOKEN_ERROR;
    
    return node;
}

/* Add AST node to list with dynamic resizing */
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

// Forward declarations for expression parsing
static ASTNode *parse_assignment_expression(ParserState *state);
static ASTNode *parse_logical_or_expression(ParserState *state);
static ASTNode *parse_logical_xor_expression(ParserState *state);
static ASTNode *parse_logical_and_expression(ParserState *state);
static ASTNode *parse_bitwise_or_expression(ParserState *state);
static ASTNode *parse_bitwise_xor_expression(ParserState *state);
static ASTNode *parse_bitwise_and_expression(ParserState *state);
static ASTNode *parse_equality_expression(ParserState *state);
static ASTNode *parse_relational_expression(ParserState *state);
static ASTNode *parse_shift_expression(ParserState *state);
static ASTNode *parse_additive_expression(ParserState *state);
static ASTNode *parse_multiplicative_expression(ParserState *state);
static ASTNode *parse_unary_expression(ParserState *state);
static ASTNode *parse_primary_expression(ParserState *state);

/* Parse postfix expressions (function calls, array access, etc.) */
static ASTNode *parse_postfix_expression(ParserState *state, ASTNode *node) {
    while (1) {
        if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_LPAREN)) {
            advance_token(state);
            ASTNode *arguments = NULL;
            if (!CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_RPAREN)) {
                arguments = parse_expression(state);
                if (!arguments) {
                    free_ast_node(node);
                    return NULL;
                }
            }
            CONSUME_TOKEN(state, TOKEN_RPAREN);
            node = create_ast_node(AST_FUNCTION_CALL, 0, node->value, arguments, NULL, NULL);
        } else if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_LBRACKET)) {
            advance_token(state);
            ASTNode *index = parse_expression(state);
            if (!index) {
                free_ast_node(node);
                return NULL;
            }
            CONSUME_TOKEN(state, TOKEN_RBRACKET);
            node = create_ast_node(AST_ARRAY_ACCESS, 0, NULL, node, index, NULL);
        } else if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_ARROW)) {
            advance_token(state);
            if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_LPAREN)) {
                advance_token(state);
                Type *target_type = parse_type_specifier(state, true);
                if (!target_type) {
                    free_ast_node(node);
                    return NULL;
                }
                CONSUME_TOKEN(state, TOKEN_RPAREN);
                node = create_ast_node(AST_POSTFIX_CAST, 0, NULL, node, NULL, NULL);
                node->variable_type = target_type;
            } else if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_ID)) {
                Token *field_token = get_current_token(state);
                char *field_name = strdup(field_token->value);
                advance_token(state);
                node = create_ast_node(AST_FIELD_ACCESS, 0, field_name, node, NULL, NULL);
            } else {
                REPORT_PARSE_ERROR(state, "Expected '(' for cast or identifier for field access after '->'");
                free_ast_node(node);
                return NULL;
            }
        } else if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_DOUBLE_PLUS)) {
            advance_token(state);
            node = create_ast_node(AST_POSTFIX_INCREMENT, 0, NULL, node, NULL, NULL);
        } else if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_DOUBLE_MINUS)) {
            advance_token(state);
            node = create_ast_node(AST_POSTFIX_DECREMENT, 0, NULL, node, NULL, NULL);
        } else if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_DOUBLE_COLON)) {
            advance_token(state);
            ASTNode *right = parse_primary_expression(state);
            if (!right) {
                free_ast_node(node);
                return NULL;
            }
            right = parse_postfix_expression(state, right);
            node = create_ast_node(AST_DOUBLE_COLON, 0, NULL, node, right, NULL);
        } else {
            break;
        }
    }
    return node;
}

/* Parse block statement (curly braces or single statement) */
static ASTNode *parse_block_statement(ParserState *state) {
    if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_LCURLY)) {
        CONSUME_TOKEN(state, TOKEN_LCURLY);
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
        
        while (get_current_token_type(state) != TOKEN_RCURLY && get_current_token_type(state) != TOKEN_EOF) {
            ASTNode *statement = parse_statement(state);
            if (statement && !add_ast_node_to_list(block_ast, statement)) {
                free_ast_node(statement);
                free(block_ast);
                free_ast_node(block_node);
                return NULL;
            }
        }
        CONSUME_TOKEN(state, TOKEN_RCURLY);
        
        block_node->extra = (ASTNode*)block_ast;
        return block_node;
    } else if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_THEN)) {
        advance_token(state);
        ASTNode *statement = parse_statement(state);
        return create_ast_node(AST_BLOCK, 0, NULL, statement, NULL, NULL);
    }
    
    ASTNode *single_statement = parse_statement(state);
    return create_ast_node(AST_BLOCK, 0, NULL, single_statement, NULL, NULL);
}

/* Parse array initializer { element1, element2, ... } */
static ASTNode *parse_array_initializer(ParserState *state) {
    if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_LCURLY)) {
        CONSUME_TOKEN(state, TOKEN_LCURLY);
        AST *list = malloc(sizeof(AST));
        if (!list) return NULL;
        list->nodes = NULL;
        list->count = 0;
        list->capacity = 0;

        while (!CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_RCURLY) && !CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_EOF)) {
            ASTNode *expression = parse_expression(state);
            if (!expression) {
                free_ast(list);
                return NULL;
            }
            if (!add_ast_node_to_list(list, expression)) {
                free_ast_node(expression);
                free_ast(list);
                return NULL;
            }
            if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_COMMA)) {
                advance_token(state);
            } else {
                break;
            }
        }
        CONSUME_TOKEN(state, TOKEN_RCURLY);
        return create_ast_node(AST_ARRAY_INITIALIZER, 0, NULL, NULL, NULL, (ASTNode*)list);
    } else {
        REPORT_PARSE_ERROR(state, "Expected '{' for array initializer");
        return NULL;
    }
}

/* Print AST node recursively with indentation */
static void print_ast_node(ASTNode *node, int indent) {
    if (!node) return;
    
    for (int i = 0; i < indent; i++) printf("  ");
    
    switch (node->type) {
        case AST_VARIABLE_DECLARATION:
            printf("VARIABLE_DECL:\n");
            for (int i = 0; i < indent+1; i++) printf("  ");
            printf("name: %s\n", node->value);
            if (node->variable_type) {
                for (int i = 0; i < indent+1; i++) printf("  ");
                printf("modifiers: ");
                for (uint8_t j = 0; j < node->variable_type->modifier_count; j++) printf("%s ", node->variable_type->modifiers[j]);
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
                if (node->variable_type->has_right_id) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("SIZE: %s\n", node->variable_type->right_id);
                } else if (node->variable_type->right_number != 0) {
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
                for (uint16_t i = 0; i < block_ast->count; i++) print_ast_node(block_ast->nodes[i], indent + 1);
            } else print_ast_node(node->left, indent + 1);
            break;
                        
        case AST_FUNCTION:
            printf("FUNCTION: %s", node->value);
            if (node->return_type) {
                printf(" -> ");
                for (uint8_t j = 0; j < node->return_type->modifier_count; j++) printf("%s ", node->return_type->modifiers[j]);
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
            printf("CPU");
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
            
        case AST_SYSCALL:
            printf("SYSCALL");
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
                if (node->variable_type->has_right_id) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("SIZE: %s\n", node->variable_type->right_id);
                } else if (node->variable_type->right_number != 0) {
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

        case AST_ARRAY_ACCESS:
            printf("ARRAY_ACCESS\n");
            print_ast_node(node->left, indent + 1);
            print_ast_node(node->right, indent + 1);
            break;

        case AST_LABEL_DECLARATION:
            printf("LABEL DECLARATION: %s\n", node->value);
            break;

        case AST_JUMP:
            printf("JUMP: %s\n", node->value);
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

        case AST_ORG:
            printf("ORG");
            if (node->left) {
                printf(" with expression:\n");
                print_ast_node(node->left, indent + 1);
            } else {
                printf("\n");
            }
            break;

        case AST_USE_OPTION:
            printf("USE_OPTION: %s\n", token_names[node->operation_type]);
            print_ast_node(node->left, indent + 1);
            break;

        case AST_USE_MULTI:
            printf("USE_MULTI\n");
            if (node->extra) {
                AST *list = (AST*)node->extra;
                for (int i = 0; i < list->count; i++) print_ast_node(list->nodes[i], indent + 1);
            }
            break;

        case AST_STRUCT_OBJECT_DECLARATION:
            printf("STRUCT_OBJECT_DECL: %s\n", node->value);
            print_ast_node(node->left, indent + 1);
            if (node->right) {
                for (int i = 0; i < indent; i++) printf("  ");
                printf("initializer:\n");
                print_ast_node(node->right, indent + 1);
            }
            break;

        case AST_STRUCT_OBJECT_CALL:
            printf("STRUCT_OBJECT_CALL: %s\n", node->value);
            break;

        case AST_FIELD_ACCESS:
            printf("FIELD_ACCESS: %s\n", node->value);
            print_ast_node(node->left, indent + 1);
            break;

        case AST_DOUBLE_COLON:
            printf("DOUBLE_COLON\n");
            print_ast_node(node->left, indent + 1);
            print_ast_node(node->right, indent + 1);
            break;
    }
}

static ASTNode *parse_if_statement(ParserState *state) {
    CONSUME_TOKEN(state, TOKEN_IF);
    ASTNode *condition = parse_expression(state);
    if (!condition) return NULL;
    
    ASTNode *if_block = parse_block_statement(state);
    if (!if_block) {
        free_ast_node(condition);
        return NULL;
    }
    
    ASTNode *else_block = NULL;
    if (ATTEMPT_CONSUME_TOKEN(state, TOKEN_ELSE)) else_block = parse_block_statement(state);
    
    return create_ast_node(AST_IF_STATEMENT, 0, NULL, condition, if_block, else_block);
}

static ASTNode *parse_cpu(ParserState *state) {
    CONSUME_TOKEN(state, TOKEN_CPU);
    expect_token(state, TOKEN_COLON);
    
    if (!CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_NUMBER) && !CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_ID)) {
        REPORT_PARSE_ERROR(state, "Expected cpu number or variable after ':'");
        return NULL;
    }
    
    Token *token = get_current_token(state);
    char *cpu_value = strdup(token->value);
    advance_token(state);

    AST *arguments = NULL;
    if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_LPAREN)) {
        advance_token(state);
        arguments = malloc(sizeof(AST));
        if (!arguments) {
            free(cpu_value);
            return NULL;
        }
        arguments->nodes = NULL;
        arguments->count = 0;
        arguments->capacity = 0;

        while (!CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_RPAREN) && !CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_EOF)) {
            if (arguments->count >= MAX_REGIS) {
                REPORT_PARSE_ERROR(state, "Too many arguments for cpu, maximum is %d", MAX_REGIS);
                free_ast(arguments);
                free(cpu_value);
                return NULL;
            }
            ASTNode *arg = parse_expression(state);
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
            if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_COMMA)) {
                advance_token(state);
            } else {
                break;
            }
        }
        CONSUME_TOKEN(state, TOKEN_RPAREN);
    }

    CONSUME_TOKEN(state, TOKEN_SEMICOLON);
    return create_ast_node(AST_CPU, 0, cpu_value, (ASTNode*)arguments, NULL, NULL);
}

static ASTNode *parse_syscall(ParserState *state) {
    CONSUME_TOKEN(state, TOKEN_SYSCALL);
    
    AST *arguments = NULL;
    if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_LPAREN)) {
        advance_token(state);
        arguments = malloc(sizeof(AST));
        if (!arguments) {
            return NULL;
        }
        arguments->nodes = NULL;
        arguments->count = 0;
        arguments->capacity = 0;

        while (!CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_RPAREN) && !CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_EOF)) {
            if (arguments->count >= MAX_REGIS) {
                REPORT_PARSE_ERROR(state, "Too many arguments for syscall, maximum is %d", MAX_REGIS);
                free_ast(arguments);
                return NULL;
            }
            ASTNode *arg = parse_expression(state);
            if (!arg) {
                free_ast(arguments);
                return NULL;
            }
            if (!add_ast_node_to_list(arguments, arg)) {
                free_ast_node(arg);
                free_ast(arguments);
                return NULL;
            }
            if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_COMMA)) {
                advance_token(state);
            } else {
                break;
            }
        }
        CONSUME_TOKEN(state, TOKEN_RPAREN);
    }

    CONSUME_TOKEN(state, TOKEN_SEMICOLON);
    return create_ast_node(AST_SYSCALL, 0, NULL, (ASTNode*)arguments, NULL, NULL);
}

static ASTNode *parse_function_parameter(ParserState *state) {
    CONSUME_TOKEN(state, TOKEN_FUNC);

    if (!CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_ID)) {
        REPORT_PARSE_ERROR(state, "Expected function name after '.' in function parameter");
        return NULL;
    }

    Token *name_token = get_current_token(state);
    char *func_name = strdup(name_token->value);
    advance_token(state);

    ASTNode *params = NULL;
    bool is_variadic = false;
    if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_LPAREN)) {
        params = parse_parameter_list(state, &is_variadic);
        if (!params) {
            free(func_name);
            return NULL;
        }
        if (is_variadic) {
            REPORT_PARSE_ERROR(state, "Variadic parameters are not allowed in function parameter");
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

    Type *return_type = try_parse_type_specifier(state, true);

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

static ASTNode *parse_parameter_list(ParserState *state, bool *is_variadic) {
    AST *parameter_ast = malloc(sizeof(AST));
    if (!parameter_ast) return NULL;
    parameter_ast->nodes = NULL;
    parameter_ast->count = 0;
    parameter_ast->capacity = 0;
    
    *is_variadic = false;
    
    CONSUME_TOKEN(state, TOKEN_LPAREN);
    
    if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_RPAREN)) {
        CONSUME_TOKEN(state, TOKEN_RPAREN);
        return create_ast_node(AST_BLOCK, 0, NULL, NULL, NULL, (ASTNode*)parameter_ast);
    }
    
    while (get_current_token_type(state) != TOKEN_RPAREN && get_current_token_type(state) != TOKEN_EOF) {
        if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_ELLIPSIS)) {
            advance_token(state);
            *is_variadic = true;
            if (!CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_RPAREN)) {
                REPORT_PARSE_ERROR(state, "Expected ')' after ellipsis");
                free_ast(parameter_ast);
                return NULL;
            }
            break;
        }
        
        if (!CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_VAR) && !CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_FUNC) && !CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_OBJ)) {
            REPORT_PARSE_ERROR(state, "Expected 'var', 'func' or 'obj' for parameter declaration");
            free_ast(parameter_ast);
            return NULL;
        }

        TokenType param_type = get_current_token_type(state);
        advance_token(state);
        
        uint8_t local_pointer_level = 0;
        uint8_t local_is_reference = 0;
        while (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_AT) || 
               CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_DOUBLE_AT) ||
               CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_AMPERSAND)) {
            if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_AT)) {
                local_pointer_level++;
                advance_token(state);
            } else if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_DOUBLE_AT)) {
                local_pointer_level += 2;
                advance_token(state);
            } else if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_AMPERSAND)) {
                local_is_reference = 1;
                advance_token(state);
            }
        }

        Token *identifier_token = get_current_token(state);
        if (identifier_token->type != TOKEN_ID) {
            REPORT_PARSE_ERROR(state, "Expected identifier for parameter name");
            free_ast(parameter_ast);
            return NULL;
        }
        char *parameter_name = strdup(identifier_token->value);
        advance_token(state);

        int array_size = -1;
        if (param_type == TOKEN_VAR && CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_LBRACKET)) {
            advance_token(state);
            if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_NUMBER)) {
                Token *size_token = get_current_token(state);
                array_size = atoi(size_token->value);
                advance_token(state);
            } else if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_RBRACKET)) {
                array_size = 0;
            } else {
                REPORT_PARSE_ERROR(state, "Expected array size");
                free(parameter_name);
                free_ast(parameter_ast);
                return NULL;
            }
            CONSUME_TOKEN(state, TOKEN_RBRACKET);
        }
        
        Type *parameter_type = NULL;
        if (param_type == TOKEN_FUNC) {
            if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_LPAREN)) {
                advance_token(state);
                while (!CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_RPAREN) && !CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_EOF)) {
                    advance_token(state);
                }
                if (!expect_token(state, TOKEN_RPAREN)) {
                    free(parameter_name);
                    free_ast(parameter_ast);
                    return NULL;
                }
            }

            if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_COLON)) {
                advance_token(state);
                Type *return_type = parse_type_specifier(state, true);
                free_type(return_type);
            }

            parameter_type = malloc(sizeof(Type));
            parameter_type->name = strdup("func");
            parameter_type->pointer_level = 0;
            parameter_type->is_reference = 0;
            parameter_type->left_number = 0;
            parameter_type->right_number = 0;
            parameter_type->right_id = NULL;
            parameter_type->has_right_id = 0;
            parameter_type->modifiers = NULL;
            parameter_type->modifier_count = 0;
            parameter_type->is_array = 0;
            parameter_type->array_size = 0;
        } else if (param_type == TOKEN_OBJ) {
            CONSUME_TOKEN(state, TOKEN_COLON);
            char *qualified_name = parse_qualified_name(state);
            if (!qualified_name) {
                free(parameter_name);
                free_ast(parameter_ast);
                return NULL;
            }

            parameter_type = malloc(sizeof(Type));
            parameter_type->name = qualified_name;
            parameter_type->pointer_level = 0;
            parameter_type->is_reference = 0;
            parameter_type->left_number = 0;
            parameter_type->right_number = 0;
            parameter_type->right_id = NULL;
            parameter_type->has_right_id = 0;
            parameter_type->modifiers = NULL;
            parameter_type->modifier_count = 0;
            parameter_type->is_array = 0;
            parameter_type->array_size = 0;
        } else {
            if (ATTEMPT_CONSUME_TOKEN(state, TOKEN_COLON)) {
                parameter_type = parse_type_specifier(state, false);
            } else {
                parameter_type = malloc(sizeof(Type));
                parameter_type->name = strdup("int");
                parameter_type->pointer_level = 0;
                parameter_type->is_reference = 0;
                parameter_type->left_number = 0;
                parameter_type->right_number = 0;
                parameter_type->right_id = NULL;
                parameter_type->has_right_id = 0;
                parameter_type->modifiers = NULL;
                parameter_type->modifier_count = 0;
                parameter_type->is_array = 0;
                parameter_type->array_size = 0;
            }
        }

        if (!parameter_type) {
            free(parameter_name);
            free_ast((AST*)parameter_ast);
            return NULL;
        }

        apply_prefixes_to_type(parameter_type, local_pointer_level, local_is_reference);

        if (array_size != -1) {
            parameter_type->is_array = 1;
            parameter_type->array_size = array_size;
        }
        
        ASTNode *parameter_node = create_ast_node(AST_VARIABLE_DECLARATION, 0, parameter_name, NULL, NULL, NULL);
        if (!parameter_node) {
            free(parameter_name);
            free_type(parameter_type);
            free_ast((AST*)parameter_ast);
            return NULL;
        }
        parameter_node->variable_type = parameter_type;
        
        if (!add_ast_node_to_list(parameter_ast, parameter_node)) {
            free_ast_node(parameter_node);
            free_ast((AST*)parameter_ast);
            return NULL;
        }
        
        if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_COMMA)) advance_token(state);
        else if (!CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_RPAREN) && !CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_ELLIPSIS)) {
            REPORT_PARSE_ERROR(state, "Expected ',' or ')' after parameter");
            free_ast((AST*)parameter_ast);
            return NULL;
        }
    }
    
    CONSUME_TOKEN(state, TOKEN_RPAREN);
    return create_ast_node(AST_BLOCK, 0, NULL, NULL, NULL, (ASTNode*)parameter_ast);
}

static ASTNode *parse_function_declaration(ParserState *state) {
    CONSUME_TOKEN(state, TOKEN_FUNC);
    
    if (!CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_ID)) {
        REPORT_PARSE_ERROR(state, "Expected function name");
        return NULL;
    }
    Token *token = get_current_token(state);
    char *function_name = strdup(token->value);
    advance_token(state);
    
    bool is_variadic = false;
    ASTNode *arguments = NULL;
    
    if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_LPAREN)) arguments = parse_parameter_list(state, &is_variadic);
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
    if (ATTEMPT_CONSUME_TOKEN(state, TOKEN_COLON)) {
        return_type = parse_type_specifier(state, true);
        if (!return_type) {
            free(function_name);
            free_ast_node(arguments);
            return NULL;
        }
    } else {
        return_type = malloc(sizeof(Type));
        return_type->name = strdup("void");
        return_type->pointer_level = 0;
        return_type->is_reference = 0;
        return_type->left_number = 0;
        return_type->right_number = 0;
        return_type->right_id = NULL;
        return_type->has_right_id = 0;
        return_type->modifiers = NULL;
        return_type->modifier_count = 0;
        return_type->is_array = 0;
        return_type->array_size = 0;
    }
    
    ASTNode *body = NULL;
    if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_LCURLY)) body = parse_block_statement(state);
    else if (ATTEMPT_CONSUME_TOKEN(state, TOKEN_THEN)) {
        ASTNode *statement = parse_statement(state);
        body = create_ast_node(AST_BLOCK, 0, NULL, statement, NULL, NULL);
    } else if (ATTEMPT_CONSUME_TOKEN(state, TOKEN_SEMICOLON)) {
    } else {
        REPORT_PARSE_ERROR(state, "Expected '{', ':', '=>' or ';' for function body");
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

static ASTNode *parse_label_declaration(ParserState *state) {
    Token *token = get_current_token(state);
    char *label_name = strdup(token->value);
    advance_token(state);
    
    CONSUME_TOKEN(state, TOKEN_COLON);
    return create_ast_node(AST_LABEL_DECLARATION, 0, label_name, NULL, NULL, NULL);
}

static ASTNode *parse_jump_statement(ParserState *state) {
    CONSUME_TOKEN(state, TOKEN_JUMP);
    
   if (!CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_ID)) {
        REPORT_PARSE_ERROR(state, "Expected label name after 'jump'");
        return NULL;
    }
    Token *token = get_current_token(state);
    char *label_name = strdup(token->value);
    advance_token(state);
    
    CONSUME_TOKEN(state, TOKEN_SEMICOLON);
    return create_ast_node(AST_JUMP, 0, label_name, NULL, NULL, NULL);
}

static ASTNode *parse_va_start(ParserState *state) {
    CONSUME_TOKEN(state, TOKEN_VA_START);
    CONSUME_TOKEN(state, TOKEN_LPAREN);
    ASTNode *va_list = parse_expression(state);
    if (!va_list) return NULL;
    CONSUME_TOKEN(state, TOKEN_COMMA);
    ASTNode *last_arg = parse_expression(state);
    if (!last_arg) {
        free_ast_node(va_list);
        return NULL;
    }
    CONSUME_TOKEN(state, TOKEN_RPAREN);
    CONSUME_TOKEN(state, TOKEN_SEMICOLON);
    return create_ast_node(AST_VA_START, 0, NULL, va_list, last_arg, NULL);
}

static ASTNode *parse_va_arg_expression(ParserState *state) {
    CONSUME_TOKEN(state, TOKEN_VA_ARG);
    CONSUME_TOKEN(state, TOKEN_LPAREN);
    ASTNode *va_list = parse_expression(state);
    if (!va_list) return NULL;
    CONSUME_TOKEN(state, TOKEN_COMMA);
    Type *arg_type = parse_type_specifier(state, true);
    if (!arg_type) {
        free_ast_node(va_list);
        return NULL;
    }
    CONSUME_TOKEN(state, TOKEN_RPAREN);
    ASTNode *node = create_ast_node(AST_VA_ARG, 0, NULL, va_list, NULL, NULL);
    if (node) node->variable_type = arg_type;
    return node;
}

static ASTNode *parse_va_end(ParserState *state) {
    CONSUME_TOKEN(state, TOKEN_VA_END);
    CONSUME_TOKEN(state, TOKEN_LPAREN);
    ASTNode *va_list = parse_expression(state);
    if (!va_list) return NULL;
    CONSUME_TOKEN(state, TOKEN_RPAREN);
    CONSUME_TOKEN(state, TOKEN_SEMICOLON);
    return create_ast_node(AST_VA_END, 0, NULL, va_list, NULL, NULL);
}

static ASTNode *parse_return_statement(ParserState *state) {
    CONSUME_TOKEN(state, TOKEN_RETURN);
    ASTNode *expression = NULL;
    if (!CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_SEMICOLON)) expression = parse_expression(state);
    CONSUME_TOKEN(state, TOKEN_SEMICOLON);
    return create_ast_node(AST_RETURN, 0, NULL, expression, NULL, NULL);
}

static ASTNode *parse_break_statement(ParserState *state) {
    CONSUME_TOKEN(state, TOKEN_BREAK);
    CONSUME_TOKEN(state, TOKEN_SEMICOLON);
    return create_ast_node(AST_BREAK, 0, NULL, NULL, NULL, NULL);
}

static ASTNode *parse_continue_statement(ParserState *state) {
    CONSUME_TOKEN(state, TOKEN_CONTINUE);
    CONSUME_TOKEN(state, TOKEN_SEMICOLON);
    return create_ast_node(AST_CONTINUE, 0, NULL, NULL,  NULL, NULL);
}

static ASTNode *parse_free_statement(ParserState *state) {
    CONSUME_TOKEN(state, TOKEN_FREE);
    CONSUME_TOKEN(state, TOKEN_LPAREN);
    ASTNode *expression = parse_expression(state);
    if (!expression) return NULL;
    CONSUME_TOKEN(state, TOKEN_RPAREN);
    CONSUME_TOKEN(state, TOKEN_SEMICOLON);
    return create_ast_node(AST_FREE, 0, NULL, expression, NULL, NULL);
}

static ASTNode *parse_function_call(ParserState *state) {
    if (!CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_ID)) {
        return NULL;
    }
    
    Token *func_name_token = get_current_token(state);
    char *func_name = strdup(func_name_token->value);
    advance_token(state);
    
    ASTNode *args = NULL;
    if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_LPAREN)) {
        advance_token(state);
        AST *arg_list = malloc(sizeof(AST));
        if (!arg_list) {
            free(func_name);
            return NULL;
        }
        arg_list->nodes = NULL;
        arg_list->count = 0;
        arg_list->capacity = 0;
        
        while (!CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_RPAREN) && !CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_EOF)) {
            ASTNode *arg = parse_expression(state);
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
            if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_COMMA)) {
                advance_token(state);
            } else {
                break;
            }
        }
        CONSUME_TOKEN(state, TOKEN_RPAREN);
        args = create_ast_node(AST_BLOCK, 0, NULL, NULL, NULL, (ASTNode*)arg_list);
    } else {
        free(func_name);
        return NULL;
    }
    
    return create_ast_node(AST_FUNCTION_CALL, 0, func_name, args, NULL, NULL);
}

static ASTNode *parse_org_statement(ParserState *state) {
    advance_token(state);
    ASTNode *expr = NULL;
    if (get_current_token_type(state) != TOKEN_SEMICOLON) {
        expr = parse_expression(state);
        if (!expr) return NULL;
    }
    CONSUME_TOKEN(state, TOKEN_SEMICOLON);
    return create_ast_node(AST_ORG, 0, NULL, expr, NULL, NULL);
}

static ASTNode *parse_use_expression(ParserState *state) {
    advance_token(state); 

    if (ATTEMPT_CONSUME_TOKEN(state, TOKEN_COLON)) {
        TokenType option = get_current_token_type(state);
        if (option != TOKEN_FAM && option != TOKEN_SER && option != TOKEN_BIT) {
            REPORT_PARSE_ERROR(state, "Expected family, series or bits after 'use:'");
            return NULL;
        }
        advance_token(state);

        CONSUME_TOKEN(state, TOKEN_EQUAL);

        ASTNode *value = parse_expression(state);
        if (!value) return NULL;

        return create_ast_node(AST_USE_OPTION, option, NULL, value, NULL, NULL);
    } else if (ATTEMPT_CONSUME_TOKEN(state, TOKEN_EQUAL)) {
        CONSUME_TOKEN(state, TOKEN_LCURLY);

        AST *values = malloc(sizeof(AST));
        if (!values) return NULL;
        values->nodes = NULL;
        values->count = 0;
        values->capacity = 0;

        while (!CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_RCURLY) && !CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_EOF)) {
            ASTNode *expr = parse_expression(state);
            if (!expr) {
                free_ast(values);
                return NULL;
            }
            if (!add_ast_node_to_list(values, expr)) {
                free_ast_node(expr);
                free_ast(values);
                return NULL;
            }
            if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_COMMA)) {
                advance_token(state);
            } else {
                break;
            }
        }

        CONSUME_TOKEN(state, TOKEN_RCURLY);
        return create_ast_node(AST_USE_MULTI, 0, NULL, NULL, NULL, (ASTNode*)values);
    } else {
        REPORT_PARSE_ERROR(state, "Expected ':' or '=' after 'use'");
        return NULL;
    }
}

static ASTNode *parse_parseof_statement(ParserState *state) {
    advance_token(state);
    ASTNode *expr = parse_expression(state);
    if (!expr) return NULL;
    CONSUME_TOKEN(state, TOKEN_SEMICOLON);
    return create_ast_node(AST_PARSEOF, 0, NULL, expr, NULL, NULL);
}

static ASTNode *parse_push_statement(ParserState *state) {
    advance_token(state);
    ASTNode *expr = parse_expression(state);
    if (!expr) return NULL;
    CONSUME_TOKEN(state, TOKEN_SEMICOLON);
    return create_ast_node(AST_PUSH, 0, NULL, expr, NULL, NULL);
}

static ASTNode *parse_struct_object_declaration(ParserState *state) {
    CONSUME_TOKEN(state, TOKEN_OBJ);
    
    if (!CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_ID)) {
        REPORT_PARSE_ERROR(state, "Expected object name");
        return NULL;
    }
    Token *obj_name_token = get_current_token(state);
    char *obj_name = strdup(obj_name_token->value);
    advance_token(state);
    
    CONSUME_TOKEN(state, TOKEN_COLON);
    
    char *qualified_name = parse_qualified_name(state);
    if (!qualified_name) {
        free(obj_name);
        REPORT_PARSE_ERROR(state, "Expected struct name");
        return NULL;
    }
    
    ASTNode *initializer = NULL;
    if (ATTEMPT_CONSUME_TOKEN(state, TOKEN_EQUAL)) {
        initializer = parse_array_initializer(state);
        if (!initializer) {
            free(obj_name);
            free(qualified_name);
            return NULL;
        }
    }
    
    CONSUME_TOKEN(state, TOKEN_SEMICOLON);
    
    Type *struct_type = malloc(sizeof(Type));
    struct_type->name = qualified_name;
    struct_type->pointer_level = 0;
    struct_type->is_reference = 0;
    struct_type->left_number = 0;
    struct_type->right_number = 0;
    struct_type->right_id = NULL;
    struct_type->has_right_id = 0;
    struct_type->modifiers = NULL;
    struct_type->modifier_count = 0;
    struct_type->is_array = 0;
    struct_type->array_size = 0;
    
    ASTNode *node = create_ast_node(AST_STRUCT_OBJECT_DECLARATION, 0, obj_name, NULL, initializer, NULL);
    node->variable_type = struct_type;
    
    return node;
}

static ASTNode *parse_struct_object_access(ParserState *state) {
    if (!CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_ID)) {
        return NULL;
    }
    
    Token *id_token = get_current_token(state);
    char *obj_name = strdup(id_token->value);
    advance_token(state);
    
    return create_ast_node(AST_STRUCT_OBJECT_CALL, 0, obj_name, NULL, NULL, NULL);
}

static ASTNode *parse_expression(ParserState *state) {
    return parse_assignment_expression(state);
}

static ASTNode *parse_assignment_expression(ParserState *state) {
    ASTNode *left = parse_logical_or_expression(state);
    if (!left) return NULL;
    
    if (
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_DOUBLE_PLUS) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_DOUBLE_MINUS) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_EQUAL) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_PLUS_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_MINUS_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_STAR_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_SLASH_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_PERCENT_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_PIPE_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_AMPERSAND_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_CARET_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_TILDE_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_SHL_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_SHR_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_SAL_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_SAR_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_ROL_EQ) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_ROR_EQ)
    ) {
        
        TokenType operation = get_current_token_type(state);
        advance_token(state);
        ASTNode *right = parse_assignment_expression(state);
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

static ASTNode *parse_binary_operation(ParserState *state, ASTNode *(*next_parser)(ParserState *), TokenType operation1, TokenType operation2) {
    ASTNode *node = next_parser(state);
    if (!node) return NULL;
    
    while (CURRENT_TOKEN_TYPE_MATCHES(state, operation1) || CURRENT_TOKEN_TYPE_MATCHES(state, operation2)) {
        TokenType operation = get_current_token_type(state);
        advance_token(state);
        ASTNode *right = next_parser(state);
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

static ASTNode *parse_logical_or_expression(ParserState *state) {
    return parse_binary_operation(state, parse_logical_xor_expression, TOKEN_DOUBLE_PIPE, TOKEN_ERROR);
}

static ASTNode *parse_logical_xor_expression(ParserState *state) {
    return parse_binary_operation(state, parse_logical_and_expression, TOKEN_DOUBLE_CARET, TOKEN_ERROR);
}

static ASTNode *parse_logical_and_expression(ParserState *state) {
    return parse_binary_operation(state, parse_bitwise_or_expression, TOKEN_DOUBLE_AMPERSAND, TOKEN_ERROR);
}

static ASTNode *parse_bitwise_or_expression(ParserState *state) {
    return parse_binary_operation(state, parse_bitwise_xor_expression, TOKEN_PIPE, TOKEN_ERROR);
}

static ASTNode *parse_bitwise_xor_expression(ParserState *state) {
    return parse_binary_operation(state, parse_bitwise_and_expression, TOKEN_CARET, TOKEN_ERROR);
}

static ASTNode *parse_bitwise_and_expression(ParserState *state) {
    return parse_binary_operation(state, parse_equality_expression, TOKEN_AMPERSAND, TOKEN_ERROR);
}

static ASTNode *parse_equality_expression(ParserState *state) {
    return parse_binary_operation(state, parse_relational_expression, TOKEN_DOUBLE_EQ, TOKEN_NE);
}

static ASTNode *parse_relational_expression(ParserState *state) {
    ASTNode *node = parse_shift_expression(state);
    if (!node)
        return NULL;
    
    while (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_LT) || 
           CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_GT) ||
           CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_LE) ||
           CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_GE)) {
        TokenType operation = get_current_token_type(state);
        advance_token(state);
        ASTNode *right = parse_shift_expression(state);
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

static ASTNode *parse_shift_expression(ParserState *state) {
    return parse_binary_operation(state, parse_additive_expression, TOKEN_SHL, TOKEN_SHR);
}

static ASTNode *parse_additive_expression(ParserState *state) {
    return parse_binary_operation(state, parse_multiplicative_expression, TOKEN_PLUS, TOKEN_MINUS);
}

static ASTNode *parse_multiplicative_expression(ParserState *state) {
    return parse_binary_operation(state, parse_unary_expression, TOKEN_STAR, TOKEN_SLASH);
}

static ASTNode *parse_unary_expression(ParserState *state) {
    if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_PLUS) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_MINUS) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_BANG) || 
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_TILDE) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_AMPERSAND) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_DOUBLE_PLUS) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_DOUBLE_MINUS) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_STAR) ||
        CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_SLASH)) {
        TokenType operation = get_current_token_type(state);
        advance_token(state);
        ASTNode *operand = parse_unary_expression(state);
        if (!operand) return NULL;
        return create_ast_node(AST_UNARY_OPERATION, operation, NULL, NULL, operand, NULL);
    }
    ASTNode *primary = parse_primary_expression(state);
    if (!primary) return NULL;
    return parse_postfix_expression(state, primary);
}

static ASTNode *parse_primary_expression(ParserState *state) {
    Token *token = get_current_token(state);
    if (!token) {
        REPORT_PARSE_ERROR(state, "Unexpected end of input");
        return NULL;
    }
    
    switch (token->type) {
        case TOKEN_ID: {
            char *value = strdup(token->value);
            advance_token(state);
            
            ASTNode *identifier_node = create_ast_node(AST_IDENTIFIER, 0, value, NULL, NULL, NULL);
            
            if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_LPAREN)) {
                ASTNode *func_call = parse_function_call(state);
                if (func_call) {
                    free_ast_node(identifier_node);
                    return func_call;
                }
            }
            
            ASTNode *struct_access = parse_struct_object_access(state);
            if (struct_access) {
                free_ast_node(identifier_node);
                return struct_access;
            }
            
            return parse_postfix_expression(state, identifier_node);
        }
        case TOKEN_LPAREN: {
            advance_token(state);
            int saved_pos = state->current_token_position;
            Type *cast_type = parse_type_specifier_silent(state, true, true);
            if (cast_type != NULL && CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_RPAREN)) {
                advance_token(state);
                ASTNode *expr = parse_unary_expression(state);
                if (!expr) {
                    free_type(cast_type);
                    return NULL;
                }
                ASTNode *node = create_ast_node(AST_CAST, 0, NULL, expr, NULL, NULL);
                node->variable_type = cast_type;
                return node;
            } else {
                state->current_token_position = saved_pos;
                ASTNode *expression = parse_expression(state);
                if (!expression) return NULL;
                CONSUME_TOKEN(state, TOKEN_RPAREN);
                return expression;
            }
        }
        case TOKEN_LCURLY:
            return parse_array_initializer(state);
        case TOKEN_ALLOC:
        case TOKEN_REALLOC:
        case TOKEN_SIZEOF: {
            ASTNodeType node_type;
            switch (token->type) {
                case TOKEN_ALLOC: node_type = AST_ALLOC; break;
                case TOKEN_REALLOC: node_type = AST_REALLOC; break;
                case TOKEN_SIZEOF: node_type = AST_SIZEOF; break;
                default: return NULL;
            }
            advance_token(state);
            CONSUME_TOKEN(state, TOKEN_LPAREN);
            ASTNode *arguments = parse_expression(state);
            if (!arguments) return NULL;
            CONSUME_TOKEN(state, TOKEN_RPAREN);
            return create_ast_node(node_type, 0, NULL, arguments, NULL, NULL);
        }
        case TOKEN_POP: {
            advance_token(state);
            return create_ast_node(AST_POP, 0, NULL, NULL, NULL, NULL);
        }
        case TOKEN_STACK: {
            advance_token(state);
            return create_ast_node(AST_STACK, 0, NULL, NULL, NULL, NULL);
            break;
        }
        case TOKEN_NUMBER:
        case TOKEN_STRING:
        case TOKEN_CHAR: {
            char *value = strdup(token->value);
            TokenType type = token->type;
            advance_token(state);
            return create_ast_node(AST_LITERAL_VALUE, type, value, NULL, NULL, NULL);
        }
        case TOKEN_STRUCT: {
            advance_token(state);
            Token *identifier_token = get_current_token(state);
            if (identifier_token->type != TOKEN_ID) {
                REPORT_PARSE_ERROR(state, "Expected structure name after 'struct'");
                return NULL;
            }
            char *structure_name = strdup(identifier_token->value);
            advance_token(state);
            return create_ast_node(AST_STRUCT_INSTANCE, 0, structure_name, NULL, NULL, NULL);
        }
        case TOKEN_VA_ARG:
            return parse_va_arg_expression(state);
        case TOKEN_ORG:
        case TOKEN_USE: {
            ASTNodeType node_type;
            switch (token->type) {
                case TOKEN_ORG: node_type = AST_ORG; break;
                case TOKEN_USE: return parse_use_expression(state); break;
                default: return NULL; break;
            }
            advance_token(state);
            return create_ast_node(node_type, 0, NULL, NULL, NULL, NULL);
        }
        default: 
            REPORT_PARSE_ERROR(state, "Unexpected token in expression: %s", token_names[token->type]);
            return NULL;
    }
}

static ASTNode *parse_variable_declaration(ParserState *state) {
    CONSUME_TOKEN(state, TOKEN_VAR);
    
    AST *variable_list = malloc(sizeof(AST));
    if (!variable_list) return NULL;
    variable_list->nodes = NULL;
    variable_list->count = 0;
    variable_list->capacity = 0;
    
    int *array_sizes = NULL;
    int array_count = 0;
    
    while (1) {
        uint8_t local_pointer_level = 0;
        uint8_t local_is_reference = 0;
        while (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_AT) || 
               CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_DOUBLE_AT) ||
               CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_AMPERSAND)) {
            if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_AT)) {
                local_pointer_level++;
                advance_token(state);
            } else if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_DOUBLE_AT)) {
                local_pointer_level += 2;
                advance_token(state);
            } else if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_AMPERSAND)) {
                local_is_reference = 1;
                advance_token(state);
            }
        }

        if (!CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_ID)) {
            REPORT_PARSE_ERROR(state, "Expected identifier");
            free_ast(variable_list);
            free(array_sizes);
            return NULL;
        }
        
        Token *identifier_token = get_current_token(state);
        char *variable_name = strdup(identifier_token->value);
        advance_token(state);
        
        int array_size = -1;
        if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_LBRACKET)) {
            advance_token(state);
            if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_NUMBER)) {
                Token *size_token = get_current_token(state);
                array_size = atoi(size_token->value);
                advance_token(state);
            } else if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_RBRACKET)) {
                array_size = 0;
            } else {
                REPORT_PARSE_ERROR(state, "Expected number or ']' after '['");
                free(variable_name);
                free_ast(variable_list);
                free(array_sizes);
                return NULL;
            }
            CONSUME_TOKEN(state, TOKEN_RBRACKET);
        }
        
        ASTNode *variable_node = create_ast_node(AST_IDENTIFIER, 0, variable_name, NULL, NULL, NULL);
        if (!add_ast_node_to_list(variable_list, variable_node)) {
            free(variable_name);
            free_ast(variable_list);
            free(array_sizes);
            return NULL;
        }
        
        int *new_array_sizes = realloc(array_sizes, (array_count + 1) * sizeof(int));
        if (!new_array_sizes) {
            free(variable_name);
            free_ast(variable_list);
            free(array_sizes);
            return NULL;
        }
        array_sizes = new_array_sizes;
        array_sizes[array_count++] = array_size;
        
        if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_COMMA)) advance_token(state);
        else break;
    }
    
    Type *variable_type = NULL;
    if (ATTEMPT_CONSUME_TOKEN(state, TOKEN_COLON)) {
        variable_type = parse_type_specifier(state, false);
        if (!variable_type) {
            free_ast(variable_list);
            free(array_sizes);
            return NULL;
        }
    } else {
        variable_type = malloc(sizeof(Type));
        variable_type->name = strdup("int");
        variable_type->pointer_level = 0;
        variable_type->is_reference = 0;
        variable_type->left_number = 0;
        variable_type->right_number = 0;
        variable_type->right_id = NULL;
        variable_type->has_right_id = 0;
        variable_type->modifiers = NULL;
        variable_type->modifier_count = 0;
        variable_type->is_array = 0;
        variable_type->array_size = 0;
    }

    for (int i = 0; i < array_count; i++) {
        if (array_sizes[i] != -1) {
            variable_type->is_array = 1;
            variable_type->array_size = array_sizes[i];
        }
    }
    
    free(array_sizes);
    
    AST *initializer_list = NULL;
    if (ATTEMPT_CONSUME_TOKEN(state, TOKEN_EQUAL)) {
        initializer_list = malloc(sizeof(AST));
        if (!initializer_list) {
            if (variable_type) free_type(variable_type);
            free_ast(variable_list);
            return NULL;
        }
        initializer_list->nodes = NULL;
        initializer_list->count = 0;
        initializer_list->capacity = 0;
        
        if (variable_type && variable_type->is_array) {
            ASTNode *initializer = parse_array_initializer(state);
            if (!initializer) {
                free_ast(initializer_list);
                if (variable_type) free_type(variable_type);
                free_ast(variable_list);
                return NULL;
            }
            if (!add_ast_node_to_list(initializer_list, initializer)) {
                free_ast(initializer_list);
                if (variable_type) free_type(variable_type);
                free_ast(variable_list);
                return NULL;
            }
        } else {
            while (1) {
                ASTNode *expression = parse_expression(state);
                if (!expression) {
                    free_ast(initializer_list);
                    if (variable_type) free_type(variable_type);
                    free_ast(variable_list);
                    return NULL;
                }
                
                if (!add_ast_node_to_list(initializer_list, expression)) {
                    free_ast_node(expression);
                    free_ast(initializer_list);
                    if (variable_type) free_type(variable_type);
                    free_ast(variable_list);
                    return NULL;
                }
                
                if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_COMMA)) advance_token(state);
                else break;
            }
        }
    }
    
    CONSUME_TOKEN(state, TOKEN_SEMICOLON);
    
    ASTNode *multi_declaration = create_ast_node(AST_MULTI_DECLARATION, 0, NULL, (ASTNode*)variable_list, (ASTNode*)initializer_list, NULL);
    if (!multi_declaration) {
        free_ast(variable_list);
        if (initializer_list) free_ast(initializer_list);
        if (variable_type) free_type(variable_type);
        return NULL;
    }
    multi_declaration->variable_type = variable_type;
    return multi_declaration;
}

static ASTNode *parse_struct_declaration(ParserState *state) {
    CONSUME_TOKEN(state, TOKEN_STRUCT);
    Token *identifier_token = get_current_token(state);
    if (identifier_token->type != TOKEN_ID) {
        REPORT_PARSE_ERROR(state, "Expected structure name after 'struct'");
        return NULL;
    }
    char *structure_name = strdup(identifier_token->value);
    advance_token(state);
    
    ASTNode *body = NULL;
    if (CURRENT_TOKEN_TYPE_MATCHES(state, TOKEN_LCURLY)) {
        body = parse_block_statement(state);
    } else if (ATTEMPT_CONSUME_TOKEN(state, TOKEN_SEMICOLON)) {
    } else {
        REPORT_PARSE_ERROR(state, "Expected '{' or ';' for struct body");
        free(structure_name);
        return NULL;
    }
    
    if (!body) {
        return create_ast_node(AST_STRUCT_DECLARATION, 0, structure_name, NULL, NULL, NULL);
    }
    return create_ast_node(AST_STRUCT_DECLARATION, 0, structure_name, NULL, body, NULL);
}

static ASTNode *parse_statement(ParserState *state) {
    /* Parse different statement types */
    Token *token = get_current_token(state);
    if (!token) {
        REPORT_PARSE_ERROR(state, "Unexpected end of input");
        return NULL;
    }
    
    switch (token->type) {
        case TOKEN_VAR:
            return parse_variable_declaration(state);
        case TOKEN_OBJ:
            return parse_struct_object_declaration(state);
        case TOKEN_IF:
            return parse_if_statement(state);
        case TOKEN_CPU:
            return parse_cpu(state);
        case TOKEN_SYSCALL:
            return parse_syscall(state);
        case TOKEN_FUNC: {
            int saved_pos = state->current_token_position;
            ASTNode *func_decl = parse_function_declaration(state);
            if (func_decl)
                return func_decl;
            state->current_token_position = saved_pos;
            ASTNode *func_call = parse_function_call(state);
            if (func_call) {
                CONSUME_TOKEN(state, TOKEN_SEMICOLON);
                return create_ast_node(AST_FUNCTION_CALL_STATEMENT, 0, func_call->value, func_call->left, NULL, NULL);
            }
            return NULL;
        }
        case TOKEN_STRUCT:
            return parse_struct_declaration(state);
        case TOKEN_ID: {
            if (state->current_token_position + 1 < state->total_tokens && 
                state->token_stream[state->current_token_position + 1].type == TOKEN_COLON) {
                return parse_label_declaration(state);
            }
            ASTNode *expression = parse_expression(state);
            if (expression) {
                if (!expect_token(state, TOKEN_SEMICOLON)) {
                    free_ast_node(expression);
                    return NULL;
                }
            }
            return expression;
        }
        case TOKEN_RETURN:
            return parse_return_statement(state);
        case TOKEN_BREAK:
            return parse_break_statement(state);
        case TOKEN_CONTINUE:
            return parse_continue_statement(state);
        case TOKEN_FREE:
            return parse_free_statement(state);
        case TOKEN_JUMP:
            return parse_jump_statement(state);
        case TOKEN_VA_START:
            return parse_va_start(state);
        case TOKEN_VA_ARG: {
            ASTNode *node = parse_va_arg_expression(state);
            if (!node) return NULL;
            CONSUME_TOKEN(state, TOKEN_SEMICOLON);
            return node;
        }
        case TOKEN_VA_END:
            return parse_va_end(state);
        case TOKEN_LCURLY:
            return parse_block_statement(state);
        case TOKEN_ORG:
            return parse_org_statement(state);
        case TOKEN_USE: {
            ASTNode *use_expr = parse_use_expression(state);
            if (!use_expr) return NULL;
            CONSUME_TOKEN(state, TOKEN_SEMICOLON);
            return use_expr;
        }
        case TOKEN_PARSEOF:
            return parse_parseof_statement(state);
        case TOKEN_PUSH:
            return parse_push_statement(state);
        default: {
            ASTNode *expression = parse_expression(state);
            if (expression) {
                if (!expect_token(state, TOKEN_SEMICOLON)) {
                    free_ast_node(expression);
                    return NULL;
                }
            }
            return expression;
        }
    }
}

void free_type(Type *type) {
    /* Free type structure and all its members */
    if (!type) return;
    free(type->name);
    if (type->has_right_id) free(type->right_id);
    for (int i = 0; i < type->modifier_count; i++) free(type->modifiers[i]);
    free(type->modifiers);
    free(type);
}


void free_ast_node(ASTNode *node) {
    /* Recursively free AST node and all its children */
    if (!node) return;
    
    switch (node->type) {
        case AST_BLOCK:
            if (node->extra) {
                AST *block_ast = (AST*)node->extra;
                for (int i = 0; i < block_ast->count; i++)
                    free_ast_node(block_ast->nodes[i]);
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
            if (node->variable_type)
                free_type(node->variable_type);
            break;
        case AST_CPU:
            if (node->left) {
                AST *args = (AST*)node->left;
                for (int i = 0; i < args->count; i++)
                    free_ast_node(args->nodes[i]);
                free(args->nodes);
                free(args);
            }
            free(node->value);
            break;
        case AST_SYSCALL:
            if (node->left) {
                AST *args = (AST*)node->left;
                for (int i = 0; i < args->count; i++)
                    free_ast_node(args->nodes[i]);
                free(args->nodes);
                free(args);
            }
            free(node->value);
            break; 
        case AST_MULTI_DECLARATION:
            if (node->left) {
                AST *variable_list = (AST*)node->left;
                for (int i = 0; i < variable_list->count; i++)
                    free_ast_node(variable_list->nodes[i]);
                free(variable_list->nodes);
                free(variable_list);
            }
            if (node->right) {
                AST *initializer_list = (AST*)node->right;
                for (int i = 0; i < initializer_list->count; i++)
                    free_ast_node(initializer_list->nodes[i]);
                free(initializer_list->nodes);
                free(initializer_list);
            }
            break;
        case AST_MULTI_ASSIGNMENT:
            if (node->left) {
                AST *lvalue_list = (AST*)node->left;
                for (int i = 0; i < lvalue_list->count; i++)
                    free_ast_node(lvalue_list->nodes[i]);
                free(lvalue_list->nodes);
                free(lvalue_list);
            }
            if (node->right) {
                AST *rvalue_list = (AST*)node->right;
                for (int i = 0; i < rvalue_list->count; i++)
                    free_ast_node(rvalue_list->nodes[i]);
                free(rvalue_list->nodes);
                free(rvalue_list);
            }
            break;
        case AST_ARRAY_INITIALIZER:
            if (node->extra) {
                AST *list = (AST*)node->extra;
                for (int i = 0; i < list->count; i++)
                    free_ast_node(list->nodes[i]);
                free(list->nodes);
                free(list);
            }
            break;
        case AST_ARRAY_ACCESS:
            free_ast_node(node->left);
            free_ast_node(node->right);
            break;
        case AST_LABEL_DECLARATION:
        case AST_JUMP:
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
        case AST_ORG:
        case AST_USE_OPTION:
            free_ast_node(node->left);
            break;
        case AST_USE_MULTI:
            if (node->extra) {
                AST *list = (AST*)node->extra;
                for (int i = 0; i < list->count; i++) free_ast_node(list->nodes[i]);
                free(list->nodes);
                free(list);
            }
            break;
        case AST_PUSH:
            free_ast_node(node->left);
            break;
        case AST_STRUCT_OBJECT_DECLARATION:
            free(node->value);
            free_ast_node(node->left);
            free_ast_node(node->right);
            break;
        case AST_STRUCT_OBJECT_CALL:
            free(node->value);
            break;
        case AST_FIELD_ACCESS:
            free(node->value);
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

/* Main parsing function - converts tokens to AST */
AST *parse(Token *input_tokens, int input_token_count) {
    ParserState state;
    state.token_stream = input_tokens;
    state.total_tokens = input_token_count;
    state.current_token_position = 0;

    // Handle empty input
    if (state.total_tokens == 0 || (state.total_tokens == 1 && state.token_stream[0].type == TOKEN_EOF)) {
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
    
    // Parse all statements until EOF or error
    while (get_current_token_type(&state) != TOKEN_EOF && !has_errors()) {
        ASTNode *node = parse_statement(&state);
        if (node) {
            if (!add_ast_node_to_list(ast, node)) {
                free_ast_node(node);
                free_ast(ast);
                return NULL;
            }
        } else if (has_errors()) break;
        else advance_token(&state);
    }
    
    // Clean up on error
    if (has_errors()) {
        free_ast(ast);
        return NULL;
    }
    
    return ast;
}

