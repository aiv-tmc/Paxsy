#include "internal.h"
#include <stdlib.h>
#include <string.h>

/*
 * Parses a type specification. Handles prefix '&', base type,
 * then postfix operators [expr], ?, !.
 */
ASTNode* parse_type_spec(Parser* p) {
    // Pointer: & Type
    if (match(p, TOKEN_AMPERSAND)) {
        ASTNode* base = parse_type_spec(p);
        if (!base) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected type after '&'");
            p->panic_mode = true;
            return NULL;
        }
        PointerType* pt = (PointerType*)make_node(p, NODE_POINTER_TYPE, sizeof(PointerType));
        if (!pt) {
            free_ast_node(base);
            return NULL;
        }
        pt->base_type = base;
        return (ASTNode*)pt;
    }

    // Base type
    ASTNode* base_node = NULL;
    TokenType type = peek(p);

    if (type == TOKEN_LPAREN) {
        // Tuple type: ( Type , ... )
        advance(p);
        TupleType* tt = (TupleType*)make_node(p, NODE_TUPLE_TYPE, sizeof(TupleType));
        if (!tt) {
            return NULL;
        }
        tt->elements = NULL;
        tt->count = 0;

        if (peek(p) != TOKEN_RPAREN) {
            do {
                ASTNode* elem = parse_type_spec(p);
                if (!elem) {
                    errhandler__report_error(p->line, p->column, "parser",
                                             "Expected type in tuple");
                    p->panic_mode = true;
                    break;
                }
                append_child(p, (void***)&tt->elements, &tt->count, elem);
            } while (match(p, TOKEN_COMMA) && peek(p) != TOKEN_RPAREN);
        }
        if (!match(p, TOKEN_RPAREN)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected ')' to close tuple type");
            p->panic_mode = true;
        }
        base_node = (ASTNode*)tt;
    } else if (type == TOKEN_FUNC) {
        // Function type: func ( Type , ... ) : Type
        advance(p);
        if (!match(p, TOKEN_LPAREN)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected '(' in function type");
            p->panic_mode = true;
            return NULL;
        }
        FuncType* ft = (FuncType*)make_node(p, NODE_FUNC_TYPE, sizeof(FuncType));
        if (!ft) {
            return NULL;
        }
        ft->param_types = NULL;
        ft->param_count = 0;

        if (peek(p) != TOKEN_RPAREN) {
            do {
                ASTNode* param = parse_type_spec(p);
                if (!param) {
                    errhandler__report_error(p->line, p->column, "parser",
                                             "Expected parameter type");
                    p->panic_mode = true;
                    break;
                }
                append_child(p, (void***)&ft->param_types, &ft->param_count, param);
            } while (match(p, TOKEN_COMMA) && peek(p) != TOKEN_RPAREN);
        }
        if (!match(p, TOKEN_RPAREN)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected ')' after function parameters");
            p->panic_mode = true;
        }
        if (!match(p, TOKEN_COLON)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected ':' in function return type");
            p->panic_mode = true;
        }
        ft->return_type = parse_type_spec(p);
        if (!ft->return_type) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected return type");
            p->panic_mode = true;
        }
        base_node = (ASTNode*)ft;
    } else if (type == TOKEN_IDENTIFIER) {
        Token* tok = advance(p);
        bool is_family = false;
        FamilyTypeKind fkind;

        switch (tok->type) {
            case TOKEN_INT:   fkind = TYPE_FAMILY_INT;   is_family = true; break;
            case TOKEN_UINT:  fkind = TYPE_FAMILY_UINT;  is_family = true; break;
            case TOKEN_REAL:  fkind = TYPE_FAMILY_REAL;  is_family = true; break;
            case TOKEN_CHAR:  fkind = TYPE_FAMILY_CHAR;  is_family = true; break;
            default: break;
        }
        if (is_family) {
            FamilyType* ft = (FamilyType*)make_node(p, NODE_FAMILY_TYPE, sizeof(FamilyType));
            if (!ft) {
                return NULL;
            }
            ft->kind = fkind;
            base_node = (ASTNode*)ft;
        } else {
            BasicTypeKind kind;
            bool is_basic = true;
            switch (tok->type) {
                case TOKEN_INT8:   kind = TYPE_BASIC_INT8; break;
                case TOKEN_INT16:  kind = TYPE_BASIC_INT16; break;
                case TOKEN_INT32:  kind = TYPE_BASIC_INT32; break;
                case TOKEN_INT64:  kind = TYPE_BASIC_INT64; break;
                case TOKEN_INT128: kind = TYPE_BASIC_INT128; break;
                case TOKEN_INT256: kind = TYPE_BASIC_INT256; break;
                case TOKEN_UINT8:  kind = TYPE_BASIC_UINT8; break;
                case TOKEN_UINT16: kind = TYPE_BASIC_UINT16; break;
                case TOKEN_UINT32: kind = TYPE_BASIC_UINT32; break;
                case TOKEN_UINT64: kind = TYPE_BASIC_UINT64; break;
                case TOKEN_UINT128: kind = TYPE_BASIC_UINT128; break;
                case TOKEN_UINT256: kind = TYPE_BASIC_UINT256; break;
                case TOKEN_REAL32: kind = TYPE_BASIC_REAL32; break;
                case TOKEN_REAL64: kind = TYPE_BASIC_REAL64; break;
                case TOKEN_UTF8:   kind = TYPE_BASIC_UTF8; break;
                case TOKEN_UTF16:  kind = TYPE_BASIC_UTF16; break;
                case TOKEN_UTF32:  kind = TYPE_BASIC_UTF32; break;
                case TOKEN_BOOL:   kind = TYPE_BASIC_BOOL; break;
                case TOKEN_VOID:   kind = TYPE_BASIC_VOID; break;
                case TOKEN_AUTO:   kind = TYPE_BASIC_AUTO; break;
                default:
                    is_basic = false;
                    break;
            }
            if (is_basic) {
                BasicType* bt = (BasicType*)make_node(p, NODE_BASIC_TYPE, sizeof(BasicType));
                if (!bt) {
                    return NULL;
                }
                bt->kind = kind;
                base_node = (ASTNode*)bt;
            } else {
                IdentType* it = (IdentType*)make_node(p, NODE_IDENT_TYPE, sizeof(IdentType));
                if (!it) {
                    return NULL;
                }
                it->name = strdup_custom(tok->value);
                base_node = (ASTNode*)it;
            }
        }
    } else if (type == TOKEN_STRUCT) {
        advance(p);
        StructType* st = (StructType*)make_node(p, NODE_STRUCT_TYPE, sizeof(StructType));
        if (!st) {
            return NULL;
        }
        st->fields = NULL;
        st->field_count = 0;
        st->methods = NULL;
        st->method_count = 0;

        if (!match(p, TOKEN_LPAREN)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected '(' after Struct");
            p->panic_mode = true;
            return (ASTNode*)st;
        }

        while (peek(p) != TOKEN_RPAREN && peek(p) != TOKEN_EOF) {
            if (peek(p) == TOKEN_FUNC) {
                ASTNode* method = parse_func_decl(p, 1);
                if (method) {
                    append_child(p, (void***)&st->methods, &st->method_count, method);
                } else {
                    p->panic_mode = true;
                    synchronize(p);
                }
            } else {
                // Field: name [ ]? : Type [ = expr ]
                char* fname = NULL;
                ASTNode* ftype = NULL;
                uint32_t mods = 0;
                // We cannot use parse_name_type directly because of array field syntax
                // So we keep the manual parsing.
                if (!match(p, TOKEN_IDENTIFIER)) {
                    errhandler__report_error(p->line, p->column, "parser",
                                             "Expected field name in struct");
                    break;
                }
                fname = strdup_custom(p->tokens[p->current - 1].value);
                bool is_array_field = false;
                if (match(p, TOKEN_LBRACKET)) {
                    if (!match(p, TOKEN_RBRACKET)) {
                        errhandler__report_error(p->line, p->column, "parser",
                                                 "Expected ']' for array field");
                        free(fname);
                        break;
                    }
                    is_array_field = true;
                }
                if (!match(p, TOKEN_COLON)) {
                    errhandler__report_error(p->line, p->column, "parser",
                                             "Expected ':' after field name");
                    free(fname);
                    break;
                }
                ftype = parse_type_spec(p);
                if (!ftype) {
                    free(fname);
                    break;
                }
                if (is_array_field) {
                    ArrayType* at = (ArrayType*)make_node(p, NODE_ARRAY_TYPE, sizeof(ArrayType));
                    if (!at) {
                        free(fname);
                        free_ast_node(ftype);
                        break;
                    }
                    at->base_type = ftype;
                    at->size = NULL;
                    ftype = (ASTNode*)at;
                }
                ASTNode* defval = NULL;
                if (match(p, TOKEN_EQUAL)) {
                    defval = parse_assignment_expr(p);
                    if (!defval) {
                        errhandler__report_error(p->line, p->column, "parser",
                                                 "Expected default value");
                        p->panic_mode = true;
                    }
                }
                Field* f = (Field*)make_node(p, NODE_FIELD, sizeof(Field));
                if (!f) {
                    free(fname);
                    free_ast_node(ftype);
                    free_ast_node(defval);
                    break;
                }
                f->name = fname;
                f->type = ftype;
                f->default_value = defval;
                append_child(p, (void***)&st->fields, &st->field_count, f);
                if (!match(p, TOKEN_COMMA)) {
                    break;
                }
            }
        }
        if (!match(p, TOKEN_RPAREN)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected ')' to close struct");
            p->panic_mode = true;
        }
        base_node = (ASTNode*)st;
    } else if (type == TOKEN_UNION) {
        advance(p);
        UnionType* ut = (UnionType*)make_node(p, NODE_UNION_TYPE, sizeof(UnionType));
        if (!ut) {
            return NULL;
        }
        ut->fields = NULL;
        ut->field_count = 0;

        if (!match(p, TOKEN_LPAREN)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected '(' after Union");
            p->panic_mode = true;
            return (ASTNode*)ut;
        }

        while (peek(p) != TOKEN_RPAREN && peek(p) != TOKEN_EOF) {
            if (!match(p, TOKEN_IDENTIFIER)) {
                errhandler__report_error(p->line, p->column, "parser",
                                         "Expected field name in union");
                break;
            }
            char* fname = strdup_custom(p->tokens[p->current - 1].value);
            bool is_array_field = false;
            if (match(p, TOKEN_LBRACKET)) {
                if (!match(p, TOKEN_RBRACKET)) {
                    errhandler__report_error(p->line, p->column, "parser",
                                             "Expected ']' for array field");
                    free(fname);
                    break;
                }
                is_array_field = true;
            }
            if (!match(p, TOKEN_COLON)) {
                errhandler__report_error(p->line, p->column, "parser",
                                         "Expected ':' after field name");
                free(fname);
                break;
            }
            ASTNode* ftype = parse_type_spec(p);
            if (!ftype) {
                free(fname);
                break;
            }
            if (is_array_field) {
                ArrayType* at = (ArrayType*)make_node(p, NODE_ARRAY_TYPE, sizeof(ArrayType));
                if (!at) {
                    free(fname);
                    free_ast_node(ftype);
                    break;
                }
                at->base_type = ftype;
                at->size = NULL;
                ftype = (ASTNode*)at;
            }
            Field* f = (Field*)make_node(p, NODE_FIELD, sizeof(Field));
            if (!f) {
                free(fname);
                free_ast_node(ftype);
                break;
            }
            f->name = fname;
            f->type = ftype;
            f->default_value = NULL;
            append_child(p, (void***)&ut->fields, &ut->field_count, f);
            if (!match(p, TOKEN_COMMA)) {
                break;
            }
        }
        if (!match(p, TOKEN_RPAREN)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected ')' to close union");
            p->panic_mode = true;
        }
        base_node = (ASTNode*)ut;
    } else if (type == TOKEN_ENUM) {
        advance(p);
        EnumType* et = (EnumType*)make_node(p, NODE_ENUM_TYPE, sizeof(EnumType));
        if (!et) {
            return NULL;
        }
        et->constants = NULL;
        et->const_count = 0;

        if (!match(p, TOKEN_LBRACE)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected '{' after Enum");
            p->panic_mode = true;
            return (ASTNode*)et;
        }

        while (peek(p) != TOKEN_RBRACE && peek(p) != TOKEN_EOF) {
            if (!match(p, TOKEN_IDENTIFIER)) {
                errhandler__report_error(p->line, p->column, "parser",
                                         "Expected constant name in enum");
                break;
            }
            char* cname = strdup_custom(p->tokens[p->current - 1].value);
            ASTNode* cval = NULL;
            if (match(p, TOKEN_EQUAL)) {
                cval = parse_assignment_expr(p);
                if (!cval) {
                    errhandler__report_error(p->line, p->column, "parser",
                                             "Expected constant value");
                    p->panic_mode = true;
                }
            }
            EnumConst* ec = (EnumConst*)make_node(p, NODE_ENUM_CONST, sizeof(EnumConst));
            if (!ec) {
                free(cname);
                free_ast_node(cval);
                break;
            }
            ec->name = cname;
            ec->value = cval;
            append_child(p, (void***)&et->constants, &et->const_count, ec);
            if (!match(p, TOKEN_COMMA)) {
                break;
            }
        }
        if (!match(p, TOKEN_RBRACE)) {
            errhandler__report_error(p->line, p->column, "parser",
                                     "Expected '}' to close enum");
            p->panic_mode = true;
        }
        base_node = (ASTNode*)et;
    } else {
        errhandler__report_error(p->line, p->column, "parser", "Expected type");
        p->panic_mode = true;
        return NULL;
    }

    if (!base_node) {
        return NULL;
    }

    // Postfix operators: array, nullable, error.
    while (1) {
        if (match(p, TOKEN_LBRACKET)) {
            ASTNode* size = NULL;
            if (peek(p) != TOKEN_RBRACKET) {
                size = parse_assignment_expr(p);
                if (!size) {
                    errhandler__report_error(p->line, p->column, "parser",
                                             "Expected array size expression");
                    p->panic_mode = true;
                    break;
                }
            }
            if (!match(p, TOKEN_RBRACKET)) {
                errhandler__report_error(p->line, p->column, "parser",
                                         "Expected ']' after array size");
                p->panic_mode = true;
                free_ast_node(size);
                break;
            }
            ArrayType* at = (ArrayType*)make_node(p, NODE_ARRAY_TYPE, sizeof(ArrayType));
            if (!at) {
                free_ast_node(base_node);
                free_ast_node(size);
                return NULL;
            }
            at->base_type = base_node;
            at->size = size;
            base_node = (ASTNode*)at;
        } else if (match(p, TOKEN_QUESTION)) {
            NullableType* nt = (NullableType*)make_node(p, NODE_NULLABLE_TYPE, sizeof(NullableType));
            if (!nt) {
                free_ast_node(base_node);
                return NULL;
            }
            nt->base_type = base_node;
            base_node = (ASTNode*)nt;
        } else if (match(p, TOKEN_BANG)) {
            ErrorType* et = (ErrorType*)make_node(p, NODE_ERROR_TYPE, sizeof(ErrorType));
            if (!et) {
                free_ast_node(base_node);
                return NULL;
            }
            et->base_type = base_node;
            base_node = (ASTNode*)et;
        } else {
            break;
        }
    }
    return base_node;
}
