#include "internal.h"
#include "../lexer/lexer.h"
#include <stdlib.h>
#include <string.h>

typedef enum {
    PREC_NONE = 0,
    PREC_ASSIGNMENT = 1,
    PREC_CAST = 2,
    PREC_SEND = 3,
    PREC_LOGICAL_OR = 4,
    PREC_LOGICAL_AND = 5,
    PREC_BITWISE_OR = 6,
    PREC_BITWISE_XOR = 7,
    PREC_BITWISE_AND = 8,
    PREC_EQUALITY = 9,
    PREC_RELATIONAL = 10,
    PREC_SHIFT = 11,
    PREC_ADDITIVE = 12,
    PREC_MULTIPLICATIVE = 13,
    PREC_UNARY = 14,
    PREC_POSTFIX = 15
} Precedence;

static ASTNode* parse_expression(Parser* p, Precedence min_prec);
static ASTNode* parse_call(Parser* p, ASTNode* callee);
static ASTNode* parse_index_or_slice_or_struct_init(Parser* p, ASTNode* left);

static ASTNode* parse_expression_from_substring(Parser* main_p, const char* str, size_t len) {
    char* buf = (char*)malloc(len + 1);
    if (!buf) {
        errhandler__report_error(main_p->line, main_p->column, "parser",
                                 "Failed to allocate temporary string");
        return NULL;
    }
    memcpy(buf, str, len);
    buf[len] = '\0';
    Lexer* temp_lexer = lexer__init_lexer(buf);
    if (!temp_lexer) {
        free(buf);
        errhandler__report_error(main_p->line, main_p->column, "parser",
                                 "Failed to create temporary lexer");
        return NULL;
    }
    lexer__tokenize(temp_lexer);
    Token* old_tokens = main_p->tokens;
    uint16_t old_count = main_p->token_count;
    uint16_t old_current = main_p->current;
    main_p->tokens = temp_lexer->tokens;
    main_p->token_count = (uint16_t)temp_lexer->token_count;
    main_p->current = 0;
    ASTNode* expr = parse_assignment_expr(main_p);
    main_p->tokens = old_tokens;
    main_p->token_count = old_count;
    main_p->current = old_current;
    lexer__free_lexer(temp_lexer);
    free(buf);
    return expr;
}

static void parse_interpolated_string(Parser* p, const char* str, InterpolatedString* node) {
    size_t len = strlen(str);
    size_t pos = 0;
    const char* text_start = str;
    size_t text_len = 0;
    while (pos < len) {
        if (str[pos] == '{') {
            if (text_len > 0) {
                InterpolatedSegment* seg = (InterpolatedSegment*)parser_alloc_raw(p, sizeof(InterpolatedSegment));
                seg->kind = SEG_TEXT;
                seg->text = arena_strdup(p, text_start, text_len);
                seg->expr = NULL;
                append_child(p, (void***)&node->segments, &node->segment_count, seg);
                text_len = 0;
            }
            int depth = 1;
            size_t expr_start = pos + 1;
            pos++;
            while (pos < len && depth > 0) {
                if (str[pos] == '{') depth++;
                else if (str[pos] == '}') depth--;
                pos++;
            }
            if (depth != 0) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Unclosed brace in interpolated string");
                p->panic_mode = true;
                return;
            }
            size_t expr_len = pos - expr_start - 1;
            const char* expr_str = str + expr_start;
            ASTNode* expr_node = parse_expression_from_substring(p, expr_str, expr_len);
            if (!expr_node) return;
            InterpolatedSegment* seg = (InterpolatedSegment*)parser_alloc_raw(p, sizeof(InterpolatedSegment));
            seg->kind = SEG_EXPR;
            seg->text = NULL;
            seg->expr = expr_node;
            append_child(p, (void***)&node->segments, &node->segment_count, seg);
            text_start = str + pos;
            text_len = 0;
        } else if (str[pos] == '}') {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Unexpected '}' in interpolated string");
            p->panic_mode = true;
            return;
        } else {
            pos++;
            text_len++;
        }
    }
    if (text_len > 0) {
        InterpolatedSegment* seg = (InterpolatedSegment*)parser_alloc_raw(p, sizeof(InterpolatedSegment));
        seg->kind = SEG_TEXT;
        seg->text = arena_strdup(p, text_start, text_len);
        seg->expr = NULL;
        append_child(p, (void***)&node->segments, &node->segment_count, seg);
    }
}

ASTNode* parse_assignment_expr(Parser* p) { return parse_expression(p, PREC_ASSIGNMENT); }
ASTNode* parse_send_expr(Parser* p) { return parse_expression(p, PREC_SEND); }
ASTNode* parse_logical_or_expr(Parser* p) { return parse_expression(p, PREC_LOGICAL_OR); }
ASTNode* parse_logical_and_expr(Parser* p) { return parse_expression(p, PREC_LOGICAL_AND); }
ASTNode* parse_bitwise_or_expr(Parser* p) { return parse_expression(p, PREC_BITWISE_OR); }
ASTNode* parse_bitwise_xor_expr(Parser* p) { return parse_expression(p, PREC_BITWISE_XOR); }
ASTNode* parse_bitwise_and_expr(Parser* p) { return parse_expression(p, PREC_BITWISE_AND); }
ASTNode* parse_equality_expr(Parser* p) { return parse_expression(p, PREC_EQUALITY); }
ASTNode* parse_relational_expr(Parser* p) { return parse_expression(p, PREC_RELATIONAL); }
ASTNode* parse_shift_expr(Parser* p) { return parse_expression(p, PREC_SHIFT); }
ASTNode* parse_additive_expr(Parser* p) { return parse_expression(p, PREC_ADDITIVE); }
ASTNode* parse_multiplicative_expr(Parser* p) { return parse_expression(p, PREC_MULTIPLICATIVE); }

ASTNode* parse_unary_expr(Parser* p) {
    TokenType type = peek(p);
    if (type == TOKEN_TRY) {
        advance(p);
        ASTNode* expr = parse_unary_expr(p);
        if (!expr) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected expression after try");
            p->panic_mode = true;
            return NULL;
        }
        TryExpr* te = (TryExpr*)make_node(p, NODE_TRY_EXPR, sizeof(TryExpr));
        if (!te) { free_ast_node(expr); return NULL; }
        te->expr = expr;
        return (ASTNode*)te;
    }
    if (type == TOKEN_PLUS || type == TOKEN_MINUS || type == TOKEN_NOT ||
        type == TOKEN_TILDE || type == TOKEN_AMPERSAND || type == TOKEN_AT) {
        Token* tok = advance(p);
        UnaryOp op;
        switch (tok->type) {
            case TOKEN_PLUS:  op = UNARY_PLUS; break;
            case TOKEN_MINUS: op = UNARY_MINUS; break;
            case TOKEN_NOT:   op = UNARY_LOGICAL_NOT; break;
            case TOKEN_TILDE: op = UNARY_BIT_NOT; break;
            case TOKEN_AMPERSAND: op = UNARY_ADDR; break;
            case TOKEN_AT:    op = UNARY_DEREF; break;
            default: op = UNARY_PLUS; break;
        }
        ASTNode* operand = parse_unary_expr(p);
        if (!operand) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected operand after unary operator");
            p->panic_mode = true;
            return NULL;
        }
        UnaryExpr* u = (UnaryExpr*)make_node(p, NODE_UNARY, sizeof(UnaryExpr));
        if (!u) { free_ast_node(operand); return NULL; }
        u->op = op;
        u->operand = operand;
        return (ASTNode*)u;
    } else if (type == TOKEN_BANG) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Unexpected '!' operator (use 'not')");
        p->panic_mode = true;
        advance(p);
        return NULL;
    } else {
        return parse_postfix_expr(p);
    }
}

static ASTNode* parse_call(Parser* p, ASTNode* callee) {
    advance(p);
    CallExpr* call = (CallExpr*)make_node(p, NODE_CALL, sizeof(CallExpr));
    if (!call) { free_ast_node(callee); return NULL; }
    call->callee = callee;
    call->args = NULL;
    call->arg_count = 0;
    if (peek(p) != TOKEN_RPAREN) {
        do {
            ASTNode* arg = parse_assignment_expr(p);
            if (!arg) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Expected argument expression");
                break;
            }
            append_child(p, (void***)&call->args, &call->arg_count, arg);
        } while (match(p, TOKEN_COMMA) && peek(p) != TOKEN_RPAREN);
    }
    if (!match(p, TOKEN_RPAREN)) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected ')' after arguments");
        p->panic_mode = true;
    }
    return (ASTNode*)call;
}

static ASTNode* parse_index_or_slice_or_struct_init(Parser* p, ASTNode* left) {
    advance(p);
    if (peek(p) == TOKEN_DOT) {
        StructInitExpr* init = (StructInitExpr*)make_node(p, NODE_STRUCT_INIT, sizeof(StructInitExpr));
        if (!init) { free_ast_node(left); return NULL; }
        init->struct_type = left;
        init->field_names = NULL;
        init->values = NULL;
        init->count = 0;
        init->named = 1;
        while (peek(p) != TOKEN_RBRACKET && peek(p) != TOKEN_EOF) {
            if (!match(p, TOKEN_DOT)) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Expected '.' for named field");
                break;
            }
            if (!match(p, TOKEN_IDENTIFIER)) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Expected field name");
                break;
            }
            char* fname = strdup_custom(p->tokens[p->current - 1].value);
            if (!match(p, TOKEN_EQUAL)) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Expected '=' after field name");
                free(fname);
                break;
            }
            ASTNode* val = parse_assignment_expr(p);
            if (!val) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Expected field value");
                free(fname);
                break;
            }
            size_t name_count = init->count;
            size_t value_count = init->count;
            append_child(p, (void***)&init->field_names, &name_count, fname);
            append_child(p, (void***)&init->values, &value_count, val);
            init->count = name_count < value_count ? name_count : value_count;
            if (!match(p, TOKEN_COMMA)) break;
            if (peek(p) == TOKEN_RBRACKET) break;
        }
        if (!match(p, TOKEN_RBRACKET)) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected ']' to close struct init");
            p->panic_mode = true;
        }
        return (ASTNode*)init;
    }
    if (peek(p) == TOKEN_RANGE || peek(p) == TOKEN_GREATER) {
        ASTNode* range = parse_range_expr(p);
        if (!range) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected range expression in slice");
            p->panic_mode = true;
            free_ast_node(left);
            return NULL;
        }
        if (!match(p, TOKEN_RBRACKET)) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected ']' after slice");
            p->panic_mode = true;
            free_ast_node(left);
            free_ast_node(range);
            return NULL;
        }
        SliceExpr* slice = (SliceExpr*)make_node(p, NODE_SLICE, sizeof(SliceExpr));
        if (!slice) {
            free_ast_node(left);
            free_ast_node(range);
            return NULL;
        }
        slice->array = left;
        slice->range = range;
        return (ASTNode*)slice;
    }
    ASTNode* first = parse_assignment_expr(p);
    if (!first) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected expression in brackets");
        p->panic_mode = true;
        free_ast_node(left);
        return NULL;
    }
    if (peek(p) == TOKEN_RANGE) {
        advance(p);
        ASTNode* end_expr = parse_assignment_expr(p);
        if (!end_expr) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected end expression in slice");
            p->panic_mode = true;
            free_ast_node(left);
            free_ast_node(first);
            return NULL;
        }
        RangeExpr* range = (RangeExpr*)make_node(p, NODE_RANGE, sizeof(RangeExpr));
        if (!range) {
            free_ast_node(left);
            free_ast_node(first);
            free_ast_node(end_expr);
            return NULL;
        }
        range->start = first;
        range->end = end_expr;
        range->exclude_start = 0;
        range->exclude_end = 0;
        if (!match(p, TOKEN_RBRACKET)) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected ']' after slice");
            p->panic_mode = true;
            free_ast_node(left);
            free_ast_node((ASTNode*)range);
            return NULL;
        }
        SliceExpr* slice = (SliceExpr*)make_node(p, NODE_SLICE, sizeof(SliceExpr));
        if (!slice) {
            free_ast_node(left);
            free_ast_node((ASTNode*)range);
            return NULL;
        }
        slice->array = left;
        slice->range = (ASTNode*)range;
        return (ASTNode*)slice;
    }
    if (peek(p) == TOKEN_COMMA) {
        StructInitExpr* init = (StructInitExpr*)make_node(p, NODE_STRUCT_INIT, sizeof(StructInitExpr));
        if (!init) {
            free_ast_node(left);
            free_ast_node(first);
            return NULL;
        }
        init->struct_type = left;
        init->field_names = NULL;
        init->values = NULL;
        init->count = 0;
        init->named = 0;
        append_child(p, (void***)&init->values, &init->count, first);
        while (match(p, TOKEN_COMMA)) {
            if (peek(p) == TOKEN_RBRACKET) break;
            ASTNode* val = parse_assignment_expr(p);
            if (!val) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Expected expression in struct initialiser");
                p->panic_mode = true;
                break;
            }
            append_child(p, (void***)&init->values, &init->count, val);
        }
        if (!match(p, TOKEN_RBRACKET)) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected ']' to close struct init");
            p->panic_mode = true;
        }
        return (ASTNode*)init;
    }
    if (!match(p, TOKEN_RBRACKET)) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected ']' after index expression");
        p->panic_mode = true;
        free_ast_node(left);
        free_ast_node(first);
        return NULL;
    }
    IndexExpr* idx = (IndexExpr*)make_node(p, NODE_INDEX, sizeof(IndexExpr));
    if (!idx) {
        free_ast_node(left);
        free_ast_node(first);
        return NULL;
    }
    idx->array = left;
    idx->index = first;
    return (ASTNode*)idx;
}

ASTNode* parse_postfix_expr(Parser* p) {
    ASTNode* left = parse_primary_expr(p);
    if (!left) return NULL;
    while (1) {
        TokenType type = peek(p);
        if (type == TOKEN_LPAREN) {
            left = parse_call(p, left);
        } else if (type == TOKEN_LBRACKET) {
            left = parse_index_or_slice_or_struct_init(p, left);
        } else if (type == TOKEN_DOT) {
            advance(p);
            if (!match(p, TOKEN_IDENTIFIER)) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Expected member name after '.'");
                p->panic_mode = true;
                free_ast_node(left);
                return NULL;
            }
            char* member = strdup_custom(p->tokens[p->current - 1].value);
            MemberAccess* ma = (MemberAccess*)make_node(p, NODE_MEMBER_ACCESS, sizeof(MemberAccess));
            if (!ma) { free(member); free_ast_node(left); return NULL; }
            ma->object = left;
            ma->member = member;
            left = (ASTNode*)ma;
        } else if (type == TOKEN_CATCH) {
            advance(p);
            if (!match(p, TOKEN_PIPE)) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Expected '|' after catch");
                p->panic_mode = true;
                free_ast_node(left);
                return NULL;
            }
            if (!match(p, TOKEN_IDENTIFIER)) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Expected variable name in catch");
                p->panic_mode = true;
                free_ast_node(left);
                return NULL;
            }
            char* var = strdup_custom(p->tokens[p->current - 1].value);
            if (!match(p, TOKEN_PIPE)) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Expected '|' after catch variable");
                p->panic_mode = true;
                free(var);
                free_ast_node(left);
                return NULL;
            }
            ASTNode* handler = parse_block(p);
            if (!handler) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Expected block after catch");
                p->panic_mode = true;
                free(var);
                free_ast_node(left);
                return NULL;
            }
            CatchExpr* ce = (CatchExpr*)make_node(p, NODE_CATCH_EXPR, sizeof(CatchExpr));
            if (!ce) {
                free(var);
                free_ast_node(left);
                free_ast_node(handler);
                return NULL;
            }
            ce->expr = left;
            ce->var_name = var;
            ce->handler = handler;
            left = (ASTNode*)ce;
        } else {
            break;
        }
    }
    return left;
}

ASTNode* parse_primary_expr(Parser* p) {
    TokenType type = peek(p);
    if (type == TOKEN_IDENTIFIER) {
        Token* tok = advance(p);
        char* name = strdup_custom(tok->value);
        if (match(p, TOKEN_COLON_COLON)) {
            char** path = NULL;
            size_t path_len = 0;
            append_child(p, (void***)&path, &path_len, name);
            while (1) {
                if (!match(p, TOKEN_IDENTIFIER)) {
                    errhandler__report_error(p->line, p->column, "parser",
                                             "Expected identifier after '::'");
                    break;
                }
                char* part = strdup_custom(p->tokens[p->current - 1].value);
                append_child(p, (void***)&path, &path_len, part);
                if (!match(p, TOKEN_COLON_COLON)) break;
            }
            if (path_len == 0) return NULL;
            char* symbol = path[path_len - 1];
            char** new_path = NULL;
            if (path_len > 1) {
                new_path = (char**)parser_alloc_raw(p, (path_len - 1) * sizeof(char*));
                if (!new_path) {
                    free(symbol);
                    for (size_t i = 0; i < path_len - 1; ++i) free(path[i]);
                    return NULL;
                }
                for (size_t i = 0; i < path_len - 1; ++i) new_path[i] = path[i];
            }
            ModuleAccess* ma = (ModuleAccess*)make_node(p, NODE_MODULE_ACCESS, sizeof(ModuleAccess));
            if (!ma) { free(symbol); return NULL; }
            ma->path = new_path;
            ma->path_len = path_len - 1;
            ma->symbol = symbol;
            return (ASTNode*)ma;
        } else {
            IdentExpr* id = (IdentExpr*)make_node(p, NODE_IDENT, sizeof(IdentExpr));
            if (!id) { free(name); return NULL; }
            id->name = name;
            return (ASTNode*)id;
        }
    } else if (type == TOKEN_NUMBER_LITERAL) {
        Token* tok = advance(p);
        LiteralExpr* lit = (LiteralExpr*)make_node(p, NODE_LITERAL, sizeof(LiteralExpr));
        if (!lit) return NULL;
        lit->kind = LIT_NUMBER;
        lit->raw_value = strdup_custom(tok->value);
        lit->string_val = NULL;
        return (ASTNode*)lit;
    } else if (type == TOKEN_STRING_LITERAL) {
        Token* tok = advance(p);
        LiteralExpr* lit = (LiteralExpr*)make_node(p, NODE_LITERAL, sizeof(LiteralExpr));
        if (!lit) return NULL;
        lit->kind = LIT_STRING;
        lit->string_val = strdup_custom(tok->value);
        lit->raw_value = NULL;
        return (ASTNode*)lit;
    } else if (type == TOKEN_INTERPOLATED_STRING) {
        Token* tok = advance(p);
        InterpolatedString* is = (InterpolatedString*)make_node(p, NODE_INTERPOLATED_STRING, sizeof(InterpolatedString));
        if (!is) return NULL;
        is->segments = NULL;
        is->segment_count = 0;
        parse_interpolated_string(p, tok->value, is);
        return (ASTNode*)is;
    } else if (type == TOKEN_CHAR_LITERAL) {
        Token* tok = advance(p);
        LiteralExpr* lit = (LiteralExpr*)make_node(p, NODE_LITERAL, sizeof(LiteralExpr));
        if (!lit) return NULL;
        lit->kind = LIT_CHAR;
        lit->string_val = strdup_custom(tok->value);
        lit->raw_value = NULL;
        return (ASTNode*)lit;
    } else if (type == TOKEN_TRUE || type == TOKEN_FALSE) {
        Token* tok = advance(p);
        LiteralExpr* lit = (LiteralExpr*)make_node(p, NODE_LITERAL, sizeof(LiteralExpr));
        if (!lit) return NULL;
        lit->kind = LIT_BOOL;
        lit->bool_val = (tok->type == TOKEN_TRUE) ? 1 : 0;
        lit->raw_value = NULL;
        return (ASTNode*)lit;
    } else if (type == TOKEN_NONE) {
        advance(p);
        LiteralExpr* lit = (LiteralExpr*)make_node(p, NODE_LITERAL, sizeof(LiteralExpr));
        if (!lit) return NULL;
        lit->kind = LIT_NONE;
        return (ASTNode*)lit;
    } else if (type == TOKEN_NULL) {
        advance(p);
        LiteralExpr* lit = (LiteralExpr*)make_node(p, NODE_LITERAL, sizeof(LiteralExpr));
        if (!lit) return NULL;
        lit->kind = LIT_NULL;
        return (ASTNode*)lit;
    } else if (type == TOKEN_LPAREN) {
        advance(p);
        ASTNode* expr = parse_assignment_expr(p);
        if (!match(p, TOKEN_RPAREN)) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected ')' after expression");
            p->panic_mode = true;
            free_ast_node(expr);
            return NULL;
        }
        return expr;
    } else if (type == TOKEN_IF) {
        advance(p);
        ASTNode* cond = parse_assignment_expr(p);
        if (!cond) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected condition in if expression");
            p->panic_mode = true;
            return NULL;
        }
        ASTNode* then_expr = parse_block(p);
        if (!then_expr) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected then block in if expression");
            p->panic_mode = true;
            free_ast_node(cond);
            return NULL;
        }
        ASTNode* else_expr = NULL;
        if (match(p, TOKEN_ELSE)) {
            else_expr = parse_assignment_expr(p);
            if (!else_expr) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Expected else expression");
                p->panic_mode = true;
                free_ast_node(cond);
                free_ast_node(then_expr);
                return NULL;
            }
        }
        IfExpr* ie = (IfExpr*)make_node(p, NODE_IF_EXPR, sizeof(IfExpr));
        if (!ie) {
            free_ast_node(cond);
            free_ast_node(then_expr);
            free_ast_node(else_expr);
            return NULL;
        }
        ie->condition = cond;
        ie->then_expr = then_expr;
        ie->else_expr = else_expr;
        return (ASTNode*)ie;
    } else if (type == TOKEN_ERROR) {
        advance(p);
        ASTNode* code = parse_assignment_expr(p);
        if (!code) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected error code expression");
            p->panic_mode = true;
            return NULL;
        }
        ErrorExpr* ee = (ErrorExpr*)make_node(p, NODE_ERROR_EXPR, sizeof(ErrorExpr));
        if (!ee) { free_ast_node(code); return NULL; }
        ee->code = code;
        return (ASTNode*)ee;
    } else if (type == TOKEN_SPAWN) {
        advance(p);
        if (peek(p) == TOKEN_IDENTIFIER) {
            Token* tok = advance(p);
            char* name = strdup_custom(tok->value);
            SpawnExpr* se = (SpawnExpr*)make_node(p, NODE_SPAWN_EXPR, sizeof(SpawnExpr));
            if (!se) { free(name); return NULL; }
            se->proc_name = name;
            se->proc_body = NULL;
            return (ASTNode*)se;
        } else if (peek(p) == TOKEN_LBRACE) {
            ASTNode* body = parse_block(p);
            if (!body) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Expected proc body after spawn");
                p->panic_mode = true;
                return NULL;
            }
            SpawnExpr* se = (SpawnExpr*)make_node(p, NODE_SPAWN_EXPR, sizeof(SpawnExpr));
            if (!se) { free_ast_node(body); return NULL; }
            se->proc_name = NULL;
            se->proc_body = body;
            return (ASTNode*)se;
        } else {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected identifier or block after spawn");
            p->panic_mode = true;
            return NULL;
        }
    } else if (type == TOKEN_SELF) {
        advance(p);
        SelfExpr* se = (SelfExpr*)make_node(p, NODE_SELF_EXPR, sizeof(SelfExpr));
        return (ASTNode*)se;
    } else if (type == TOKEN_LBRACKET) {
        advance(p);
        if (peek(p) == TOKEN_DOT) {
            StructInitExpr* init = (StructInitExpr*)make_node(p, NODE_STRUCT_INIT, sizeof(StructInitExpr));
            if (!init) return NULL;
            init->struct_type = NULL;
            init->field_names = NULL;
            init->values = NULL;
            init->count = 0;
            init->named = 1;
            while (peek(p) != TOKEN_RBRACKET && peek(p) != TOKEN_EOF) {
                if (!match(p, TOKEN_DOT)) {
                    errhandler__report_error(p->line, p->column, "syntax",
                                             "Expected '.' for named field");
                    break;
                }
                if (!match(p, TOKEN_IDENTIFIER)) {
                    errhandler__report_error(p->line, p->column, "syntax",
                                             "Expected field name");
                    break;
                }
                char* fname = strdup_custom(p->tokens[p->current - 1].value);
                if (!match(p, TOKEN_EQUAL)) {
                    errhandler__report_error(p->line, p->column, "syntax",
                                             "Expected '=' after field name");
                    free(fname);
                    break;
                }
                ASTNode* val = parse_assignment_expr(p);
                if (!val) {
                    errhandler__report_error(p->line, p->column, "syntax",
                                             "Expected field value");
                    free(fname);
                    break;
                }
                size_t name_count = init->count;
                size_t value_count = init->count;
                append_child(p, (void***)&init->field_names, &name_count, fname);
                append_child(p, (void***)&init->values, &value_count, val);
                init->count = name_count < value_count ? name_count : value_count;
                if (!match(p, TOKEN_COMMA)) break;
                if (peek(p) == TOKEN_RBRACKET) break;
            }
            if (!match(p, TOKEN_RBRACKET)) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Expected ']' to close struct init");
                p->panic_mode = true;
            }
            return (ASTNode*)init;
        }
        ArrayLiteral* arr = (ArrayLiteral*)make_node(p, NODE_ARRAY_LITERAL, sizeof(ArrayLiteral));
        if (!arr) return NULL;
        arr->elements = NULL;
        arr->count = 0;
        while (peek(p) != TOKEN_RBRACKET && peek(p) != TOKEN_EOF) {
            ASTNode* elem = parse_assignment_expr(p);
            if (!elem) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Expected expression in array literal");
                p->panic_mode = true;
                break;
            }
            append_child(p, (void***)&arr->elements, &arr->count, elem);
            if (!match(p, TOKEN_COMMA)) break;
        }
        if (!match(p, TOKEN_RBRACKET)) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected ']' to close array literal");
            p->panic_mode = true;
        }
        return (ASTNode*)arr;
    } else if (type == TOKEN_ALLOC) {
        advance(p);
        AllocExpr* alloc = (AllocExpr*)make_node(p, NODE_ALLOC_EXPR, sizeof(AllocExpr));
        if (!alloc) return NULL;
        alloc->kind = ALLOC_NONE;
        alloc->list = NULL;
        alloc->list_count = 0;
        alloc->range = NULL;
        alloc->type = NULL;
        if (match(p, TOKEN_NONE)) {
            alloc->kind = ALLOC_NONE;
        } else if (match(p, TOKEN_LBRACKET)) {
            bool is_range = false;
            if (peek(p) == TOKEN_RANGE || peek(p) == TOKEN_GREATER) {
                is_range = true;
            } else {
                uint16_t scan = p->current;
                int depth = 1;
                while (scan < p->token_count && depth > 0) {
                    TokenType t = p->tokens[scan].type;
                    if (t == TOKEN_LBRACKET) depth++;
                    else if (t == TOKEN_RBRACKET) depth--;
                    else if (t == TOKEN_RANGE && depth == 1) {
                        is_range = true;
                        break;
                    }
                    scan++;
                }
            }
            if (is_range) {
                ASTNode* range = parse_range_expr(p);
                if (!range) {
                    errhandler__report_error(p->line, p->column, "syntax",
                                             "Expected range expression in alloc");
                    p->panic_mode = true;
                } else {
                    alloc->kind = ALLOC_RANGE;
                    alloc->range = range;
                }
                if (!match(p, TOKEN_RBRACKET)) {
                    errhandler__report_error(p->line, p->column, "syntax",
                                             "Expected ']' after range in alloc");
                    p->panic_mode = true;
                }
            } else {
                alloc->kind = ALLOC_LIST;
                while (peek(p) != TOKEN_RBRACKET && peek(p) != TOKEN_EOF) {
                    ASTNode* elem = parse_assignment_expr(p);
                    if (!elem) {
                        errhandler__report_error(p->line, p->column, "syntax",
                                                 "Expected expression in alloc list");
                        p->panic_mode = true;
                        break;
                    }
                    append_child(p, (void***)&alloc->list, &alloc->list_count, elem);
                    if (!match(p, TOKEN_COMMA)) break;
                }
                if (!match(p, TOKEN_RBRACKET)) {
                    errhandler__report_error(p->line, p->column, "syntax",
                                             "Expected ']' to close alloc list");
                    p->panic_mode = true;
                }
            }
        } else {
            alloc->kind = ALLOC_NONE;
        }
        if (!match(p, TOKEN_AS)) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected 'as' in alloc expression");
            p->panic_mode = true;
            return (ASTNode*)alloc;
        }
        alloc->type = parse_type_spec(p);
        if (!alloc->type) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected type after 'as'");
            p->panic_mode = true;
        }
        return (ASTNode*)alloc;
    } else {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Unexpected token in expression");
        p->panic_mode = true;
        return NULL;
    }
}

ASTNode* parse_range_expr(Parser* p) {
    uint8_t exclude_start = 0, exclude_end = 0;
    ASTNode* start = NULL;
    ASTNode* end = NULL;
    if (match(p, TOKEN_GREATER)) exclude_start = 1;
    if (peek(p) != TOKEN_RANGE) {
        start = parse_assignment_expr(p);
        if (!start) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected start of range");
            return NULL;
        }
    }
    if (!match(p, TOKEN_RANGE)) {
        errhandler__report_error(p->line, p->column, "syntax", "Expected '..' in range");
        free_ast_node(start);
        return NULL;
    }
    if (match(p, TOKEN_LESS)) exclude_end = 1;
    if (peek(p) != TOKEN_RBRACKET && peek(p) != TOKEN_COMMA && peek(p) != TOKEN_PIPE &&
        peek(p) != TOKEN_SEMICOLON && peek(p) != TOKEN_RPAREN) {
        end = parse_assignment_expr(p);
        if (!end) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected end of range");
            free_ast_node(start);
            return NULL;
        }
    }
    RangeExpr* r = (RangeExpr*)make_node(p, NODE_RANGE, sizeof(RangeExpr));
    if (!r) {
        free_ast_node(start);
        free_ast_node(end);
        return NULL;
    }
    r->start = start;
    r->end = end;
    r->exclude_start = exclude_start;
    r->exclude_end = exclude_end;
    return (ASTNode*)r;
}

static ASTNode* parse_expression(Parser* p, Precedence min_prec) {
    ASTNode* left = parse_unary_expr(p);
    if (!left) return NULL;
    while (1) {
        TokenType type = peek(p);
        Precedence prec;
        bool right_assoc = false;
        switch (type) {
            case TOKEN_EQUAL:
            case TOKEN_PLUS_EQUAL: case TOKEN_MINUS_EQUAL: case TOKEN_STAR_EQUAL:
            case TOKEN_SLASH_EQUAL: case TOKEN_PERCENT_EQUAL: case TOKEN_DOLLAR_EQUAL:
            case TOKEN_AMPERSAND_EQUAL: case TOKEN_CARET_EQUAL: case TOKEN_LSHIFT_EQUAL:
            case TOKEN_RSHIFT_EQUAL: case TOKEN_LSHIFT_LOGICAL_EQUAL: case TOKEN_RSHIFT_LOGICAL_EQUAL:
                prec = PREC_ASSIGNMENT; right_assoc = true; break;
            case TOKEN_AS: prec = PREC_CAST; right_assoc = false; break;
            case TOKEN_ARROW: prec = PREC_SEND; right_assoc = false; break;
            case TOKEN_OR: prec = PREC_LOGICAL_OR; right_assoc = false; break;
            case TOKEN_AND: prec = PREC_LOGICAL_AND; right_assoc = false; break;
            case TOKEN_DOLLAR: prec = PREC_BITWISE_OR; right_assoc = false; break;
            case TOKEN_CARET: prec = PREC_BITWISE_XOR; right_assoc = false; break;
            case TOKEN_AMPERSAND: prec = PREC_BITWISE_AND; right_assoc = false; break;
            case TOKEN_EQUAL_EQUAL: case TOKEN_NOT_EQUAL:
                prec = PREC_EQUALITY; right_assoc = false; break;
            case TOKEN_LESS: case TOKEN_GREATER: case TOKEN_LESS_EQUAL: case TOKEN_GREATER_EQUAL:
                prec = PREC_RELATIONAL; right_assoc = false; break;
            case TOKEN_LSHIFT: case TOKEN_RSHIFT: case TOKEN_LSHIFT_LOGICAL: case TOKEN_RSHIFT_LOGICAL:
                prec = PREC_SHIFT; right_assoc = false; break;
            case TOKEN_PLUS: case TOKEN_MINUS:
                prec = PREC_ADDITIVE; right_assoc = false; break;
            case TOKEN_STAR: case TOKEN_SLASH: case TOKEN_PERCENT:
                prec = PREC_MULTIPLICATIVE; right_assoc = false; break;
            default: prec = PREC_NONE; break;
        }
        if (prec == PREC_NONE || prec < min_prec) break;
        if (type == TOKEN_AS) {
            advance(p);
            ASTNode* type_node = parse_type_spec(p);
            if (!type_node) { free_ast_node(left); return NULL; }
            CastExpr* cast = (CastExpr*)make_node(p, NODE_CAST, sizeof(CastExpr));
            if (!cast) { free_ast_node(left); free_ast_node(type_node); return NULL; }
            cast->expr = left;
            cast->target_type = type_node;
            left = (ASTNode*)cast;
            min_prec = (Precedence)(prec + 1);
            continue;
        }
        Token* op_tok = advance(p);
        Precedence next_min = right_assoc ? prec : (Precedence)(prec + 1);
        ASTNode* right = parse_expression(p, next_min);
        if (!right) { free_ast_node(left); return NULL; }
        if (prec == PREC_ASSIGNMENT) {
            AssignOp op;
            switch (op_tok->type) {
                case TOKEN_EQUAL: op = ASSIGN_NORMAL; break;
                case TOKEN_PLUS_EQUAL: op = ASSIGN_ADD; break;
                case TOKEN_MINUS_EQUAL: op = ASSIGN_SUB; break;
                case TOKEN_STAR_EQUAL: op = ASSIGN_MUL; break;
                case TOKEN_SLASH_EQUAL: op = ASSIGN_DIV; break;
                case TOKEN_PERCENT_EQUAL: op = ASSIGN_MOD; break;
                case TOKEN_DOLLAR_EQUAL: op = ASSIGN_BIT_OR; break;
                case TOKEN_AMPERSAND_EQUAL: op = ASSIGN_BIT_AND; break;
                case TOKEN_CARET_EQUAL: op = ASSIGN_BIT_XOR; break;
                case TOKEN_LSHIFT_EQUAL: op = ASSIGN_SHL; break;
                case TOKEN_RSHIFT_EQUAL: op = ASSIGN_SHR; break;
                case TOKEN_LSHIFT_LOGICAL_EQUAL: op = ASSIGN_SHL_LOG; break;
                case TOKEN_RSHIFT_LOGICAL_EQUAL: op = ASSIGN_SHR_LOG; break;
                default: op = ASSIGN_NORMAL; break;
            }
            AssignExpr* a = (AssignExpr*)make_node(p, NODE_ASSIGN, sizeof(AssignExpr));
            if (!a) { free_ast_node(left); free_ast_node(right); return NULL; }
            a->op = op; a->left = left; a->right = right;
            left = (ASTNode*)a;
        } else if (prec == PREC_SEND) {
            SendExpr* s = (SendExpr*)make_node(p, NODE_SEND, sizeof(SendExpr));
            if (!s) { free_ast_node(left); free_ast_node(right); return NULL; }
            s->target = left; s->message = right;
            left = (ASTNode*)s;
        } else {
            BinaryOp op;
            switch (op_tok->type) {
                case TOKEN_OR: op = BIN_LOGICAL_OR; break;
                case TOKEN_AND: op = BIN_LOGICAL_AND; break;
                case TOKEN_DOLLAR: op = BIN_OR; break;
                case TOKEN_CARET: op = BIN_XOR; break;
                case TOKEN_AMPERSAND: op = BIN_AND; break;
                case TOKEN_EQUAL_EQUAL: op = BIN_EQ; break;
                case TOKEN_NOT_EQUAL: op = BIN_NE; break;
                case TOKEN_LESS: op = BIN_LT; break;
                case TOKEN_GREATER: op = BIN_GT; break;
                case TOKEN_LESS_EQUAL: op = BIN_LE; break;
                case TOKEN_GREATER_EQUAL: op = BIN_GE; break;
                case TOKEN_LSHIFT: op = BIN_SHL; break;
                case TOKEN_RSHIFT: op = BIN_SHR; break;
                case TOKEN_LSHIFT_LOGICAL: op = BIN_SHL_LOG; break;
                case TOKEN_RSHIFT_LOGICAL: op = BIN_SHR_LOG; break;
                case TOKEN_PLUS: op = BIN_ADD; break;
                case TOKEN_MINUS: op = BIN_SUB; break;
                case TOKEN_STAR: op = BIN_MUL; break;
                case TOKEN_SLASH: op = BIN_DIV; break;
                case TOKEN_PERCENT: op = BIN_MOD; break;
                default: op = BIN_ADD; break;
            }
            BinaryExpr* b = (BinaryExpr*)make_node(p, NODE_BINARY, sizeof(BinaryExpr));
            if (!b) { free_ast_node(left); free_ast_node(right); return NULL; }
            b->op = op; b->left = left; b->right = right;
            left = (ASTNode*)b;
        }
    }
    return left;
}
