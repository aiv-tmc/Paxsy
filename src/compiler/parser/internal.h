#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#include "parser.h"
#include "../../interfaces/errhandler/errhandler.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Parser state structure.
 * Contains token stream, current position, error recovery flags,
 * and a memory arena for AST nodes.
 */
typedef struct Parser {
    Token* tokens;          /* array of tokens from lexer */
    uint16_t token_count;   /* total number of tokens */
    uint16_t current;       /* index of next token to read */
    uint16_t line;          /* current line (for error reporting) */
    uint16_t column;        /* current column */
    bool panic_mode;        /* set when a syntax error requires recovery */
    bool had_error;         /* true if any syntax error was reported */

    uint8_t* arena;         /* memory arena for AST nodes and dynamic arrays */
    size_t arena_size;      /* total allocated size of arena */
    size_t arena_used;      /* used bytes in arena */
    bool arena_owned;       /* true if arena was allocated by this parser */
} Parser;

/*
 * Inline helpers for token stream access.
 */
static inline TokenType peek(Parser* p) {
    return p->tokens[p->current].type;
}

static inline Token* advance(Parser* p) {
    if (p->current < p->token_count) {
        Token* tok = &p->tokens[p->current++];
        p->line = tok->line;
        p->column = tok->column;
        return tok;
    }
    return &p->tokens[p->token_count - 1];
}

static inline bool match(Parser* p, TokenType type) {
    if (peek(p) == type) {
        advance(p);
        return true;
    }
    return false;
}

static inline Token* expect(Parser* p, TokenType type, const char* msg) {
    if (peek(p) == type) {
        return advance(p);
    }
    errhandler__report_error(p->line, p->column, "parser", msg);
    p->panic_mode = true;
    return NULL;
}

/*
 * Synchronise after a syntax error: skip tokens until a recovery point.
 */
static inline void synchronize(Parser* p) {
    while (peek(p) != TOKEN_EOF) {
        TokenType t = peek(p);
        if (t == TOKEN_SEMICOLON || t == TOKEN_RBRACE || t == TOKEN_ELSE ||
            t == TOKEN_CASE || t == TOKEN_EOF) {
            break;
        }
        advance(p);
    }
    if (peek(p) == TOKEN_SEMICOLON || peek(p) == TOKEN_RBRACE) {
        advance(p);
    }
}

/*
 * Arena and AST node allocation.
 */
void* make_node(Parser* p, NodeType type, size_t size);
void append_child(Parser* p, void*** array, size_t* count, void* child);
char* strdup_custom(const char* s);
void* parser_alloc_raw(Parser* p, size_t size);
char* arena_strdup(Parser* p, const char* s, size_t len);

/*
 * Parse optional modifier tokens (extern, volatile).
 */
uint32_t parse_modifiers(Parser* p);

/*
 * Unified parser for the common pattern "name : Type".
 */
bool parse_name_type(Parser* p, char** out_name, ASTNode** out_type, uint32_t* out_mods);

/*
 * Parse a function signature: ( params ) : return_type.
 * Optionally consumes a name before '('.
 */
bool parse_function_signature(Parser* p, char** out_name, ASTNode*** out_params,
                              size_t* out_param_count, ASTNode** out_return_type,
                              uint32_t* out_mods, bool allow_mods);

/*
 * Free an AST node and all its children recursively.
 */
void free_ast_node(ASTNode* node);

/*
 * Top‑level parsing functions.
 */
ASTNode* parse_program(Parser* p);
ASTNode* parse_top_level_item(Parser* p);
ASTNode* parse_module_decl(Parser* p);
ASTNode* parse_use_decl(Parser* p);
ASTNode* parse_func_decl(Parser* p, uint8_t is_method);
ASTNode* parse_let_decl(Parser* p);
ASTNode* parse_comptime_decl(Parser* p);
ASTNode* parse_extern_decl(Parser* p);
ASTNode* parse_proc_decl(Parser* p);

/*
 * Statement parsers.
 */
ASTNode* parse_block(Parser* p);
ASTNode* parse_statement(Parser* p);
ASTNode* parse_if_stmt(Parser* p);
ASTNode* parse_match_stmt(Parser* p);
ASTNode* parse_match_case(Parser* p);
ASTNode* parse_while_stmt(Parser* p);
ASTNode* parse_do_while_stmt(Parser* p);
ASTNode* parse_for_stmt(Parser* p);
ASTNode* parse_break_stmt(Parser* p);
ASTNode* parse_continue_stmt(Parser* p);
ASTNode* parse_return_stmt(Parser* p);
ASTNode* parse_defer_stmt(Parser* p);
ASTNode* parse_receive_stmt(Parser* p);
ASTNode* parse_receive_case(Parser* p);
ASTNode* parse_asm_stmt(Parser* p);
ASTNode* parse_volatile_stmt(Parser* p);
ASTNode* parse_comptime_stmt(Parser* p);
ASTNode* parse_pass_stmt(Parser* p);
ASTNode* parse_free_stmt(Parser* p);
ASTNode* parse_expr_stmt(Parser* p);

/*
 * Expression parsers.
 */
ASTNode* parse_assignment_expr(Parser* p);
ASTNode* parse_send_expr(Parser* p);
ASTNode* parse_logical_or_expr(Parser* p);
ASTNode* parse_logical_and_expr(Parser* p);
ASTNode* parse_bitwise_or_expr(Parser* p);
ASTNode* parse_bitwise_xor_expr(Parser* p);
ASTNode* parse_bitwise_and_expr(Parser* p);
ASTNode* parse_equality_expr(Parser* p);
ASTNode* parse_relational_expr(Parser* p);
ASTNode* parse_shift_expr(Parser* p);
ASTNode* parse_additive_expr(Parser* p);
ASTNode* parse_multiplicative_expr(Parser* p);
ASTNode* parse_unary_expr(Parser* p);
ASTNode* parse_postfix_expr(Parser* p);
ASTNode* parse_primary_expr(Parser* p);
ASTNode* parse_range_expr(Parser* p);
ASTNode* parse_tuple_literal(Parser* p);

/*
 * Pattern parser.
 */
ASTNode* parse_pattern(Parser* p);

/*
 * Type parser.
 */
ASTNode* parse_type_spec(Parser* p);

#endif
