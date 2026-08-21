#include "internal.h"
#include <stdlib.h>
#include <string.h>

/*
* Parses a module declaration.
* Consumes the 'module' keyword, then optionally reads an identifier for the module name.
* Expects an opening brace '{'.
* Parses top-level items inside the module body until a closing brace '}' or EOF is reached.
* Returns a ModuleDecl AST node.
*/
ASTNode* parse_module_decl(Parser* p) {
   advance(p);
   char* name = NULL;
   if (match(p, TOKEN_IDENTIFIER)) {
       name = strdup_custom(p->tokens[p->current - 1].value);
   }
   if (!match(p, TOKEN_LBRACE)) {
       errhandler__report_error(p->line, p->column, "syntax",
                                "Expected '{' after module name or after 'module'");
       p->panic_mode = true;
       free(name);
       return NULL;
   }
   ModuleDecl* mod = (ModuleDecl*)make_node(p, NODE_MODULE, sizeof(ModuleDecl));
   if (!mod) { free(name); return NULL; }
   mod->name = name;
   mod->body = NULL;
   mod->body_count = 0;
   while (peek(p) != TOKEN_RBRACE && peek(p) != TOKEN_EOF) {
       if (p->panic_mode) {
           synchronize_progress(p);
           p->panic_mode = false;
           if (peek(p) == TOKEN_EOF) break;
       }
       ASTNode* item = parse_top_level_item(p);
       if (item) {
           append_child(p, (void***)&mod->body, &mod->body_count, item);
       } else {
           if (peek(p) != TOKEN_RBRACE && peek(p) != TOKEN_EOF) {
               synchronize_progress(p);
               p->panic_mode = false;
           }
       }
   }
   if (!match(p, TOKEN_RBRACE)) {
       errhandler__report_error(p->line, p->column, "syntax",
                                "Expected '}' to close module");
       p->panic_mode = true;
   }
   return (ASTNode*)mod;
}

/*
* Parses a use declaration.
* Consumes the 'use' keyword.
* Reads the first identifier as the start of the module path.
* While '::' is found, appends subsequent identifiers to the path.
* Handles wildcard imports ('*') and braced import lists ('{ a, b }').
* Expects a semicolon at the end.
* Returns a UseDecl AST node.
*/
ASTNode* parse_use_decl(Parser* p) {
   advance(p);
   UseDecl* use = (UseDecl*)make_node(p, NODE_USE, sizeof(UseDecl));
   if (!use) return NULL;
   use->path = NULL; use->path_len = 0;
   use->imports = NULL; use->import_count = 0;
   if (!match(p, TOKEN_IDENTIFIER)) {
       errhandler__report_error(p->line, p->column, "parser",
                                "Expected identifier in use path");
       p->panic_mode = true;
       return (ASTNode*)use;
   }
   char* part = strdup_custom(p->tokens[p->current - 1].value);
   append_child(p, (void***)&use->path, &use->path_len, part);
   while (1) {
       if (!match(p, TOKEN_COLON_COLON)) break;
       if (peek(p) == TOKEN_STAR) { advance(p); break; }
       if (peek(p) == TOKEN_LBRACE) {
           advance(p);
           while (peek(p) != TOKEN_RBRACE && peek(p) != TOKEN_EOF) {
               if (!match(p, TOKEN_IDENTIFIER)) {
                   errhandler__report_error(p->line, p->column, "parser",
                                            "Expected identifier in import list");
                   break;
               }
               char* imp = strdup_custom(p->tokens[p->current - 1].value);
               append_child(p, (void***)&use->imports, &use->import_count, imp);
               if (!match(p, TOKEN_COMMA)) break;
           }
           if (!match(p, TOKEN_RBRACE)) {
               errhandler__report_error(p->line, p->column, "parser",
                                        "Expected '}' to close import list");
               p->panic_mode = true;
           }
           break;
       }
       if (!match(p, TOKEN_IDENTIFIER)) {
           errhandler__report_error(p->line, p->column, "parser",
                                    "Expected identifier in use path");
           p->panic_mode = true;
           return (ASTNode*)use;
       }
       char* part2 = strdup_custom(p->tokens[p->current - 1].value);
       append_child(p, (void***)&use->path, &use->path_len, part2);
   }
   if (!match(p, TOKEN_SEMICOLON)) {
       errhandler__report_error(p->line, p->column, "parser",
                                "Expected ';' after use declaration");
       p->panic_mode = true;
   }
   return (ASTNode*)use;
}

/*
* Parses a function declaration.
* Optionally consumes a 'comptime' keyword if present before 'func'.
* Consumes the 'func' keyword.
* Parses the function signature: name, parameters, return type.
* Then parses the function body, which can be:
*   - a block enclosed in braces,
*   - a single expression preceded by '=' and terminated with ';',
*   - or a prototype terminated with ';'.
* Returns a FuncDecl AST node.
*/
ASTNode* parse_func_decl(Parser* p, uint8_t is_method) {
   uint8_t is_comptime = 0;
   if (peek(p) == TOKEN_COMPTIME) {
       advance(p);
       is_comptime = 1;
   }
   if (!match(p, TOKEN_FUNC)) {
       errhandler__report_error(p->line, p->column, "syntax", "Expected 'func'");
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
       errhandler__report_error(p->line, p->column, "syntax",
                                "Methods cannot have 'extern' or 'volatile' modifiers");
   }
   ASTNode* body = NULL;
   ASTNode* expr_body = NULL;
   if (peek(p) == TOKEN_LBRACE) {
       body = parse_block(p);
       if (!body) {
           errhandler__report_error(p->line, p->column, "syntax",
                                    "Expected function body block");
           return NULL;
       }
   } else if (match(p, TOKEN_EQUAL)) {
       expr_body = parse_assignment_expr(p);
       if (!expr_body) {
           errhandler__report_error(p->line, p->column, "syntax",
                                    "Expected expression after '='");
           return NULL;
       }
       if (!match(p, TOKEN_SEMICOLON)) {
           errhandler__report_error(p->line, p->column, "syntax",
                                    "Expected ';' after expression body");
           return NULL;
       }
   } else if (match(p, TOKEN_SEMICOLON)) {
   } else {
       errhandler__report_error(p->line, p->column, "syntax",
                                "Expected function body, '=' expression, or ';' prototype");
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
* Parses a let declaration.
* Consumes the 'let' keyword.
* Handles single variable declarations and tuple declarations in parentheses.
* For each variable, parses an optional array size in brackets, an optional type annotation after ':',
* and an optional initializer after '='.
* Expects a semicolon at the end.
* Returns a LetDecl AST node.
*/
ASTNode* parse_let_decl(Parser* p) {
   advance(p);
   LetDecl* let = (LetDecl*)make_node(p, NODE_LET_DECL, sizeof(LetDecl));
   if (!let) return NULL;
   let->names = NULL;
   let->types = NULL;
   let->count = 0;
   let->initializer = NULL;
   let->modifiers = 0;
   let->array_size = NULL;
   let->inits = NULL;
   let->init_count = 0;
   if (match(p, TOKEN_LPAREN)) {
       while (peek(p) != TOKEN_RPAREN && peek(p) != TOKEN_EOF) {
           char* name = NULL;
           ASTNode* type = NULL;
           uint32_t mods = 0;
           if (!parse_name_type(p, &name, &type, &mods)) {
               errhandler__report_error(p->line, p->column, "syntax",
                                        "Invalid variable declaration in tuple");
               break;
           }
           size_t name_count = let->count;
           size_t type_count = let->count;
           append_child(p, (void***)&let->names, &name_count, name);
           append_child(p, (void***)&let->types, &type_count, type);
           let->count = name_count < type_count ? name_count : type_count;
           ASTNode* init = NULL;
           if (match(p, TOKEN_EQUAL)) {
               init = parse_assignment_expr(p);
               if (!init) {
                   errhandler__report_error(p->line, p->column, "syntax",
                                            "Expected expression after '=' in tuple let");
                   p->panic_mode = true;
               }
           }
           append_child(p, (void***)&let->inits, &let->init_count, init);
           if (!match(p, TOKEN_COMMA)) break;
       }
       if (!match(p, TOKEN_RPAREN)) {
           errhandler__report_error(p->line, p->column, "syntax",
                                    "Expected ')' to close tuple pattern");
           p->panic_mode = true;
       }
   } else {
       if (!match(p, TOKEN_IDENTIFIER)) {
           errhandler__report_error(p->line, p->column, "syntax",
                                    "Expected variable name");
           p->panic_mode = true;
           return (ASTNode*)let;
       }
       char* name = strdup_custom(p->tokens[p->current - 1].value);
       size_t name_count = 0;
       append_child(p, (void***)&let->names, &name_count, name);
       let->count = name_count;
       if (match(p, TOKEN_LBRACKET)) {
           if (peek(p) != TOKEN_RBRACKET) {
               let->array_size = parse_assignment_expr(p);
               if (!let->array_size) {
                   errhandler__report_error(p->line, p->column, "syntax",
                                            "Expected array size expression");
                   p->panic_mode = true;
               }
           }
           if (!match(p, TOKEN_RBRACKET)) {
               errhandler__report_error(p->line, p->column, "syntax",
                                        "Expected ']' after array size");
               p->panic_mode = true;
           }
       }
       ASTNode* type = NULL;
       if (match(p, TOKEN_COLON)) {
           type = parse_type_spec(p);
           if (!type) {
               errhandler__report_error(p->line, p->column, "syntax",
                                        "Expected type after ':'");
           }
       }
       size_t type_count = 0;
       append_child(p, (void***)&let->types, &type_count, type);
   }
   if (match(p, TOKEN_EQUAL)) {
       let->initializer = parse_assignment_expr(p);
       if (!let->initializer) {
           errhandler__report_error(p->line, p->column, "syntax",
                                    "Expected expression after '='");
           p->panic_mode = true;
       }
   }
   if (!match(p, TOKEN_SEMICOLON)) {
       errhandler__report_error(p->line, p->column, "syntax",
                                "Expected ';' after let declaration");
       p->panic_mode = true;
   }
   return (ASTNode*)let;
}

/*
* Parses a comptime declaration.
* Consumes the 'comptime' keyword.
* If the next token is 'func', parses a compile-time function:
*   reads the signature, then expects a body block, a '=' expression, or a ';' prototype.
* Otherwise, reads an identifier as the declaration name.
* Expects '=' after the name.
* If the right-hand side is 'Struct', 'Union', or 'Enum', parses it as a type definition.
* Otherwise, parses the right-hand side as a value expression.
* Expects a semicolon at the end.
* Returns a FuncDecl or ComptimeDecl AST node.
*/
ASTNode* parse_comptime_decl(Parser* p) {
   advance(p);
   if (peek(p) == TOKEN_FUNC) {
       advance(p);
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
           errhandler__report_error(p->line, p->column, "syntax",
                                    "comptime functions cannot have 'extern' or 'volatile'");
       }
       ASTNode* body = NULL;
       ASTNode* expr_body = NULL;
       if (peek(p) == TOKEN_LBRACE) {
           body = parse_block(p);
           if (!body) {
               errhandler__report_error(p->line, p->column, "syntax",
                                        "Expected function body");
               return NULL;
           }
       } else if (match(p, TOKEN_EQUAL)) {
           expr_body = parse_assignment_expr(p);
           if (!expr_body) {
               errhandler__report_error(p->line, p->column, "syntax",
                                        "Expected expression after '='");
               return NULL;
           }
           if (!match(p, TOKEN_SEMICOLON)) {
               errhandler__report_error(p->line, p->column, "syntax",
                                        "Expected ';' after expression body");
               return NULL;
           }
       } else if (match(p, TOKEN_SEMICOLON)) {
       } else {
           errhandler__report_error(p->line, p->column, "syntax",
                                    "Expected function body, '=' expression, or ';' prototype");
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
   if (!match(p, TOKEN_IDENTIFIER)) {
       errhandler__report_error(p->line, p->column, "syntax",
                                "Expected comptime name");
       p->panic_mode = true;
       return NULL;
   }
   char* name = strdup_custom(p->tokens[p->current - 1].value);
   ComptimeDecl* decl = (ComptimeDecl*)make_node(p, NODE_COMPTIME_DECL, sizeof(ComptimeDecl));
   if (!decl) { free(name); return NULL; }
   decl->name = name;
   decl->type_def = NULL;
   decl->value = NULL;
   if (!match(p, TOKEN_EQUAL)) {
       errhandler__report_error(p->line, p->column, "syntax",
                                "Expected '=' in comptime declaration");
       p->panic_mode = true;
       return (ASTNode*)decl;
   }
   if (peek(p) == TOKEN_STRUCT || peek(p) == TOKEN_UNION || peek(p) == TOKEN_ENUM) {
       decl->type_def = parse_type_spec(p);
       if (!decl->type_def) {
           errhandler__report_error(p->line, p->column, "syntax",
                                    "Expected type definition");
       }
   } else {
       decl->value = parse_assignment_expr(p);
       if (!decl->value) {
           errhandler__report_error(p->line, p->column, "syntax",
                                    "Expected expression after '='");
       }
   }
   if (!match(p, TOKEN_SEMICOLON)) {
       errhandler__report_error(p->line, p->column, "syntax",
                                "Expected ';' after comptime declaration");
       p->panic_mode = true;
   }
   return (ASTNode*)decl;
}

/*
* Parses an extern declaration.
* Consumes the 'extern' keyword.
* If the next token is 'func', parses an external function prototype:
*   reads the name, parameter list in parentheses, colon, and return type.
* If the next token is 'let', parses an external variable declaration:
*   reads the name, colon, and type.
* Expects a semicolon at the end.
* Returns an ExternDecl AST node.
*/
ASTNode* parse_extern_decl(Parser* p) {
   advance(p);
   if (peek(p) == TOKEN_FUNC) {
       advance(p);
       if (!match(p, TOKEN_IDENTIFIER)) {
           errhandler__report_error(p->line, p->column, "syntax",
                                    "Expected function name");
           p->panic_mode = true;
           return NULL;
       }
       char* name = strdup_custom(p->tokens[p->current - 1].value);
       ExternDecl* ext = (ExternDecl*)make_node(p, NODE_EXTERN_DECL, sizeof(ExternDecl));
       if (!ext) { free(name); return NULL; }
       ext->name = name;
       ext->is_func = 1;
       ext->type = NULL;
       ext->params = NULL;
       ext->param_count = 0;
       if (!match(p, TOKEN_LPAREN)) {
           errhandler__report_error(p->line, p->column, "syntax",
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
                   errhandler__report_error(p->line, p->column, "syntax",
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
           errhandler__report_error(p->line, p->column, "syntax",
                                    "Expected ')' after parameters");
           p->panic_mode = true;
       }
       if (!match(p, TOKEN_COLON)) {
           errhandler__report_error(p->line, p->column, "syntax",
                                    "Expected ':' for extern func return type");
           p->panic_mode = true;
       }
       ext->type = parse_type_spec(p);
       if (!ext->type) {
           errhandler__report_error(p->line, p->column, "syntax",
                                    "Expected return type");
           p->panic_mode = true;
       }
       if (!match(p, TOKEN_SEMICOLON)) {
           errhandler__report_error(p->line, p->column, "syntax",
                                    "Expected ';' after extern func");
           p->panic_mode = true;
       }
       return (ASTNode*)ext;
   } else if (peek(p) == TOKEN_LET) {
       advance(p);
       if (!match(p, TOKEN_IDENTIFIER)) {
           errhandler__report_error(p->line, p->column, "syntax",
                                    "Expected variable name");
           p->panic_mode = true;
           return NULL;
       }
       char* name = strdup_custom(p->tokens[p->current - 1].value);
       ExternDecl* ext = (ExternDecl*)make_node(p, NODE_EXTERN_DECL, sizeof(ExternDecl));
       if (!ext) { free(name); return NULL; }
       ext->name = name;
       ext->is_func = 0;
       ext->type = NULL;
       ext->params = NULL;
       ext->param_count = 0;
       if (!match(p, TOKEN_COLON)) {
           errhandler__report_error(p->line, p->column, "syntax",
                                    "Expected ':' for extern let type");
           p->panic_mode = true;
           return (ASTNode*)ext;
       }
       ext->type = parse_type_spec(p);
       if (!ext->type) {
           errhandler__report_error(p->line, p->column, "syntax",
                                    "Expected type");
           p->panic_mode = true;
       }
       if (!match(p, TOKEN_SEMICOLON)) {
           errhandler__report_error(p->line, p->column, "syntax",
                                    "Expected ';' after extern let");
           p->panic_mode = true;
       }
       return (ASTNode*)ext;
   } else {
       errhandler__report_error(p->line, p->column, "syntax",
                                "Expected 'func' or 'let' after 'extern'");
       p->panic_mode = true;
       return NULL;
   }
}

/*
* Parses a process declaration.
* Consumes the 'proc' keyword.
* Reads an identifier as the process name.
* Parses the process body as a block.
* Returns a ProcDecl AST node.
*/
ASTNode* parse_proc_decl(Parser* p) {
   advance(p);
   if (!match(p, TOKEN_IDENTIFIER)) {
       errhandler__report_error(p->line, p->column, "syntax",
                                "Expected proc name");
       p->panic_mode = true;
       return NULL;
   }
   char* name = strdup_custom(p->tokens[p->current - 1].value);
   ProcDecl* proc = (ProcDecl*)make_node(p, NODE_PROC_DECL, sizeof(ProcDecl));
   if (!proc) { free(name); return NULL; }
   proc->name = name;
   proc->body = parse_block(p);
   if (!proc->body) {
       errhandler__report_error(p->line, p->column, "syntax",
                                "Expected proc body block");
       p->panic_mode = true;
   }
   return (ASTNode*)proc;
}
