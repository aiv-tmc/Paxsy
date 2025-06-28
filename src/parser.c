#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "lexer.h"

static int current_token_index = 0;
static Token *tokens = NULL;
static int token_count = 0;
static int start_function_declared = 0;  // Флаг объявления стартовой функции

static TokenType current_token_type();
static void advance();
static void expect(TokenType expected_type);
static Token *current_token();
static void error(const char *message);
static ASTNode *parse_statement();

// parser2.c
static TokenType current_token_type() {
    Token *t = current_token();
    return t ? t->type : TOKEN_EOF;
}

static void advance() {
    if (current_token_index < token_count) current_token_index++;
}

static Token *current_token() {
    if (current_token_index < token_count) return &tokens[current_token_index];
    return NULL;
}

static void error(const char *message) {
    if (current_token_index < token_count) {
        Token *t = &tokens[current_token_index];
        fprintf(stderr, "Parser error at line %d, column %d: %s\n", t->line, t->column, message);
    } else {
        fprintf(stderr, "Parser error at end of input: %s\n", message);
    }
    exit(EXIT_FAILURE);
}

static void expect(TokenType expected_type) {
    TokenType actual = current_token_type();
    if (actual != expected_type) {
        fprintf(stderr, "Expected %s but got %s\n", 
                token_names[expected_type],
                actual == TOKEN_EOF ? "EOF" : token_names[actual]);
        error("Unexpected token");
    }
    advance();
}

// Создание узла AST с дополнительным полем
static ASTNode *create_ast_node(ASTNodeType type, TokenType op_type, 
                                char *value, ASTNode *left, ASTNode *right, ASTNode *extra) {
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    node->type = type;
    node->op_type = op_type;
    node->value = value ? strdup(value) : NULL;
    node->left = left;
    node->right = right;
    node->extra = extra;  // Дополнительное поле для условий/блоков
    return node;
}

static void add_ast_node(AST *ast, ASTNode *node) {
    if (ast->count >= ast->capacity) {
        ast->capacity = ast->capacity == 0 ? 4 : ast->capacity * 2;
        ast->nodes = realloc(ast->nodes, ast->capacity * sizeof(ASTNode*));
        if (!ast->nodes) {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
    }
    ast->nodes[ast->count++] = node;
}

// Парсинг выражений с приоритетами
static ASTNode *parse_expression();
static ASTNode *parse_assignment();
static ASTNode *parse_bitwise_or();
static ASTNode *parse_bitwise_xor();
static ASTNode *parse_bitwise_and();
static ASTNode *parse_equality();
static ASTNode *parse_relational();
static ASTNode *parse_shift();
static ASTNode *parse_additive();
static ASTNode *parse_multiplicative();
static ASTNode *parse_unary();
static ASTNode *parse_primary();

// Парсинг блока кода (однострочный или многострочный)
static ASTNode *parse_block() {
    if (current_token_type() == TOKEN_LCURLY) {
        advance();  // Пропускаем {
        ASTNode *block_node = create_ast_node(AST_BLOCK, 0, NULL, NULL, NULL, NULL);
        AST *block_ast = malloc(sizeof(AST));
        block_ast->nodes = NULL;
        block_ast->count = 0;
        block_ast->capacity = 0;
        
        while (current_token_type() != TOKEN_RCURLY && current_token_type() != TOKEN_EOF) {
            ASTNode *stmt = parse_statement();
            add_ast_node(block_ast, stmt);
        }
        expect(TOKEN_RCURLY);
        
        block_node->extra = (ASTNode*)block_ast;  // Храним блок как под-AST
        return block_node;
    } else {
        // Однострочный блок
        ASTNode *single_stmt = parse_statement();
        return create_ast_node(AST_BLOCK, 0, NULL, single_stmt, NULL, NULL);
    }
}

void print_ast_node(ASTNode *node, int indent) {
    if (!node) return;
    
    for (int i = 0; i < indent; i++) printf("  ");
    
    switch (node->type) {
        case AST_VARIABLE_DECL:
            printf("VariableDecl: %s\n", node->value);
            break;
            
        case AST_BINARY_OP:
            printf("BinaryOp: %s\n", token_names[node->op_type]);
            print_ast_node(node->left, indent + 1);
            print_ast_node(node->right, indent + 1);
            break;
            
        case AST_UNARY_OP:
            printf("UnaryOp: %s\n", token_names[node->op_type]);
            print_ast_node(node->right, indent + 1);  // Unary ops only have right child
            break;
            
        case AST_LITERAL:
            printf("Literal(%s): %s\n", token_names[node->op_type], node->value);
            break;
            
        case AST_IDENTIFIER:
            printf("Identifier: %s\n", node->value);
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
            
        case AST_IF:
            printf("If\n");
            print_ast_node(node->left, indent + 1);  // Условие
            print_ast_node(node->right, indent + 1); // Блок if
            print_ast_node(node->extra, indent + 1); // Else/Elif
            break;
            
        case AST_ELIF:
            printf("Elif\n");
            print_ast_node(node->left, indent + 1);  // Условие
            print_ast_node(node->right, indent + 1); // Блок elif
            print_ast_node(node->extra, indent + 1); // Следующий elif
            break;
            
        case AST_ELSE:
            printf("Else\n");
            print_ast_node(node->left, indent + 1);  // Блок else
            break;
            
        case AST_BLOCK:
            printf("Block\n");
            if (node->extra) {
                // Многострочный блок
                AST *block_ast = (AST*)node->extra;
                for (int i = 0; i < block_ast->count; i++) {
                    print_ast_node(block_ast->nodes[i], indent + 1);
                }
            } else {
                // Однострочный блок
                print_ast_node(node->left, indent + 1);
            }
            break;
            
        case AST_FUNCTION:
            printf("Function: %s\n", node->value);
            print_ast_node(node->left, indent + 1);  // Аргументы
            print_ast_node(node->right, indent + 1); // Тело
            break;
            
        case AST_START_FUNCTION:
            printf("Start Function: %s\n", node->value);
            print_ast_node(node->left, indent + 1);  // Аргументы
            print_ast_node(node->right, indent + 1); // Тело
            break;
            
        case AST_FUNCTION_CALL:
            printf("Call: %s\n", node->value);
            print_ast_node(node->left, indent + 1);  // Аргументы
            break;
    }
}

// Парсинг условных выражений (if/elif/else)
static ASTNode *parse_if_statement() {
    advance();  // Пропускаем if
    ASTNode *cond = parse_expression();  // Условие
    ASTNode *if_block = parse_block();   // Блок if
    
    ASTNode *elif_node = NULL;
    ASTNode *else_node = NULL;
    
    // Обработка elif
    while (current_token_type() == TOKEN_ELIF) {
        advance();  // Пропускаем elif
        ASTNode *elif_cond = parse_expression();
        ASTNode *elif_block = parse_block();
        elif_node = create_ast_node(AST_ELIF, 0, NULL, elif_cond, elif_block, elif_node);
    }
    
    // Обработка else
    if (current_token_type() == TOKEN_ELSE) {
        advance();  // Пропускаем else
        else_node = parse_block();
    }
    
    return create_ast_node(AST_IF, 0, NULL, cond, if_block, 
                          create_ast_node(AST_ELSE, 0, NULL, else_node, elif_node, NULL));
}

// Парсинг функций
static ASTNode *parse_function() {
    bool is_start_function = false;
    if (current_token_type() == TOKEN_DOUBLE_UNDERSCORE) {
        is_start_function = true;
        advance();
    }
    
    if (current_token_type() != TOKEN_ID) {
        error("Expected function name");
    }

    Token *t = current_token();
    char *func_name = strdup(t->value);
    advance();  // Пропускаем имя функции
    
    // Обработка аргументов
    ASTNode *args = NULL;
    if (current_token_type() == TOKEN_LPAREN) {
        advance();  // Пропускаем (
        if (current_token_type() != TOKEN_RPAREN) {
            args = parse_expression();  // Парсим список аргументов
        }
        expect(TOKEN_RPAREN);
    }
    
    ASTNode *body = parse_block();
    
    // Проверка стартовой функции
    if (is_start_function) {
        if (start_function_declared) {
            error("Only one start function allowed");
        }
        start_function_declared = 1;
        return create_ast_node(AST_START_FUNCTION, 0, func_name, args, body, NULL);
    }
    
    return create_ast_node(AST_FUNCTION, 0, func_name, args, body, NULL);
}

// Парсинг выражений с приоритетами
static ASTNode *parse_expression() {
    return parse_assignment();
}

static ASTNode *parse_assignment() {
    ASTNode *left = parse_bitwise_or();
    
    TokenType t = current_token_type();
    if (t == TOKEN_EQUAL || 
        t == TOKEN_PLUS_EQ || t == TOKEN_MINUS_EQ ||
        t == TOKEN_STAR_EQ || t == TOKEN_SLASH_EQ ||
        t == TOKEN_PIPE_EQ || t == TOKEN_AMPERSAND_EQ ||
        t == TOKEN_CARET_EQ || t == TOKEN_TILDE_EQ) {
        
        advance();
        ASTNode *right = parse_assignment();
        
        if (t == TOKEN_EQUAL) {
            return create_ast_node(AST_ASSIGNMENT, t, NULL, left, right, NULL);
        } else {
            return create_ast_node(AST_COMPOUND_ASSIGN, t, NULL, left, right, NULL);
        }
    }
    return left;
}

// Побитовое ИЛИ
static ASTNode *parse_bitwise_or() {
    ASTNode *node = parse_bitwise_xor();
    while (current_token_type() == TOKEN_PIPE) {
        TokenType op = current_token_type();
        advance();
        ASTNode *right = parse_bitwise_xor();
        node = create_ast_node(AST_BINARY_OP, op, NULL, node, right, NULL);
    }
    return node;
}

// Побитовое XOR
static ASTNode *parse_bitwise_xor() {
    ASTNode *node = parse_bitwise_and();
    while (current_token_type() == TOKEN_CARET) {
        TokenType op = current_token_type();
        advance();
        ASTNode *right = parse_bitwise_and();
        node = create_ast_node(AST_BINARY_OP, op, NULL, node, right, NULL);
    }
    return node;
}

// Побитовое И
static ASTNode *parse_bitwise_and() {
    ASTNode *node = parse_equality();
    while (current_token_type() == TOKEN_AMPERSAND) {
        TokenType op = current_token_type();
        advance();
        ASTNode *right = parse_equality();
        node = create_ast_node(AST_BINARY_OP, op, NULL, node, right, NULL);
    }
    return node;
}

// Операции сравнения (==, !=)
static ASTNode *parse_equality() {
    ASTNode *node = parse_relational();
    while (current_token_type() == TOKEN_DOUBLE_EQ || 
           current_token_type() == TOKEN_NE) {
        TokenType op = current_token_type();
        advance();
        ASTNode *right = parse_relational();
        node = create_ast_node(AST_BINARY_OP, op, NULL, node, right, NULL);
    }
    return node;
}

// Операции сравнения (<, >, <=, >=)
static ASTNode *parse_relational() {
    ASTNode *node = parse_shift();
    while (current_token_type() == TOKEN_LT || 
           current_token_type() == TOKEN_GT ||
           current_token_type() == TOKEN_LE ||
           current_token_type() == TOKEN_GE) {
        TokenType op = current_token_type();
        advance();
        ASTNode *right = parse_shift();
        node = create_ast_node(AST_BINARY_OP, op, NULL, node, right, NULL);
    }
    return node;
}

// Побитовые сдвиги
static ASTNode *parse_shift() {
    ASTNode *node = parse_additive();
    while (current_token_type() == TOKEN_SHL || 
           current_token_type() == TOKEN_SHR ||
           current_token_type() == TOKEN_SAL || 
           current_token_type() == TOKEN_SAR ||
           current_token_type() == TOKEN_ROL || 
           current_token_type() == TOKEN_ROR) {
        TokenType op = current_token_type();
        advance();
        ASTNode *right = parse_additive();
        node = create_ast_node(AST_BINARY_OP, op, NULL, node, right, NULL);
    }
    return node;
}

// Сложение/вычитание
static ASTNode *parse_additive() {
    ASTNode *node = parse_multiplicative();
    while (current_token_type() == TOKEN_PLUS || 
           current_token_type() == TOKEN_MINUS) {
        TokenType op = current_token_type();
        advance();
        ASTNode *right = parse_multiplicative();
        node = create_ast_node(AST_BINARY_OP, op, NULL, node, right, NULL);
    }
    return node;
}

// Умножение/деление
static ASTNode *parse_multiplicative() {
    ASTNode *node = parse_unary();
    while (current_token_type() == TOKEN_STAR || 
           current_token_type() == TOKEN_SLASH) {
        TokenType op = current_token_type();
        advance();
        ASTNode *right = parse_unary();
        node = create_ast_node(AST_BINARY_OP, op, NULL, node, right, NULL);
    }
    return node;
}

// Унарные операции
static ASTNode *parse_unary() {
    TokenType t = current_token_type();
    if (t == TOKEN_PLUS || t == TOKEN_MINUS || 
        t == TOKEN_BANG || t == TOKEN_TILDE) {
        advance();
        ASTNode *operand = parse_unary();
        return create_ast_node(AST_UNARY_OP, t, NULL, NULL, operand, NULL);
    }
    return parse_primary();
}

// Первичные выражения
static ASTNode *parse_primary() {
    Token *t = current_token();
    switch (t->type) {
        case TOKEN_INT:
        case TOKEN_REAL:
        case TOKEN_CHAR:
        case TOKEN_STRING: {
            char *value = strdup(t->value);
            advance();
            return create_ast_node(AST_LITERAL, t->type, value, NULL, NULL, NULL);
        }
        case TOKEN_ID: {
            char *value = strdup(t->value);
            advance();
            
            // Проверка на вызов функции
            if (current_token_type() == TOKEN_LPAREN) {
                advance();  // Пропускаем (
                ASTNode *args = NULL;
                if (current_token_type() != TOKEN_RPAREN) {
                    args = parse_expression();  // Аргументы
                }
                expect(TOKEN_RPAREN);
                return create_ast_node(AST_FUNCTION_CALL, 0, value, args, NULL, NULL);
            }
            return create_ast_node(AST_IDENTIFIER, TOKEN_ID, value, NULL, NULL, NULL);
        }
        case TOKEN_LPAREN: {
            advance();
            ASTNode *expr = parse_expression();
            expect(TOKEN_RPAREN);
            return expr;
        }
        default:
            error("Unexpected token in expression");
            return NULL;
    }
}

// Парсинг объявления переменной
static ASTNode *parse_variable_decl() {
    advance();  // Пропускаем $
    Token *id_token = current_token();
    expect(TOKEN_ID);
    
    expect(TOKEN_COLON);
    
    Token *type_token = current_token();
    expect(TOKEN_TYPE);
    
    // Сохраняем имя и тип
    char *decl = malloc(strlen(id_token->value) + strlen(type_token->value) + 4);
    sprintf(decl, "%s:%s", id_token->value, type_token->value);
    
    // Проверка инициализации
    ASTNode *init = NULL;
    if (current_token_type() == TOKEN_EQUAL) {
        advance();
        init = parse_expression();
    }
    
    expect(TOKEN_SEMICOLON);
    return create_ast_node(AST_VARIABLE_DECL, 0, decl, init, NULL, NULL);
}

// Парсинг операторов
static ASTNode *parse_statement() {
    Token *t = current_token();
    if (!t) error("Unexpected end of input");
    
    switch (t->type) {
        case TOKEN_DOLLAR:
            return parse_variable_decl();
            
        case TOKEN_IF:
            return parse_if_statement();
            
        case TOKEN_DOUBLE_UNDERSCORE:
        case TOKEN_UNDERSCORE:
            return parse_function();
            
        default: {
            ASTNode *expr = parse_expression();
            expect(TOKEN_SEMICOLON);
            return expr;
        }
    }
}

// Основная функция парсинга (дополненная инициализация AST)
AST *parse(Token *input_tokens, int input_token_count) {
    tokens = input_tokens;
    token_count = input_token_count;
    current_token_index = 0;
    start_function_declared = 0;

    AST *ast = malloc(sizeof(AST));
    if (!ast) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    ast->nodes = NULL;
    ast->count = 0;
    ast->capacity = 0;
    
    while (current_token_type() != TOKEN_EOF) {
        ASTNode *node = parse_statement();
        add_ast_node(ast, node);
    }
    
    if (!start_function_declared) {
        error("Start function not declared");
    }
    
    return ast;
}

// Освобождение памяти AST-узла (с учетом новых типов)
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
        case AST_ELIF:
            free_ast_node(node->left);
            free_ast_node(node->right);
            free_ast_node(node->extra);
            break;
            
        case AST_ELSE:
            free_ast_node(node->left);
            break;
            
        case AST_FUNCTION:
        case AST_START_FUNCTION:
            free_ast_node(node->left); // Аргументы
            free_ast_node(node->right); // Тело функции
            break;
            
        case AST_FUNCTION_CALL:
            free_ast_node(node->left); // Аргументы
            break;
            
        default:
            free_ast_node(node->left);
            free_ast_node(node->right);
            break;
    }
    
    if (node->value) free(node->value);
    free(node);
}

// Печать AST-узла (полная реализация для всех типов)
/*void print_ast_node(ASTNode *node, int indent) {
    if (!node) return;
    
    for (int i = 0; i < indent; i++) printf("  ");
    
    switch (node->type) {
        case AST_VARIABLE_DECL:
            printf("VariableDecl: %s\n", node->value);
            if (node->left) {
                for (int i = 0; i < indent+1; i++) printf("  ");
                printf("Initializer:\n");
                print_ast_node(node->left, indent+2);
            }
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
            
        case AST_IDENTIFIER:
            printf("Identifier: %s\n", node->value);
            break;
            
        case AST_IF:
            printf("If\n");
            print_ast_node(node->left, indent + 1);
            print_ast_node(node->right, indent + 1);
            print_ast_node(node->extra, indent + 1);
            break;
            
        case AST_ELIF:
            printf("Elif\n");
            print_ast_node(node->left, indent + 1);
            print_ast_node(node->right, indent + 1);
            print_ast_node(node->extra, indent + 1);
            break;
            
        case AST_ELSE:
            printf("Else\n");
            print_ast_node(node->left, indent + 1);
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
            
        case AST_FUNCTION:
            printf("Function: %s\n", node->value);
            print_ast_node(node->left, indent + 1);
            print_ast_node(node->right, indent + 1);
            break;
            
        case AST_START_FUNCTION:
            printf("Start Function: %s\n", node->value);
            print_ast_node(node->left, indent + 1);
            print_ast_node(node->right, indent + 1);
            break;
            
        case AST_FUNCTION_CALL:
            printf("Call: %s\n", node->value);
            print_ast_node(node->left, indent + 1);
            break;
    }
}*/

// Печать всего AST
void print_ast(AST *ast) {
    for (int i = 0; i < ast->count; i++) {
        printf("Statement %d:\n", i + 1);
        print_ast_node(ast->nodes[i], 1);
    }
}

// Освобождение всего AST
void free_ast(AST *ast) {
    if (!ast) return;
    
    for (int i = 0; i < ast->count; i++) {
        free_ast_node(ast->nodes[i]);
    }
    free(ast->nodes);
    free(ast);
}
