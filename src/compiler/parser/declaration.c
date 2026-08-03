#include "internal.h"
#include <stdlib.h>
#include <string.h>

/*
 * Parses a module declaration: module [Name] { ... }.
 * If name is omitted, the module is anonymous.
 */
ASTNode* parse_module_decl(Parser* p) {
    advance(p); // consume TOKEN_MODULE

    char* name = NULL;
    if (match(p, TOKEN_IDENTIFIER)) {
        name = strdup_custom(p->tokens[p->current - 1].value);
    }

    if (!match(p, TOKEN_LBRACE)) {
        errhandler__report_error(p->line, p->column, "parser",
                                 "Expected '{' after module name or after 'module'");
        p->panic_mode = true;
        free(name);
        return NULL;
    }

    ModuleDecl* mod = (ModuleDecl*)make_node(p, NODE_MODULE, sizeof(ModuleDecl));
    if (!mod) {
        free(name);
        return NULL;
    }
    mod->name = name;
    mod->body = NULL;
    mod->body_count = 0;

    while (peek(p) != TOKEN_RBRACE && peek(p) != TOKEN_EOF) {
        if (p->panic_mode) {
            synchronize(p);
            p->panic_mode = false;
            if (peek(p) == TOKEN_EOF) {
                break;
            }
        }
        ASTNode* item = parse_top_level_item(p);
        if (item) {
            append_child(p, (void***)&mod->body, &mod->body_count, item);
        } else {
            if (peek(p) != TOKEN_RBRACE && peek(p) != TOKEN_EOF) {
                synchronize(p);
                p->panic_mode = false;
            }
        }
    }

    if (!match(p, TOKEN_RBRACE)) {
        errhandler__report_error(p->line, p->column, "parser",
                                 "Expected '}' to close module");
        p->panic_mode = true;
    }
    return (ASTNode*)mod;
}

/*
 * Parses a use declaration: use Path::[ * | { id, ... } ] ;.
 * If no import specifier is present, imports all symbols.
 */
ASTNode* parse_use_decl(Parser* p) {
    advance(p); // consume TOKEN_USE

    UseDecl* use = (UseDecl*)make_node(p, NODE_USE, sizeof(UseDecl));
    if (!use) {
        return NULL;
    }
    use->path = NULL;
    use->path_len = 0;
    use->imports = NULL;
    use->import_count = 0;

    if (!match(p, TOKEN_IDENTIFIER)) {
        errhandler__report_error(p->line, p->column, "parser",
                                 "Expected identifier in use path");
        p->panic_mode = true;
        return (ASTNode*)use;
    }
    do {
        char* part = strdup_custom(p->tokens[p->current - 1].value);
        append_child(p, (void***)&use->path, &use->path_len, part);
    } while (match(p, TOKEN_COLON_COLON) && match(p, TOKEN_IDENTIFIER));

    if (match(p, TOKEN_COLON_COLON)) {
        if (match(p, TOKEN_STAR)) {
            // import all – nothing to store
        } else if (match(p, TOKEN_LBRACE)) {
            while (peek(p) != TOKEN_RBRACE && peek(p) != TOKEN_EOF) {
                if (!match(p, TOKEN_IDENTIFIER)) {
                    errhandler__report_error(p->line, p->column, "parser",
                                             "Expected identifier in import list");
                    break;
                }
                char* imp = strdup_custom(p->tokens[p->current - 1].value);
                append_child(p, (void***)&use->imports, &use->import_count, imp);
                if (!match(p, TOKEN_COMMA)) {
                    break;
                }
            }
            if (!match(p, TOKEN_RBRACE)) {
                errhandler__report_error(p->line, p->column, "parser",
                                         "Expected '}' to close import list");
                p->panic_mode = true;
            }
        } else {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected '*' or '{' after '::'");
            p->panic_mode = true;
        }
    }

    if (!match(p, TOKEN_SEMICOLON)) {
        errhandler__report_error(p->line, p->column, "parser",
                                 "Expected ';' after use declaration");
        p->panic_mode = true;
    }
    return (ASTNode*)use;
}

/*
 * Parses a function declaration: [comptime] func Name ( params ) : Type ( { ... } | = expr ; ).
 * If is_method is 1, the function is a method inside a struct.
 */
ASTNode* parse_func_decl(Parser* p, uint8_t is_method) {
    uint8_t is_comptime = 0;
    if (peek(p) == TOKEN_COMPTIME) {
        advance(p);
        is_comptime = 1;
    }

    if (!match(p, TOKEN_FUNC)) {
        errhandler__report_error(p->line, p->column, "parser", "Expected 'func'");
        return NULL;
    }

    char* name = NULL;
    ASTNode** params = NULL;
    size_t param_count = 0;
    ASTNode* ret_type = NULL;
    uint32_t mods = 0;

    if (!parse_function_signature(p, &name, &params, &param_count, &ret_type, &mods, true)) {
        free(name);
        return NULL;
    }

    if (is_method && (mods & (MOD_EXTERN | MOD_VOLATILE))) {
        errhandler__report_error(p->line, p->column, "parser",
                                 "Methods cannot have 'extern' or 'volatile' modifiers");
    }

    ASTNode* body = NULL;
    ASTNode* expr_body = NULL;

    if (peek(p) == TOKEN_LBRACE) {
        body = parse_block(p);
        if (!body) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected function body block");
            return NULL;
        }
    } else if (match(p, TOKEN_EQUAL)) {
        expr_body = parse_assignment_expr(p);
        if (!expr_body) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected expression after '='");
            return NULL;
        }
        if (!match(p, TOKEN_SEMICOLON)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected ';' after expression body");
            return NULL;
        }
    } else {
        errhandler__report_error(p->line, p->column, "parser",
                                 "Expected function body or '=' expression");
        return NULL;
    }

    FuncDecl* func = (FuncDecl*)make_node(p, NODE_FUNC_DECL, sizeof(FuncDecl));
    if (!func) {
        free(name);
        return NULL;
    }
    func->name = name;
    func->modifiers = mods;
    func->is_comptime = is_comptime;
    func->is_method = is_method;
    func->params = params;
    func->param_count = param_count;
    func->return_type = ret_type;
    func->body = body;
    func->expr_body = expr_body;
    return (ASTNode*)func;
}

/*
 * Parses a let declaration: let ( name : Type, ... ) = expr ; or
 * let name [ size ] : Type = expr ;.
 */
ASTNode* parse_let_decl(Parser* p) {
    advance(p); // consume TOKEN_LET

    LetDecl* let = (LetDecl*)make_node(p, NODE_LET_DECL, sizeof(LetDecl));
    if (!let) {
        return NULL;
    }
    let->names = NULL;
    let->types = NULL;
    let->count = 0;
    let->initializer = NULL;
    let->modifiers = 0;
    let->array_size = NULL;

    if (match(p, TOKEN_LPAREN)) {
        // Tuple pattern: ( name : Type, ... )
        while (peek(p) != TOKEN_RPAREN && peek(p) != TOKEN_EOF) {
            char* name = NULL;
            ASTNode* type = NULL;
            uint32_t mods = 0;
            if (!parse_name_type(p, &name, &type, &mods)) {
                errhandler__report_error(p->line, p->column, "parser",
                                         "Invalid variable declaration in tuple");
                break;
            }
            append_child(p, (void***)&let->names, &let->count, name);
            append_child(p, (void***)&let->types, &let->count, type);
            // modifiers are ignored for tuple variables
            if (!match(p, TOKEN_COMMA)) {
                break;
            }
        }
        if (!match(p, TOKEN_RPAREN)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected ')' to close tuple pattern");
            p->panic_mode = true;
        }
    } else {
        // Single variable with optional array size.
        if (!match(p, TOKEN_IDENTIFIER)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected variable name");
            p->panic_mode = true;
            return (ASTNode*)let;
        }
        char* name = strdup_custom(p->tokens[p->current - 1].value);
        append_child(p, (void***)&let->names, &let->count, name);

        // Check for array size: [ expr ] or [ ]
        if (match(p, TOKEN_LBRACKET)) {
            if (peek(p) != TOKEN_RBRACKET) {
                let->array_size = parse_assignment_expr(p);
                if (!let->array_size) {
                    errhandler__report_error(p->line, p->column, "parser",
                                             "Expected array size expression");
                    p->panic_mode = true;
                }
            }
            if (!match(p, TOKEN_RBRACKET)) {
                errhandler__report_error(p->line, p->column, "parser",
                                         "Expected ']' after array size");
                p->panic_mode = true;
            }
        }

        // Optional type after ':'
        ASTNode* type = NULL;
        if (match(p, TOKEN_COLON)) {
            type = parse_type_spec(p);
            if (!type) {
                errhandler__report_error(p->line, p->column, "parser",
                                         "Expected type after ':'");
            }
        }
        append_child(p, (void***)&let->types, &let->count, type);
    }

    if (match(p, TOKEN_EQUAL)) {
        let->initializer = parse_assignment_expr(p);
        if (!let->initializer) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected expression after '='");
            p->panic_mode = true;
        }
    }

    if (!match(p, TOKEN_SEMICOLON)) {
        errhandler__report_error(p->line, p->column, "parser",
                                 "Expected ';' after let declaration");
        p->panic_mode = true;
    }
    return (ASTNode*)let;
}

/*
 * Parses a comptime declaration: comptime ( func ... | Name = ... ).
 * Also handles comptime functions via the func branch.
 */
ASTNode* parse_comptime_decl(Parser* p) {
    advance(p); // consume TOKEN_COMPTIME

    // Check for comptime function
    if (peek(p) == TOKEN_FUNC) {
        advance(p); // consume 'func'
        char* name = NULL;
        ASTNode** params = NULL;
        size_t param_count = 0;
        ASTNode* ret_type = NULL;
        uint32_t mods = 0;
        if (!parse_function_signature(p, &name, &params, &param_count, &ret_type, &mods, false)) {
            free(name);
            return NULL;
        }
        if (mods & (MOD_EXTERN | MOD_VOLATILE)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "comptime functions cannot have 'extern' or 'volatile'");
        }

        ASTNode* body = NULL;
        ASTNode* expr_body = NULL;
        if (peek(p) == TOKEN_LBRACE) {
            body = parse_block(p);
            if (!body) {
                errhandler__report_error(p->line, p->column, "parser",
                                         "Expected function body");
                return NULL;
            }
        } else if (match(p, TOKEN_EQUAL)) {
            expr_body = parse_assignment_expr(p);
            if (!expr_body) {
                errhandler__report_error(p->line, p->column, "parser",
                                         "Expected expression after '='");
                return NULL;
            }
            if (!match(p, TOKEN_SEMICOLON)) {
                errhandler__report_error(p->line, p->column, "parser",
                                         "Expected ';' after expression body");
                return NULL;
            }
        } else {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected function body or '=' expression");
            return NULL;
        }

        FuncDecl* func = (FuncDecl*)make_node(p, NODE_FUNC_DECL, sizeof(FuncDecl));
        if (!func) {
            free(name);
            return NULL;
        }
        func->name = name;
        func->modifiers = mods;
        func->is_comptime = 1;
        func->is_method = 0;
        func->params = params;
        func->param_count = param_count;
        func->return_type = ret_type;
        func->body = body;
        func->expr_body = expr_body;
        return (ASTNode*)func;
    }

    // Otherwise: comptime Name = ...
    if (!match(p, TOKEN_IDENTIFIER)) {
        errhandler__report_error(p->line, p->column, "parser",
                                 "Expected comptime name");
        p->panic_mode = true;
        return NULL;
    }
    char* name = strdup_custom(p->tokens[p->current - 1].value);

    ComptimeDecl* decl = (ComptimeDecl*)make_node(p, NODE_COMPTIME_DECL, sizeof(ComptimeDecl));
    if (!decl) {
        free(name);
        return NULL;
    }
    decl->name = name;
    decl->type_def = NULL;
    decl->value = NULL;

    if (!match(p, TOKEN_EQUAL)) {
        errhandler__report_error(p->line, p->column, "parser",
                                 "Expected '=' in comptime declaration");
        p->panic_mode = true;
        return (ASTNode*)decl;
    }

    if (peek(p) == TOKEN_STRUCT || peek(p) == TOKEN_UNION || peek(p) == TOKEN_ENUM) {
        decl->type_def = parse_type_spec(p);
        if (!decl->type_def) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected type definition");
        }
    } else {
        decl->value = parse_assignment_expr(p);
        if (!decl->value) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected expression after '='");
        }
    }

    if (!match(p, TOKEN_SEMICOLON)) {
        errhandler__report_error(p->line, p->column, "parser",
                                 "Expected ';' after comptime declaration");
        p->panic_mode = true;
    }
    return (ASTNode*)decl;
}

/*
 * Parses an extern declaration: extern ( func ... | let ... ) ;
 */
ASTNode* parse_extern_decl(Parser* p) {
    advance(p); // consume TOKEN_EXTERN

    if (peek(p) == TOKEN_FUNC) {
        advance(p);
        if (!match(p, TOKEN_IDENTIFIER)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected function name");
            p->panic_mode = true;
            return NULL;
        }
        char* name = strdup_custom(p->tokens[p->current - 1].value);

        ExternDecl* ext = (ExternDecl*)make_node(p, NODE_EXTERN_DECL, sizeof(ExternDecl));
        if (!ext) {
            free(name);
            return NULL;
        }
        ext->name = name;
        ext->is_func = 1;
        ext->type = NULL;
        ext->params = NULL;
        ext->param_count = 0;

        if (!match(p, TOKEN_LPAREN)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected '(' for extern func parameters");
            p->panic_mode = true;
            return (ASTNode*)ext;
        }
        if (peek(p) != TOKEN_RPAREN) {
            do {
                char* pname = NULL;
                ASTNode* ptype = NULL;
                uint32_t mods = 0;
                if (!parse_name_type(p, &pname, &ptype, &mods)) {
                    errhandler__report_error(p->line, p->column, "parser",
                                             "Invalid parameter in extern");
                    break;
                }
                Param* param = (Param*)make_node(p, NODE_PARAM, sizeof(Param));
                if (!param) {
                    free(pname);
                    free_ast_node(ptype);
                    break;
                }
                param->name = pname;
                param->type = ptype;
                param->modifiers = mods;
                param->is_volatile = (mods & MOD_VOLATILE) ? 1 : 0;
                append_child(p, (void***)&ext->params, &ext->param_count, param);
            } while (match(p, TOKEN_COMMA) && peek(p) != TOKEN_RPAREN);
        }
        if (!match(p, TOKEN_RPAREN)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected ')' after parameters");
            p->panic_mode = true;
        }

        if (!match(p, TOKEN_COLON)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected ':' for extern func return type");
            p->panic_mode = true;
        }
        ext->type = parse_type_spec(p);
        if (!ext->type) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected return type");
            p->panic_mode = true;
        }

        if (!match(p, TOKEN_SEMICOLON)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected ';' after extern func");
            p->panic_mode = true;
        }
        return (ASTNode*)ext;
    } else if (peek(p) == TOKEN_LET) {
        advance(p);
        if (!match(p, TOKEN_IDENTIFIER)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected variable name");
            p->panic_mode = true;
            return NULL;
        }
        char* name = strdup_custom(p->tokens[p->current - 1].value);

        ExternDecl* ext = (ExternDecl*)make_node(p, NODE_EXTERN_DECL, sizeof(ExternDecl));
        if (!ext) {
            free(name);
            return NULL;
        }
        ext->name = name;
        ext->is_func = 0;
        ext->type = NULL;
        ext->params = NULL;
        ext->param_count = 0;

        if (!match(p, TOKEN_COLON)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected ':' for extern let type");
            p->panic_mode = true;
            return (ASTNode*)ext;
        }
        ext->type = parse_type_spec(p);
        if (!ext->type) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected type");
            p->panic_mode = true;
        }

        if (!match(p, TOKEN_SEMICOLON)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected ';' after extern let");
            p->panic_mode = true;
        }
        return (ASTNode*)ext;
    } else {
        errhandler__report_error(p->line, p->column, "parser",
                                 "Expected 'func' or 'let' after 'extern'");
        p->panic_mode = true;
        return NULL;
    }
}

/*
 * Parses a process declaration: proc Name { ... }.
 */
ASTNode* parse_proc_decl(Parser* p) {
    advance(p); // consume TOKEN_PROC

    if (!match(p, TOKEN_IDENTIFIER)) {
        errhandler__report_error(p->line, p->column, "parser",
                                 "Expected proc name");
        p->panic_mode = true;
        return NULL;
    }
    char* name = strdup_custom(p->tokens[p->current - 1].value);

    ProcDecl* proc = (ProcDecl*)make_node(p, NODE_PROC_DECL, sizeof(ProcDecl));
    if (!proc) {
        free(name);
        return NULL;
    }
    proc->name = name;
    proc->body = parse_block(p);
    if (!proc->body) {
        errhandler__report_error(p->line, p->column, "parser",
                                 "Expected proc body block");
        p->panic_mode = true;
    }
    return (ASTNode*)proc;
}
