#include "internal.h"
#include <stdlib.h>
#include <string.h>

/*
 * Parses a block: { statement* }.
 */
ASTNode* parse_block(Parser* p) {
    if (!match(p, TOKEN_LBRACE)) {
        errhandler__report_error(p->line, p->column, "syntax", "Expected '{'");
        p->panic_mode = true;
        return NULL;
    }

    Block* block = (Block*)make_node(p, NODE_BLOCK, sizeof(Block));
    if (!block) {
        return NULL;
    }
    block->statements = NULL;
    block->stmt_count = 0;

    while (peek(p) != TOKEN_RBRACE && peek(p) != TOKEN_EOF) {
        if (p->panic_mode) {
            synchronize_progress(p);
            p->panic_mode = false;
            if (peek(p) == TOKEN_EOF) {
                break;
            }
        }
        ASTNode* stmt = parse_statement(p);
        if (stmt) {
            append_child(p, (void***)&block->statements, &block->stmt_count, stmt);
        } else {
            if (peek(p) != TOKEN_RBRACE && peek(p) != TOKEN_EOF) {
                synchronize_progress(p);
                p->panic_mode = false;
            }
        }
    }

    if (!match(p, TOKEN_RBRACE)) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected '}' to close block");
        p->panic_mode = true;
    }
    return (ASTNode*)block;
}

/*
 * Dispatcher for statements.
 */
ASTNode* parse_statement(Parser* p) {
    switch (peek(p)) {
        case TOKEN_LBRACE:       return parse_block(p);
        case TOKEN_LET:          return parse_let_decl(p);
        case TOKEN_USE:          return parse_use_decl(p);
        case TOKEN_IF:           return parse_if_stmt(p);
        case TOKEN_MATCH:        return parse_match_stmt(p);
        case TOKEN_WHILE:        return parse_while_stmt(p);
        case TOKEN_DO:           return parse_do_while_stmt(p);
        case TOKEN_FOR:          return parse_for_stmt(p);
        case TOKEN_BREAK:        return parse_break_stmt(p);
        case TOKEN_CONTINUE:     return parse_continue_stmt(p);
        case TOKEN_RETURN:       return parse_return_stmt(p);
        case TOKEN_DEFER:        return parse_defer_stmt(p);
        case TOKEN_RECEIVE:      return parse_receive_stmt(p);
        case TOKEN_ASM:          return parse_asm_stmt(p);
        case TOKEN_VOLATILE:     return parse_volatile_stmt(p);
        case TOKEN_COMPTIME:     return parse_comptime_stmt(p);
        case TOKEN_PASS:         return parse_pass_stmt(p);
        case TOKEN_FREE:         return parse_free_stmt(p);
        case TOKEN_SEMICOLON:    advance(p); return make_node(p, NODE_PASS_STMT, sizeof(PassStmt));
        default:                 return parse_expr_stmt(p);
    }
}

/*
 * Parses an if statement: if cond { ... } [ else if ... ] [ else { ... } ].
 */
ASTNode* parse_if_stmt(Parser* p) {
    advance(p); // consume TOKEN_IF

    ASTNode* cond = parse_assignment_expr(p);
    if (!cond) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected condition expression");
        p->panic_mode = true;
        return NULL;
    }

    ASTNode* then_branch = parse_block(p);
    if (!then_branch) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected then block");
        p->panic_mode = true;
        free_ast_node(cond);
        return NULL;
    }

    IfStmt* stmt = (IfStmt*)make_node(p, NODE_IF_STMT, sizeof(IfStmt));
    if (!stmt) {
        free_ast_node(cond);
        free_ast_node(then_branch);
        return NULL;
    }
    stmt->condition = cond;
    stmt->then_branch = then_branch;
    stmt->else_if_conditions = NULL;
    stmt->else_if_branches = NULL;
    stmt->else_if_count = 0;
    stmt->else_branch = NULL;

    while (match(p, TOKEN_ELSE)) {
        if (peek(p) == TOKEN_IF) {
            advance(p); // consume IF
            ASTNode* econd = parse_assignment_expr(p);
            if (!econd) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Expected condition in else-if");
                p->panic_mode = true;
                break;
            }
            ASTNode* ebody = parse_block(p);
            if (!ebody) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Expected block for else-if");
                free_ast_node(econd);
                p->panic_mode = true;
                break;
            }
            size_t cond_count = stmt->else_if_count;
            size_t branch_count = stmt->else_if_count;
            append_child(p, (void***)&stmt->else_if_conditions, &cond_count, econd);
            append_child(p, (void***)&stmt->else_if_branches, &branch_count, ebody);
            stmt->else_if_count = cond_count < branch_count ? cond_count : branch_count;
        } else {
            ASTNode* ebody = parse_block(p);
            if (!ebody) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Expected block for else");
                p->panic_mode = true;
                break;
            }
            stmt->else_branch = ebody;
            break;
        }
    }

    return (ASTNode*)stmt;
}

/*
 * Parses a match statement: match expr { case pattern { ... } ... [ else { ... } ] }.
 */
ASTNode* parse_match_stmt(Parser* p) {
    advance(p); // consume TOKEN_MATCH

    ASTNode* expr = parse_assignment_expr(p);
    if (!expr) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected match expression");
        p->panic_mode = true;
        return NULL;
    }

    if (!match(p, TOKEN_LBRACE)) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected '{' after match expression");
        p->panic_mode = true;
        free_ast_node(expr);
        return NULL;
    }

    MatchStmt* stmt = (MatchStmt*)make_node(p, NODE_MATCH_STMT, sizeof(MatchStmt));
    if (!stmt) {
        free_ast_node(expr);
        return NULL;
    }
    stmt->expr = expr;
    stmt->cases = NULL;
    stmt->case_count = 0;
    stmt->else_case = NULL;

    while (peek(p) != TOKEN_RBRACE && peek(p) != TOKEN_EOF) {
        if (match(p, TOKEN_CASE)) {
            ASTNode* case_node = parse_match_case(p);
            if (case_node) {
                append_child(p, (void***)&stmt->cases, &stmt->case_count, case_node);
            } else {
                p->panic_mode = true;
                synchronize_progress(p);
            }
        } else if (match(p, TOKEN_ELSE)) {
            if (stmt->else_case) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Multiple else clauses in match");
                p->panic_mode = true;
            }
            ASTNode* body = parse_block(p);
            if (!body) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Expected block for else");
                p->panic_mode = true;
            } else {
                stmt->else_case = body;
            }
        } else {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected 'case' or 'else' in match");
            p->panic_mode = true;
            synchronize_progress(p);
        }
    }

    if (!match(p, TOKEN_RBRACE)) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected '}' to close match");
        p->panic_mode = true;
    }
    return (ASTNode*)stmt;
}

/*
 * Parses a match case: case pattern { ... }.
 */
ASTNode* parse_match_case(Parser* p) {
    ASTNode* pattern = parse_pattern(p);
    if (!pattern) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected pattern in case");
        p->panic_mode = true;
        return NULL;
    }

    ASTNode* body = parse_block(p);
    if (!body) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected block after case pattern");
        p->panic_mode = true;
        free_ast_node(pattern);
        return NULL;
    }

    MatchCase* mc = (MatchCase*)make_node(p, NODE_MATCH_CASE, sizeof(MatchCase));
    if (!mc) {
        free_ast_node(pattern);
        free_ast_node(body);
        return NULL;
    }
    mc->pattern = pattern;
    mc->body = body;
    return (ASTNode*)mc;
}

/*
 * Parses a pattern: literal, ( pattern, ... ), identifier (capture),
 * or qualified name (e.g. Status.OK) treated as a literal.
 */
ASTNode* parse_pattern(Parser* p) {
    if (peek(p) == TOKEN_LPAREN) {
        advance(p);
        Pattern* pat = (Pattern*)make_node(p, NODE_PATTERN_TUPLE, sizeof(Pattern));
        if (!pat) {
            return NULL;
        }
        pat->kind = PATTERN_TUPLE;
        pat->tuple.elements = NULL;
        pat->tuple.count = 0;

        while (peek(p) != TOKEN_RPAREN && peek(p) != TOKEN_EOF) {
            ASTNode* elem = parse_pattern(p);
            if (!elem) {
                p->panic_mode = true;
                break;
            }
            append_child(p, (void***)&pat->tuple.elements, &pat->tuple.count, elem);
            if (!match(p, TOKEN_COMMA)) {
                break;
            }
        }
        if (!match(p, TOKEN_RPAREN)) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected ')' to close tuple pattern");
            p->panic_mode = true;
        }
        return (ASTNode*)pat;
    }

    // If the next token is an identifier followed by '.' or '::',
    // it is a qualified name literal (e.g. Color.RED or mod::Color.RED).
    if (peek(p) == TOKEN_IDENTIFIER && p->current + 1 < p->token_count) {
        TokenType next = p->tokens[p->current + 1].type;
        if (next == TOKEN_DOT || next == TOKEN_COLON_COLON) {
            ASTNode* expr = parse_postfix_expr(p);
            Pattern* pat = (Pattern*)make_node(p, NODE_PATTERN_LITERAL, sizeof(Pattern));
            if (!pat) {
                free_ast_node(expr);
                return NULL;
            }
            pat->kind = PATTERN_LITERAL;
            pat->literal = expr;
            return (ASTNode*)pat;
        }
        // Plain identifier -> capture pattern
        char* name = strdup_custom(p->tokens[p->current].value);
        advance(p);
        Pattern* pat = (Pattern*)make_node(p, NODE_PATTERN_CAPTURE, sizeof(Pattern));
        if (!pat) {
            free(name);
            return NULL;
        }
        pat->kind = PATTERN_CAPTURE;
        pat->capture_name = name;
        return (ASTNode*)pat;
    }

    // Otherwise treat as a literal expression (number, string, etc.)
    ASTNode* lit = parse_primary_expr(p);
    if (!lit) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected pattern literal");
        p->panic_mode = true;
        return NULL;
    }
    Pattern* pat = (Pattern*)make_node(p, NODE_PATTERN_LITERAL, sizeof(Pattern));
    if (!pat) {
        free_ast_node(lit);
        return NULL;
    }
    pat->kind = PATTERN_LITERAL;
    pat->literal = lit;
    return (ASTNode*)pat;
}

/*
 * Parses a while loop: [label:] while cond { ... }.
 */
ASTNode* parse_while_stmt(Parser* p) {
    char* label = NULL;
    if (peek(p) == TOKEN_IDENTIFIER && p->current + 1 < p->token_count &&
        p->tokens[p->current + 1].type == TOKEN_COLON) {
        label = strdup_custom(p->tokens[p->current].value);
        advance(p);
        advance(p); // consume colon
    }

    if (!match(p, TOKEN_WHILE)) {
        errhandler__report_error(p->line, p->column, "syntax", "Expected 'while'");
        p->panic_mode = true;
        free(label);
        return NULL;
    }

    ASTNode* cond = parse_assignment_expr(p);
    if (!cond) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected while condition");
        p->panic_mode = true;
        free(label);
        return NULL;
    }

    ASTNode* body = parse_block(p);
    if (!body) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected while body block");
        p->panic_mode = true;
        free_ast_node(cond);
        free(label);
        return NULL;
    }

    WhileStmt* stmt = (WhileStmt*)make_node(p, NODE_WHILE_STMT, sizeof(WhileStmt));
    if (!stmt) {
        free_ast_node(cond);
        free_ast_node(body);
        free(label);
        return NULL;
    }
    stmt->label = label;
    stmt->condition = cond;
    stmt->body = body;
    return (ASTNode*)stmt;
}

/*
 * Parses a do-while loop: [label:] do { ... } while cond ;.
 */
ASTNode* parse_do_while_stmt(Parser* p) {
    char* label = NULL;
    if (peek(p) == TOKEN_IDENTIFIER && p->current + 1 < p->token_count &&
        p->tokens[p->current + 1].type == TOKEN_COLON) {
        label = strdup_custom(p->tokens[p->current].value);
        advance(p);
        advance(p);
    }

    if (!match(p, TOKEN_DO)) {
        errhandler__report_error(p->line, p->column, "syntax", "Expected 'do'");
        p->panic_mode = true;
        free(label);
        return NULL;
    }

    ASTNode* body = parse_block(p);
    if (!body) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected do-while body block");
        p->panic_mode = true;
        free(label);
        return NULL;
    }

    if (!match(p, TOKEN_WHILE)) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected 'while' after do body");
        p->panic_mode = true;
        free_ast_node(body);
        free(label);
        return NULL;
    }

    ASTNode* cond = parse_assignment_expr(p);
    if (!cond) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected do-while condition");
        p->panic_mode = true;
        free_ast_node(body);
        free(label);
        return NULL;
    }

    if (!match(p, TOKEN_SEMICOLON)) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected ';' after do-while condition");
        p->panic_mode = true;
        free_ast_node(cond);
        free_ast_node(body);
        free(label);
        return NULL;
    }

    DoWhileStmt* stmt = (DoWhileStmt*)make_node(p, NODE_DO_WHILE_STMT, sizeof(DoWhileStmt));
    if (!stmt) {
        free_ast_node(cond);
        free_ast_node(body);
        free(label);
        return NULL;
    }
    stmt->label = label;
    stmt->body = body;
    stmt->condition = cond;
    return (ASTNode*)stmt;
}

/*
 * Parses a for loop: [label:] for range | var [ : Type ] | { ... }.
 */
ASTNode* parse_for_stmt(Parser* p) {
    char* label = NULL;
    if (peek(p) == TOKEN_IDENTIFIER && p->current + 1 < p->token_count &&
        p->tokens[p->current + 1].type == TOKEN_COLON) {
        label = strdup_custom(p->tokens[p->current].value);
        advance(p);
        advance(p);
    }

    if (!match(p, TOKEN_FOR)) {
        errhandler__report_error(p->line, p->column, "syntax", "Expected 'for'");
        p->panic_mode = true;
        free(label);
        return NULL;
    }

    ASTNode* range = parse_range_expr(p);
    if (!range) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected range expression");
        p->panic_mode = true;
        free(label);
        return NULL;
    }

    if (!match(p, TOKEN_PIPE)) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected '|' after range in for");
        p->panic_mode = true;
        free_ast_node(range);
        free(label);
        return NULL;
    }

    if (!match(p, TOKEN_IDENTIFIER)) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected variable name in for");
        p->panic_mode = true;
        free_ast_node(range);
        free(label);
        return NULL;
    }
    char* var_name = strdup_custom(p->tokens[p->current - 1].value);

    ASTNode* var_type = NULL;
    if (match(p, TOKEN_COLON)) {
        var_type = parse_type_spec(p);
        if (!var_type) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected variable type in for");
            p->panic_mode = true;
        }
    }

    if (!match(p, TOKEN_PIPE)) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected '|' after variable in for");
        p->panic_mode = true;
        free(var_name);
        free_ast_node(var_type);
        free_ast_node(range);
        free(label);
        return NULL;
    }

    ASTNode* body = parse_block(p);
    if (!body) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected for body block");
        p->panic_mode = true;
        free(var_name);
        free_ast_node(var_type);
        free_ast_node(range);
        free(label);
        return NULL;
    }

    ForStmt* stmt = (ForStmt*)make_node(p, NODE_FOR_STMT, sizeof(ForStmt));
    if (!stmt) {
        free(var_name);
        free_ast_node(var_type);
        free_ast_node(range);
        free_ast_node(body);
        free(label);
        return NULL;
    }
    stmt->label = label;
    stmt->range = range;
    stmt->var_name = var_name;
    stmt->var_type = var_type;
    stmt->body = body;
    return (ASTNode*)stmt;
}

/*
 * Parses a break statement: break [label] ;.
 */
ASTNode* parse_break_stmt(Parser* p) {
    advance(p); // consume TOKEN_BREAK
    char* label = NULL;
    if (peek(p) == TOKEN_IDENTIFIER) {
        label = strdup_custom(p->tokens[p->current].value);
        advance(p);
    }
    if (!match(p, TOKEN_SEMICOLON)) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected ';' after break");
        p->panic_mode = true;
        free(label);
        return NULL;
    }
    BreakStmt* stmt = (BreakStmt*)make_node(p, NODE_BREAK_STMT, sizeof(BreakStmt));
    if (!stmt) {
        free(label);
        return NULL;
    }
    stmt->label = label;
    return (ASTNode*)stmt;
}

/*
 * Parses a continue statement: continue [label] ;.
 */
ASTNode* parse_continue_stmt(Parser* p) {
    advance(p); // consume TOKEN_CONTINUE
    char* label = NULL;
    if (peek(p) == TOKEN_IDENTIFIER) {
        label = strdup_custom(p->tokens[p->current].value);
        advance(p);
    }
    if (!match(p, TOKEN_SEMICOLON)) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected ';' after continue");
        p->panic_mode = true;
        free(label);
        return NULL;
    }
    ContinueStmt* stmt = (ContinueStmt*)make_node(p, NODE_CONTINUE_STMT, sizeof(ContinueStmt));
    if (!stmt) {
        free(label);
        return NULL;
    }
    stmt->label = label;
    return (ASTNode*)stmt;
}

/*
 * Parses a return statement: return [expr] ;.
 */
ASTNode* parse_return_stmt(Parser* p) {
    advance(p); // consume TOKEN_RETURN
    ASTNode* value = NULL;
    if (peek(p) != TOKEN_SEMICOLON) {
        value = parse_assignment_expr(p);
        if (!value) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected return expression");
            p->panic_mode = true;
            return NULL;
        }
    }
    if (!match(p, TOKEN_SEMICOLON)) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected ';' after return");
        p->panic_mode = true;
        free_ast_node(value);
        return NULL;
    }
    ReturnStmt* stmt = (ReturnStmt*)make_node(p, NODE_RETURN_STMT, sizeof(ReturnStmt));
    if (!stmt) {
        free_ast_node(value);
        return NULL;
    }
    stmt->value = value;
    return (ASTNode*)stmt;
}

/*
 * Parses a defer statement: defer ( block | statement ).
 */
ASTNode* parse_defer_stmt(Parser* p) {
    advance(p); // consume TOKEN_DEFER

    ASTNode* stmt = NULL;
    if (peek(p) == TOKEN_LBRACE) {
        stmt = parse_block(p);
    } else {
        stmt = parse_statement(p);
        if (!stmt) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected statement after defer");
            p->panic_mode = true;
        }
    }
    if (!stmt) {
        return NULL;
    }

    DeferStmt* d = (DeferStmt*)make_node(p, NODE_DEFER_STMT, sizeof(DeferStmt));
    if (!d) {
        free_ast_node(stmt);
        return NULL;
    }
    d->stmt = stmt;
    return (ASTNode*)d;
}

/*
 * Parses a receive statement: receive { case pattern [ | captures | ] { ... } ... [ else { ... } ] }.
 */
ASTNode* parse_receive_stmt(Parser* p) {
    advance(p); // consume TOKEN_RECEIVE

    if (!match(p, TOKEN_LBRACE)) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected '{' after receive");
        p->panic_mode = true;
        return NULL;
    }

    ReceiveStmt* stmt = (ReceiveStmt*)make_node(p, NODE_RECEIVE_STMT, sizeof(ReceiveStmt));
    if (!stmt) {
        return NULL;
    }
    stmt->cases = NULL;
    stmt->case_count = 0;
    stmt->else_case = NULL;

    while (peek(p) != TOKEN_RBRACE && peek(p) != TOKEN_EOF) {
        if (match(p, TOKEN_CASE)) {
            ASTNode* case_node = parse_receive_case(p);
            if (case_node) {
                append_child(p, (void***)&stmt->cases, &stmt->case_count, case_node);
            } else {
                p->panic_mode = true;
                synchronize_progress(p);
            }
        } else if (match(p, TOKEN_ELSE)) {
            if (stmt->else_case) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Multiple else clauses in receive");
                p->panic_mode = true;
            }
            ASTNode* body = parse_block(p);
            if (!body) {
                errhandler__report_error(p->line, p->column, "syntax",
                                         "Expected block for else");
                p->panic_mode = true;
            } else {
                stmt->else_case = body;
            }
        } else {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected 'case' or 'else' in receive");
            p->panic_mode = true;
            synchronize_progress(p);
        }
    }

    if (!match(p, TOKEN_RBRACE)) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected '}' to close receive");
        p->panic_mode = true;
    }
    return (ASTNode*)stmt;
}

/*
 * Parses a receive case: case pattern [ | captures | ] { ... }.
 */
ASTNode* parse_receive_case(Parser* p) {
    ASTNode* pattern = parse_pattern(p);
    if (!pattern) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected pattern in receive case");
        p->panic_mode = true;
        return NULL;
    }

    char** captures = NULL;
    size_t capture_count = 0;
    if (match(p, TOKEN_PIPE)) {
        while (peek(p) == TOKEN_IDENTIFIER || peek(p) == TOKEN_NONE) {
            char* cap = NULL;
            if (match(p, TOKEN_NONE)) {
                cap = strdup_custom("none");
            } else if (match(p, TOKEN_IDENTIFIER)) {
                cap = strdup_custom(p->tokens[p->current - 1].value);
            } else {
                break;
            }
            append_child(p, (void***)&captures, &capture_count, cap);
            if (!match(p, TOKEN_COMMA)) {
                break;
            }
        }
        if (!match(p, TOKEN_PIPE)) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected '|' to close capture list");
            p->panic_mode = true;
            for (size_t i = 0; i < capture_count; ++i) free(captures[i]);
            free(captures);
            free_ast_node(pattern);
            return NULL;
        }
    }

    ASTNode* body = parse_block(p);
    if (!body) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected block after receive case");
        p->panic_mode = true;
        free_ast_node(pattern);
        for (size_t i = 0; i < capture_count; ++i) free(captures[i]);
        free(captures);
        return NULL;
    }

    ReceiveCase* rc = (ReceiveCase*)make_node(p, NODE_RECEIVE_CASE, sizeof(ReceiveCase));
    if (!rc) {
        free_ast_node(pattern);
        free_ast_node(body);
        for (size_t i = 0; i < capture_count; ++i) free(captures[i]);
        free(captures);
        return NULL;
    }
    rc->pattern = pattern;
    rc->captures = captures;
    rc->capture_count = capture_count;
    rc->body = body;
    return (ASTNode*)rc;
}

/*
 * Parses an asm statement: asm string ;.
 */
ASTNode* parse_asm_stmt(Parser* p) {
    advance(p); // consume TOKEN_ASM

    if (peek(p) != TOKEN_STRING_LITERAL) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected string literal for asm");
        p->panic_mode = true;
        return NULL;
    }
    char* code_str = strdup_custom(p->tokens[p->current].value);
    advance(p);

    LiteralExpr* lit = (LiteralExpr*)make_node(p, NODE_LITERAL, sizeof(LiteralExpr));
    if (!lit) {
        free(code_str);
        return NULL;
    }
    lit->kind = LIT_STRING;
    lit->string_val = code_str;
    lit->raw_value = NULL;

    if (!match(p, TOKEN_SEMICOLON)) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected ';' after asm");
        p->panic_mode = true;
        free_ast_node((ASTNode*)lit);
        return NULL;
    }

    AsmStmt* stmt = (AsmStmt*)make_node(p, NODE_ASM_STMT, sizeof(AsmStmt));
    if (!stmt) {
        free_ast_node((ASTNode*)lit);
        return NULL;
    }
    stmt->code = (ASTNode*)lit;
    return (ASTNode*)stmt;
}

/*
 * Parses a volatile statement: volatile ( block | statement ).
 */
ASTNode* parse_volatile_stmt(Parser* p) {
    advance(p); // consume TOKEN_VOLATILE

    ASTNode* stmt = NULL;
    if (peek(p) == TOKEN_LBRACE) {
        stmt = parse_block(p);
    } else {
        stmt = parse_statement(p);
        if (!stmt) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected statement after volatile");
            p->panic_mode = true;
        }
    }
    if (!stmt) {
        return NULL;
    }

    VolatileStmt* v = (VolatileStmt*)make_node(p, NODE_VOLATILE_STMT, sizeof(VolatileStmt));
    if (!v) {
        free_ast_node(stmt);
        return NULL;
    }
    v->stmt = stmt;
    return (ASTNode*)v;
}

/*
 * Parses a comptime statement: comptime [name] ( block | statement ).
 */
ASTNode* parse_comptime_stmt(Parser* p) {
    advance(p); // consume TOKEN_COMPTIME

    char* name = NULL;
    if (peek(p) == TOKEN_IDENTIFIER) {
        name = strdup_custom(p->tokens[p->current].value);
        advance(p);
    }

    ASTNode* stmt = NULL;
    if (peek(p) == TOKEN_LBRACE) {
        stmt = parse_block(p);
    } else {
        stmt = parse_statement(p);
        if (!stmt) {
            errhandler__report_error(p->line, p->column, "syntax",
                                     "Expected statement after comptime");
            p->panic_mode = true;
        }
    }
    if (!stmt) {
        free(name);
        return NULL;
    }

    ComptimeStmt* cs = (ComptimeStmt*)make_node(p, NODE_COMPTIME_STMT, sizeof(ComptimeStmt));
    if (!cs) {
        free(name);
        free_ast_node(stmt);
        return NULL;
    }
    cs->name = name;
    cs->stmt = stmt;
    return (ASTNode*)cs;
}

/*
 * Parses a pass statement: pass ;.
 */
ASTNode* parse_pass_stmt(Parser* p) {
    advance(p); // consume TOKEN_PASS

    if (!match(p, TOKEN_SEMICOLON)) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected ';' after pass");
        p->panic_mode = true;
        return NULL;
    }
    PassStmt* stmt = (PassStmt*)make_node(p, NODE_PASS_STMT, sizeof(PassStmt));
    return (ASTNode*)stmt;
}

/*
 * Parses a free statement: free expression ;.
 */
ASTNode* parse_free_stmt(Parser* p) {
    advance(p); // consume TOKEN_FREE

    ASTNode* expr = parse_assignment_expr(p);
    if (!expr) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected expression after free");
        p->panic_mode = true;
        return NULL;
    }

    if (!match(p, TOKEN_SEMICOLON)) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected ';' after free expression");
        p->panic_mode = true;
        free_ast_node(expr);
        return NULL;
    }

    FreeStmt* stmt = (FreeStmt*)make_node(p, NODE_FREE_STMT, sizeof(FreeStmt));
    if (!stmt) {
        free_ast_node(expr);
        return NULL;
    }
    stmt->expr = expr;
    return (ASTNode*)stmt;
}

/*
 * Parses an expression statement: expression ;.
 */
ASTNode* parse_expr_stmt(Parser* p) {
    ASTNode* expr = parse_assignment_expr(p);
    if (!expr) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected expression");
        p->panic_mode = true;
        return NULL;
    }
    if (!match(p, TOKEN_SEMICOLON)) {
        errhandler__report_error(p->line, p->column, "syntax",
                                 "Expected ';' after expression");
        p->panic_mode = true;
        free_ast_node(expr);
        return NULL;
    }
    ExprStmt* stmt = (ExprStmt*)make_node(p, NODE_EXPR_STMT, sizeof(ExprStmt));
    if (!stmt) {
        free_ast_node(expr);
        return NULL;
    }
    stmt->expr = expr;
    return (ASTNode*)stmt;
}
