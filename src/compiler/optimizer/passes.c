#include "passes.h"
#include "ast_utils.h"
#include "symbol_utils.h"
#include "../semantic/semantic.h"
#include "../../interfaces/errhandler/errhandler.h"
#include "../../interfaces/utils/str.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <float.h>

extern bool opt_debug;
#define DEBUG_PRINT(...) do { if (opt_debug) printf(__VA_ARGS__); } while (0)

/* Helper functions for literal creation and extraction */

static LiteralExpr* make_number_literal(long long value, int line, int column) {
    LiteralExpr* lit = (LiteralExpr*)malloc(sizeof(LiteralExpr));
    if (!lit) return NULL;
    memset(lit, 0, sizeof(LiteralExpr));
    lit->base.type = NODE_LITERAL;
    lit->base.line = line;
    lit->base.column = column;
    lit->kind = LIT_NUMBER;
    char buf[64];
    snprintf(buf, sizeof(buf), "%lld", value);
    lit->raw_value = u__strduplic(buf);
    if (!lit->raw_value) {
        free(lit);
        return NULL;
    }
    return lit;
}

static LiteralExpr* make_float_literal(double value, int line, int column) {
    LiteralExpr* lit = (LiteralExpr*)malloc(sizeof(LiteralExpr));
    if (!lit) return NULL;
    memset(lit, 0, sizeof(LiteralExpr));
    lit->base.type = NODE_LITERAL;
    lit->base.line = line;
    lit->base.column = column;
    lit->kind = LIT_FLOAT;
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", value);
    lit->raw_value = u__strduplic(buf);
    if (!lit->raw_value) {
        free(lit);
        return NULL;
    }
    return lit;
}

static LiteralExpr* make_bool_literal(bool value, int line, int column) {
    LiteralExpr* lit = (LiteralExpr*)malloc(sizeof(LiteralExpr));
    if (!lit) return NULL;
    memset(lit, 0, sizeof(LiteralExpr));
    lit->base.type = NODE_LITERAL;
    lit->base.line = line;
    lit->base.column = column;
    lit->kind = LIT_BOOL;
    lit->raw_value = u__strduplic(value ? "true" : "false");
    if (!lit->raw_value) {
        free(lit);
        return NULL;
    }
    return lit;
}

static LiteralExpr* make_string_literal(const char* str, int line, int column) {
    LiteralExpr* lit = (LiteralExpr*)malloc(sizeof(LiteralExpr));
    if (!lit) return NULL;
    memset(lit, 0, sizeof(LiteralExpr));
    lit->base.type = NODE_LITERAL;
    lit->base.line = line;
    lit->base.column = column;
    lit->kind = LIT_STRING;
    lit->raw_value = u__strduplic(str);
    if (!lit->raw_value) {
        free(lit);
        return NULL;
    }
    return lit;
}

static bool get_number_value(LiteralExpr* lit, long long* int_val, double* float_val, bool* is_float) {
    if (lit->kind == LIT_NUMBER || lit->kind == LIT_FLOAT) {
        char* end;
        long long v = strtoll(lit->raw_value, &end, 10);
        if (*end == '\0') {
            *int_val = v;
            *is_float = false;
            return true;
        }
        if (strchr(lit->raw_value, '.') || strchr(lit->raw_value, 'e') || strchr(lit->raw_value, 'E')) {
            double f = strtod(lit->raw_value, NULL);
            *float_val = f;
            *is_float = true;
            return true;
        }
        return false;
    }
    return false;
}

static bool get_bool_value(LiteralExpr* lit, bool* val) {
    if (lit->kind != LIT_BOOL) return false;
    if (strcmp(lit->raw_value, "true") == 0) {
        *val = true;
        return true;
    } else if (strcmp(lit->raw_value, "false") == 0) {
        *val = false;
        return true;
    }
    return false;
}

static bool check_overflow_add(long long a, long long b, long long* result) {
    if (b > 0 && a > LLONG_MAX - b) return true;
    if (b < 0 && a < LLONG_MIN - b) return true;
    *result = a + b;
    return false;
}

static bool check_overflow_sub(long long a, long long b, long long* result) {
    if (b > 0 && a < LLONG_MIN + b) return true;
    if (b < 0 && a > LLONG_MAX + b) return true;
    *result = a - b;
    return false;
}

static bool check_overflow_mul(long long a, long long b, long long* result) {
    if (a == 0 || b == 0) {
        *result = 0;
        return false;
    }
    if (a > 0) {
        if (b > 0) {
            if (a > LLONG_MAX / b) return true;
        } else {
            if (b < LLONG_MIN / a) return true;
        }
    } else {
        if (b > 0) {
            if (a < LLONG_MIN / b) return true;
        } else {
            if (a < LLONG_MAX / b) return true;
        }
    }
    *result = a * b;
    return false;
}

/* Constant folding pass (enhanced) */

static ASTNode* fold_expression(ASTNode* node, SemanticContext* ctx) {
    (void)ctx;
    if (!node) return NULL;

    switch (node->type) {
        case NODE_BINARY: {
            BinaryExpr* bin = (BinaryExpr*)node;
            if (bin->left->type != NODE_LITERAL || bin->right->type != NODE_LITERAL)
                break;

            LiteralExpr* l = (LiteralExpr*)bin->left;
            LiteralExpr* r = (LiteralExpr*)bin->right;

            long long l_int, r_int;
            double l_float, r_float;
            bool l_is_float, r_is_float;
            bool l_ok = get_number_value(l, &l_int, &l_float, &l_is_float);
            bool r_ok = get_number_value(r, &r_int, &r_float, &r_is_float);

            if (l_ok && r_ok) {
                bool use_float = l_is_float || r_is_float;
                long long int_result = 0;
                double float_result = 0.0;
                bool ok = true;

                switch (bin->op) {
                    case BIN_ADD:
                        if (use_float) {
                            float_result = (l_is_float ? l_float : (double)l_int) +
                                           (r_is_float ? r_float : (double)r_int);
                        } else {
                            if (check_overflow_add(l_int, r_int, &int_result)) {
                                errhandler__report_error(node->line, node->column,
                                                         "opt", "constant addition overflow");
                                ok = false;
                            }
                        }
                        break;
                    case BIN_SUB:
                        if (use_float) {
                            float_result = (l_is_float ? l_float : (double)l_int) -
                                           (r_is_float ? r_float : (double)r_int);
                        } else {
                            if (check_overflow_sub(l_int, r_int, &int_result)) {
                                errhandler__report_error(node->line, node->column,
                                                         "opt", "constant subtraction overflow");
                                ok = false;
                            }
                        }
                        break;
                    case BIN_MUL:
                        if (use_float) {
                            float_result = (l_is_float ? l_float : (double)l_int) *
                                           (r_is_float ? r_float : (double)r_int);
                        } else {
                            if (check_overflow_mul(l_int, r_int, &int_result)) {
                                errhandler__report_error(node->line, node->column,
                                                         "opt", "constant multiplication overflow");
                                ok = false;
                            }
                        }
                        break;
                    case BIN_DIV:
                        if (use_float) {
                            double divisor = r_is_float ? r_float : (double)r_int;
                            if (divisor == 0.0) {
                                errhandler__report_error(node->line, node->column,
                                                         "opt", "division by zero in constant expression");
                                ok = false;
                            } else {
                                float_result = (l_is_float ? l_float : (double)l_int) / divisor;
                            }
                        } else {
                            if (r_int == 0) {
                                errhandler__report_error(node->line, node->column,
                                                         "opt", "division by zero in constant expression");
                                ok = false;
                            } else {
                                int_result = l_int / r_int;
                            }
                        }
                        break;
                    case BIN_MOD:
                        if (use_float) {
                            errhandler__report_error(node->line, node->column,
                                                     "opt", "modulo operation with floating point not supported");
                            ok = false;
                        } else {
                            if (r_int == 0) {
                                errhandler__report_error(node->line, node->column,
                                                         "opt", "modulo by zero in constant expression");
                                ok = false;
                            } else {
                                int_result = l_int % r_int;
                            }
                        }
                        break;
                    case BIN_AND:
                        if (!use_float) {
                            int_result = l_int & r_int;
                        } else {
                            errhandler__report_error(node->line, node->column,
                                                     "opt", "bitwise operation on floating point");
                            ok = false;
                        }
                        break;
                    case BIN_OR:
                        if (!use_float) {
                            int_result = l_int | r_int;
                        } else {
                            errhandler__report_error(node->line, node->column,
                                                     "opt", "bitwise operation on floating point");
                            ok = false;
                        }
                        break;
                    case BIN_XOR:
                        if (!use_float) {
                            int_result = l_int ^ r_int;
                        } else {
                            errhandler__report_error(node->line, node->column,
                                                     "opt", "bitwise operation on floating point");
                            ok = false;
                        }
                        break;
                    case BIN_SHL:
                        if (!use_float) {
                            int_result = l_int << r_int;
                        } else {
                            errhandler__report_error(node->line, node->column,
                                                     "opt", "shift operation on floating point");
                            ok = false;
                        }
                        break;
                    case BIN_SHR:
                        if (!use_float) {
                            int_result = l_int >> r_int;
                        } else {
                            errhandler__report_error(node->line, node->column,
                                                     "opt", "shift operation on floating point");
                            ok = false;
                        }
                        break;
                    case BIN_SHL_LOG:
                        if (!use_float) {
                            int_result = (unsigned long long)l_int << r_int;
                        } else {
                            errhandler__report_error(node->line, node->column,
                                                     "opt", "shift operation on floating point");
                            ok = false;
                        }
                        break;
                    case BIN_SHR_LOG:
                        if (!use_float) {
                            int_result = (unsigned long long)l_int >> r_int;
                        } else {
                            errhandler__report_error(node->line, node->column,
                                                     "opt", "shift operation on floating point");
                            ok = false;
                        }
                        break;
                    case BIN_EQ:
                        if (use_float) {
                            float_result = ((l_is_float ? l_float : (double)l_int) ==
                                            (r_is_float ? r_float : (double)r_int)) ? 1.0 : 0.0;
                        } else {
                            int_result = (l_int == r_int) ? 1 : 0;
                        }
                        break;
                    case BIN_NE:
                        if (use_float) {
                            float_result = ((l_is_float ? l_float : (double)l_int) !=
                                            (r_is_float ? r_float : (double)r_int)) ? 1.0 : 0.0;
                        } else {
                            int_result = (l_int != r_int) ? 1 : 0;
                        }
                        break;
                    case BIN_LT:
                        if (use_float) {
                            float_result = ((l_is_float ? l_float : (double)l_int) <
                                            (r_is_float ? r_float : (double)r_int)) ? 1.0 : 0.0;
                        } else {
                            int_result = (l_int < r_int) ? 1 : 0;
                        }
                        break;
                    case BIN_LE:
                        if (use_float) {
                            float_result = ((l_is_float ? l_float : (double)l_int) <=
                                            (r_is_float ? r_float : (double)r_int)) ? 1.0 : 0.0;
                        } else {
                            int_result = (l_int <= r_int) ? 1 : 0;
                        }
                        break;
                    case BIN_GT:
                        if (use_float) {
                            float_result = ((l_is_float ? l_float : (double)l_int) >
                                            (r_is_float ? r_float : (double)r_int)) ? 1.0 : 0.0;
                        } else {
                            int_result = (l_int > r_int) ? 1 : 0;
                        }
                        break;
                    case BIN_GE:
                        if (use_float) {
                            float_result = ((l_is_float ? l_float : (double)l_int) >=
                                            (r_is_float ? r_float : (double)r_int)) ? 1.0 : 0.0;
                        } else {
                            int_result = (l_int >= r_int) ? 1 : 0;
                        }
                        break;
                    /* Logical operators – use the correct enum names from parser.h */
                    case BIN_LOGICAL_AND: {
                        bool lv, rv;
                        if (get_bool_value(l, &lv) && get_bool_value(r, &rv)) {
                            return (ASTNode*)make_bool_literal(lv && rv, node->line, node->column);
                        }
                        ok = false;
                        break;
                    }
                    case BIN_LOGICAL_OR: {
                        bool lv, rv;
                        if (get_bool_value(l, &lv) && get_bool_value(r, &rv)) {
                            return (ASTNode*)make_bool_literal(lv || rv, node->line, node->column);
                        }
                        ok = false;
                        break;
                    }
                    default:
                        ok = false;
                        break;
                }

                if (ok) {
                    if (bin->op >= BIN_EQ && bin->op <= BIN_GE) {
                        bool b = (use_float) ? (float_result != 0.0) : (int_result != 0);
                        return (ASTNode*)make_bool_literal(b, node->line, node->column);
                    }
                    if (use_float)
                        return (ASTNode*)make_float_literal(float_result, node->line, node->column);
                    else
                        return (ASTNode*)make_number_literal(int_result, node->line, node->column);
                }
            }

            /* String concatenation */
            if (l->kind == LIT_STRING && r->kind == LIT_STRING) {
                char* cat = (char*)malloc(strlen(l->raw_value) + strlen(r->raw_value) + 1);
                if (!cat) break;
                strcpy(cat, l->raw_value);
                strcat(cat, r->raw_value);
                LiteralExpr* new_lit = make_string_literal(cat, node->line, node->column);
                free(cat);
                return (ASTNode*)new_lit;
            }
            break;
        }

        case NODE_UNARY: {
            UnaryExpr* un = (UnaryExpr*)node;
            if (un->operand->type != NODE_LITERAL) break;
            LiteralExpr* l = (LiteralExpr*)un->operand;
            if (l->kind == LIT_NUMBER || l->kind == LIT_FLOAT) {
                bool is_float = (l->kind == LIT_FLOAT);
                long long int_val = 0;
                double float_val = 0.0;
                if (is_float) {
                    float_val = strtod(l->raw_value, NULL);
                } else {
                    int_val = strtoll(l->raw_value, NULL, 10);
                }
                bool ok = true;
                long long result_int = 0;
                double result_float = 0.0;
                switch (un->op) {
                    case UNARY_MINUS:
                        if (is_float) result_float = -float_val;
                        else result_int = -int_val;
                        break;
                    case UNARY_PLUS:
                        if (is_float) result_float = float_val;
                        else result_int = int_val;
                        break;
                    case UNARY_BIT_NOT:
                        if (!is_float) result_int = ~int_val;
                        else ok = false;
                        break;
                    default:
                        ok = false;
                        break;
                }
                if (ok) {
                    if (is_float)
                        return (ASTNode*)make_float_literal(result_float, node->line, node->column);
                    else
                        return (ASTNode*)make_number_literal(result_int, node->line, node->column);
                }
            } else if (l->kind == LIT_BOOL) {
                bool val;
                if (get_bool_value(l, &val) && un->op == UNARY_LOGICAL_NOT) {
                    return (ASTNode*)make_bool_literal(!val, node->line, node->column);
                }
            }
            break;
        }

        case NODE_IF_EXPR: {
            IfExpr* ife = (IfExpr*)node;
            if (ife->condition->type != NODE_LITERAL) break;
            bool val;
            if (!get_bool_value((LiteralExpr*)ife->condition, &val)) break;
            if (val) {
                ASTNode* replacement = ife->then_expr;
                ife->then_expr = NULL;
                u__free_node(node);
                return replacement;
            } else if (ife->else_expr) {
                ASTNode* replacement = ife->else_expr;
                ife->else_expr = NULL;
                u__free_node(node);
                return replacement;
            } else {
                u__free_node(node);
                return NULL;
            }
        }

        default:
            break;
    }
    return NULL;
}

bool pass_constant_folding(Program* ast) {
    if (!ast) return false;
    bool changed = false;
    SemanticContext ctx = {0};
    ASTNode* (*fold)(ASTNode*, SemanticContext*) = fold_expression;

    for (size_t i = 0; i < ast->item_count; ++i) {
        ASTNode* old = ast->items[i];
        u__replace_expressions(&ast->items[i], fold, &ctx);
        if (ast->items[i] != old) changed = true;
    }
    if (changed) DEBUG_PRINT("Constant folding: applied transformations.\n");
    return changed;
}

/* Constant propagation (AST-based with scoping) */

typedef enum {
    CV_UNKNOWN,
    CV_CONSTANT
} ConstValueKind;

typedef struct ConstValue {
    ConstValueKind kind;
    ASTNode* literal;
} ConstValue;

typedef struct VarMap {
    char** names;
    ConstValue* values;
    size_t count;
    size_t capacity;
} VarMap;

static VarMap* var_map_create(void) {
    VarMap* map = (VarMap*)malloc(sizeof(VarMap));
    if (!map) return NULL;
    map->names = NULL;
    map->values = NULL;
    map->count = 0;
    map->capacity = 0;
    return map;
}

static void var_map_free(VarMap* map) {
    if (!map) return;
    for (size_t i = 0; i < map->count; ++i) {
        free(map->names[i]);
    }
    free(map->names);
    free(map->values);
    free(map);
}

static ConstValue* var_map_lookup(VarMap* map, const char* name) {
    if (!map) return NULL;
    for (size_t i = 0; i < map->count; ++i) {
        if (strcmp(map->names[i], name) == 0) {
            return &map->values[i];
        }
    }
    return NULL;
}

static void var_map_insert(VarMap* map, const char* name, ConstValue val) {
    if (!map) return;
    for (size_t i = 0; i < map->count; ++i) {
        if (strcmp(map->names[i], name) == 0) {
            map->values[i] = val;
            return;
        }
    }
    if (map->count >= map->capacity) {
        size_t new_cap = map->capacity == 0 ? 4 : map->capacity * 2;
        char** new_names = (char**)realloc(map->names, new_cap * sizeof(char*));
        ConstValue* new_vals = (ConstValue*)realloc(map->values, new_cap * sizeof(ConstValue));
        if (!new_names || !new_vals) return;
        map->names = new_names;
        map->values = new_vals;
        map->capacity = new_cap;
    }
    map->names[map->count] = u__strduplic(name);
    map->values[map->count] = val;
    map->count++;
}

static void var_map_merge(VarMap* dst, VarMap* src) {
    if (!dst || !src) return;
    for (size_t i = 0; i < src->count; ++i) {
        ConstValue* existing = var_map_lookup(dst, src->names[i]);
        if (!existing) {
            var_map_insert(dst, src->names[i], src->values[i]);
        } else {
            if (existing->kind == CV_CONSTANT && src->values[i].kind == CV_CONSTANT) {
                LiteralExpr* e1 = (LiteralExpr*)existing->literal;
                LiteralExpr* e2 = (LiteralExpr*)src->values[i].literal;
                if (e1->kind != e2->kind || strcmp(e1->raw_value, e2->raw_value) != 0) {
                    existing->kind = CV_UNKNOWN;
                    existing->literal = NULL;
                }
            } else {
                existing->kind = CV_UNKNOWN;
                existing->literal = NULL;
            }
        }
    }
}

static VarMap* var_map_copy(VarMap* src) {
    if (!src) return NULL;
    VarMap* dst = var_map_create();
    if (!dst) return NULL;
    for (size_t i = 0; i < src->count; ++i) {
        var_map_insert(dst, src->names[i], src->values[i]);
    }
    return dst;
}

static void var_map_mark_all_unknown(VarMap* map) {
    if (!map) return;
    for (size_t i = 0; i < map->count; ++i) {
        map->values[i].kind = CV_UNKNOWN;
        map->values[i].literal = NULL;
    }
}

static char* find_base_identifier(ASTNode* node) {
    if (!node) return NULL;
    if (node->type == NODE_IDENT) {
        return u__strduplic(((IdentExpr*)node)->name);
    }
    if (node->type == NODE_INDEX) {
        IndexExpr* idx = (IndexExpr*)node;
        return find_base_identifier(idx->array);
    }
    if (node->type == NODE_MEMBER_ACCESS) {
        MemberAccess* ma = (MemberAccess*)node;
        return find_base_identifier(ma->object);
    }
    return NULL;
}

typedef struct ConstPropContext {
    VarMap* map;
    bool* changed;
} ConstPropContext;

/* Global pointer for the current propagation context (avoid modifying SemanticContext) */
static ConstPropContext* current_prop_ctx = NULL;

static ASTNode* propagate_fold(ASTNode* node, SemanticContext* ctx) {
    (void)ctx;
    if (!node || node->type != NODE_IDENT) return NULL;
    IdentExpr* ident = (IdentExpr*)node;
    ConstPropContext* cctx = current_prop_ctx;  // use global instead of ctx->user_data
    if (!cctx) return NULL;
    ConstValue* cv = var_map_lookup(cctx->map, ident->name);
    if (cv && cv->kind == CV_CONSTANT) {
        LiteralExpr* orig = (LiteralExpr*)cv->literal;
        LiteralExpr* new_lit = (LiteralExpr*)malloc(sizeof(LiteralExpr));
        if (!new_lit) return NULL;
        memcpy(new_lit, orig, sizeof(LiteralExpr));
        new_lit->raw_value = u__strduplic(orig->raw_value);
        if (!new_lit->raw_value) {
            free(new_lit);
            return NULL;
        }
        if (cctx->changed) *cctx->changed = true;
        return (ASTNode*)new_lit;
    }
    return NULL;
}

static void update_map_for_statement(ASTNode* stmt, ConstPropContext* ctx) {
    if (!stmt) return;
    switch (stmt->type) {
        case NODE_LET_DECL: {
            LetDecl* ld = (LetDecl*)stmt;
            if (ld->count == 1 && ld->initializer) {
                if (ld->initializer->type == NODE_LITERAL) {
                    ConstValue cv = { CV_CONSTANT, ld->initializer };
                    var_map_insert(ctx->map, ld->names[0], cv);
                } else {
                    ConstValue unknown = { CV_UNKNOWN, NULL };
                    var_map_insert(ctx->map, ld->names[0], unknown);
                }
            } else {
                for (size_t i = 0; i < ld->count; i++) {
                    ConstValue unknown = { CV_UNKNOWN, NULL };
                    var_map_insert(ctx->map, ld->names[i], unknown);
                }
            }
            break;
        }
        case NODE_EXPR_STMT: {
            ExprStmt* es = (ExprStmt*)stmt;
            if (es->expr && es->expr->type == NODE_ASSIGN) {
                AssignExpr* as = (AssignExpr*)es->expr;
                if (as->left->type == NODE_IDENT) {
                    IdentExpr* id = (IdentExpr*)as->left;
                    if (as->op == ASSIGN_NORMAL) {
                        if (as->right->type == NODE_LITERAL) {
                            ConstValue cv = { CV_CONSTANT, as->right };
                            var_map_insert(ctx->map, id->name, cv);
                        } else {
                            ConstValue unknown = { CV_UNKNOWN, NULL };
                            var_map_insert(ctx->map, id->name, unknown);
                        }
                    } else {
                        ConstValue unknown = { CV_UNKNOWN, NULL };
                        var_map_insert(ctx->map, id->name, unknown);
                    }
                } else if (as->left->type == NODE_INDEX || as->left->type == NODE_MEMBER_ACCESS) {
                    char* base = find_base_identifier(as->left);
                    if (base) {
                        ConstValue unknown = { CV_UNKNOWN, NULL };
                        var_map_insert(ctx->map, base, unknown);
                        free(base);
                    }
                }
            } else if (es->expr && es->expr->type == NODE_CALL) {
                var_map_mark_all_unknown(ctx->map);
            }
            break;
        }
        default:
            break;
    }
}

static void visit_block(Block* block, ConstPropContext* ctx, SemanticContext* sem_ctx) {
    if (!block) return;
    VarMap* saved_map = ctx->map;
    VarMap* block_map = var_map_copy(saved_map);
    if (!block_map) return;
    ctx->map = block_map;

    for (size_t i = 0; i < block->stmt_count; i++) {
        ASTNode* stmt = block->statements[i];
        current_prop_ctx = ctx;  // set global before replacement
        u__replace_expressions(&stmt, propagate_fold, sem_ctx);
        current_prop_ctx = NULL;
        block->statements[i] = stmt;
        update_map_for_statement(stmt, ctx);

        if (stmt->type == NODE_BLOCK) {
            visit_block((Block*)stmt, ctx, sem_ctx);
        } else if (stmt->type == NODE_IF_STMT) {
            IfStmt* ifs = (IfStmt*)stmt;
            current_prop_ctx = ctx;
            u__replace_expressions(&ifs->condition, propagate_fold, sem_ctx);
            current_prop_ctx = NULL;
            VarMap* then_map = var_map_copy(ctx->map);
            if (then_map) {
                ConstPropContext then_ctx = { then_map, ctx->changed };
                visit_block((Block*)ifs->then_branch, &then_ctx, sem_ctx);
                var_map_free(then_map);
            }
            for (size_t j = 0; j < ifs->else_if_count; j++) {
                current_prop_ctx = ctx;
                u__replace_expressions(&ifs->else_if_conditions[j], propagate_fold, sem_ctx);
                current_prop_ctx = NULL;
                VarMap* elif_map = var_map_copy(ctx->map);
                if (elif_map) {
                    ConstPropContext elif_ctx = { elif_map, ctx->changed };
                    visit_block((Block*)ifs->else_if_branches[j], &elif_ctx, sem_ctx);
                    var_map_free(elif_map);
                }
            }
            if (ifs->else_branch) {
                VarMap* else_map = var_map_copy(ctx->map);
                if (else_map) {
                    ConstPropContext else_ctx = { else_map, ctx->changed };
                    visit_block((Block*)ifs->else_branch, &else_ctx, sem_ctx);
                    var_map_free(else_map);
                }
            }
        } else if (stmt->type == NODE_WHILE_STMT) {
            WhileStmt* ws = (WhileStmt*)stmt;
            current_prop_ctx = ctx;
            u__replace_expressions(&ws->condition, propagate_fold, sem_ctx);
            current_prop_ctx = NULL;
            VarMap* body_map = var_map_copy(ctx->map);
            if (body_map) {
                ConstPropContext body_ctx = { body_map, ctx->changed };
                visit_block((Block*)ws->body, &body_ctx, sem_ctx);
                var_map_free(body_map);
            }
        } else if (stmt->type == NODE_DO_WHILE_STMT) {
            DoWhileStmt* dw = (DoWhileStmt*)stmt;
            VarMap* body_map = var_map_copy(ctx->map);
            if (body_map) {
                ConstPropContext body_ctx = { body_map, ctx->changed };
                visit_block((Block*)dw->body, &body_ctx, sem_ctx);
                var_map_free(body_map);
            }
            current_prop_ctx = ctx;
            u__replace_expressions(&dw->condition, propagate_fold, sem_ctx);
            current_prop_ctx = NULL;
        } else if (stmt->type == NODE_FOR_STMT) {
            ForStmt* fs = (ForStmt*)stmt;
            current_prop_ctx = ctx;
            u__replace_expressions(&fs->range, propagate_fold, sem_ctx);
            if (fs->var_type) u__replace_expressions(&fs->var_type, propagate_fold, sem_ctx);
            current_prop_ctx = NULL;
            VarMap* body_map = var_map_copy(ctx->map);
            if (body_map) {
                ConstPropContext body_ctx = { body_map, ctx->changed };
                visit_block((Block*)fs->body, &body_ctx, sem_ctx);
                var_map_free(body_map);
            }
        }
    }

    ctx->map = saved_map;
    var_map_free(block_map);
}

static void propagate_constants_in_function_ast(FuncDecl* fd, bool* changed) {
    if (!fd || !fd->body) return;
    VarMap* global_map = var_map_create();
    if (!global_map) return;
    ConstPropContext ctx = { .map = global_map, .changed = changed };
    SemanticContext sem_ctx = {0};
    /* no user_data needed; we use global current_prop_ctx */

    visit_block((Block*)fd->body, &ctx, &sem_ctx);

    var_map_free(global_map);
}

bool pass_constant_propagation(Program* ast) {
    if (!ast) return false;
    bool changed = false;
    for (size_t i = 0; i < ast->item_count; ++i) {
        if (ast->items[i]->type == NODE_FUNC_DECL) {
            propagate_constants_in_function_ast((FuncDecl*)ast->items[i], &changed);
        }
    }
    if (changed) DEBUG_PRINT("Constant propagation completed.\n");
    return changed;
}

/* Dead code elimination (enhanced) */

static ASTNode* simplify_if_chain(ASTNode* node, SemanticContext* ctx) {
    (void)ctx;
    if (node->type != NODE_IF_STMT) return NULL;
    IfStmt* ifs = (IfStmt*)node;

    if (ifs->condition->type != NODE_LITERAL) return NULL;
    bool cond_val;
    if (!get_bool_value((LiteralExpr*)ifs->condition, &cond_val)) return NULL;

    if (cond_val) {
        ASTNode* replacement = ifs->then_branch;
        ifs->then_branch = NULL;
        u__free_node(ifs->condition);
        for (size_t i = 0; i < ifs->else_if_count; i++) {
            u__free_node(ifs->else_if_conditions[i]);
            u__free_node(ifs->else_if_branches[i]);
        }
        free(ifs->else_if_conditions);
        free(ifs->else_if_branches);
        if (ifs->else_branch) u__free_node(ifs->else_branch);
        u__free_node(node);
        return replacement;
    } else {
        for (size_t i = 0; i < ifs->else_if_count; i++) {
            ASTNode* cond = ifs->else_if_conditions[i];
            if (cond->type == NODE_LITERAL) {
                bool val;
                if (get_bool_value((LiteralExpr*)cond, &val) && val) {
                    ASTNode* replacement = ifs->else_if_branches[i];
                    ifs->else_if_branches[i] = NULL;
                    u__free_node(ifs->condition);
                    for (size_t j = 0; j < ifs->else_if_count; j++) {
                        if (j != i) {
                            u__free_node(ifs->else_if_conditions[j]);
                            u__free_node(ifs->else_if_branches[j]);
                        }
                    }
                    free(ifs->else_if_conditions);
                    free(ifs->else_if_branches);
                    if (ifs->else_branch) u__free_node(ifs->else_branch);
                    u__free_node(node);
                    return replacement;
                }
            }
        }
        if (ifs->else_branch) {
            ASTNode* replacement = ifs->else_branch;
            ifs->else_branch = NULL;
            u__free_node(ifs->condition);
            for (size_t i = 0; i < ifs->else_if_count; i++) {
                u__free_node(ifs->else_if_conditions[i]);
                u__free_node(ifs->else_if_branches[i]);
            }
            free(ifs->else_if_conditions);
            free(ifs->else_if_branches);
            u__free_node(node);
            return replacement;
        } else {
            u__free_node(ifs->condition);
            for (size_t i = 0; i < ifs->else_if_count; i++) {
                u__free_node(ifs->else_if_conditions[i]);
                u__free_node(ifs->else_if_branches[i]);
            }
            free(ifs->else_if_conditions);
            free(ifs->else_if_branches);
            u__free_node(node);
            return NULL;
        }
    }
}

static bool simplify_block_dce(Block** block_ptr, bool* changed) {
    if (!block_ptr || !*block_ptr) return false;
    Block* block = *block_ptr;
    bool local_changed = false;

    size_t write = 0;
    bool reachable = true;

    for (size_t i = 0; i < block->stmt_count; i++) {
        ASTNode* stmt = block->statements[i];

        if (!reachable) {
            u__free_node(stmt);
            local_changed = true;
            continue;
        }

        if (stmt->type == NODE_BLOCK) {
            if (simplify_block_dce((Block**)&stmt, changed)) {
                local_changed = true;
            }
            if (stmt->type == NODE_BLOCK && ((Block*)stmt)->stmt_count == 0) {
                u__free_node(stmt);
                local_changed = true;
                continue;
            }
        }

        if (stmt->type == NODE_IF_STMT) {
            ASTNode* new_stmt = simplify_if_chain(stmt, NULL);
            if (new_stmt != stmt) {
                local_changed = true;
                stmt = new_stmt;
                if (stmt) {
                    if (stmt->type == NODE_BLOCK) {
                        Block* inner = (Block*)stmt;
                        for (size_t j = 0; j < inner->stmt_count; j++) {
                            block->statements[write++] = inner->statements[j];
                        }
                        inner->stmt_count = 0;
                        u__free_node(stmt);
                        continue;
                    } else {
                        block->statements[write++] = stmt;
                    }
                }
                continue;
            }
        }

        bool terminates = false;
        if (stmt->type == NODE_RETURN_STMT || stmt->type == NODE_BREAK_STMT || stmt->type == NODE_CONTINUE_STMT) {
            terminates = true;
        }

        if (stmt->type == NODE_PASS_STMT) {
            u__free_node(stmt);
            local_changed = true;
            continue;
        }

        block->statements[write++] = stmt;

        if (terminates) {
            reachable = false;
        }
    }

    if (write < block->stmt_count) {
        block->stmt_count = write;
        local_changed = true;
    }

    if (local_changed && changed) *changed = true;
    return local_changed;
}

bool pass_dead_code_elimination(Program* ast) {
    if (!ast) return false;
    bool changed = false;

    for (size_t i = 0; i < ast->item_count; i++) {
        ASTNode* item = ast->items[i];
        if (item->type == NODE_FUNC_DECL) {
            FuncDecl* fd = (FuncDecl*)item;
            if (fd->body) {
                if (simplify_block_dce((Block**)&fd->body, &changed)) {
                    changed = true;
                }
            }
        } else if (item->type == NODE_BLOCK) {
            if (simplify_block_dce((Block**)&ast->items[i], &changed)) {
                changed = true;
            }
        }
    }

    if (changed) DEBUG_PRINT("Dead code elimination: applied simplifications.\n");
    return changed;
}

/* Remove unused local variables */

/*
 * Collect all identifiers that are read (used) in a block.
 * This is a simple analysis that traverses the AST and records identifiers
 * that appear in reading contexts (not on the left side of assignment).
 */
static void collect_used_identifiers(ASTNode* node, VarMap* used_map) {
    if (!node) return;
    switch (node->type) {
        case NODE_IDENT: {
            IdentExpr* id = (IdentExpr*)node;
            /* Insert with dummy value (kind does not matter) */
            ConstValue dummy = { CV_UNKNOWN, NULL };
            var_map_insert(used_map, id->name, dummy);
            break;
        }
        case NODE_BINARY: {
            BinaryExpr* bin = (BinaryExpr*)node;
            collect_used_identifiers(bin->left, used_map);
            collect_used_identifiers(bin->right, used_map);
            break;
        }
        case NODE_UNARY: {
            UnaryExpr* un = (UnaryExpr*)node;
            collect_used_identifiers(un->operand, used_map);
            break;
        }
        case NODE_ASSIGN: {
            AssignExpr* as = (AssignExpr*)node;
            /* Left side is a write, not a read, except for compound assigns where left is also read */
            if (as->op != ASSIGN_NORMAL) {
                collect_used_identifiers(as->left, used_map);
            }
            collect_used_identifiers(as->right, used_map);
            break;
        }
        case NODE_CALL: {
            CallExpr* call = (CallExpr*)node;
            collect_used_identifiers(call->callee, used_map);
            for (size_t i = 0; i < call->arg_count; i++) {
                collect_used_identifiers(call->args[i], used_map);
            }
            break;
        }
        case NODE_MEMBER_ACCESS: {
            MemberAccess* ma = (MemberAccess*)node;
            collect_used_identifiers(ma->object, used_map);
            break;
        }
        case NODE_INDEX: {
            IndexExpr* idx = (IndexExpr*)node;
            collect_used_identifiers(idx->array, used_map);
            collect_used_identifiers(idx->index, used_map);
            break;
        }
        case NODE_RETURN_STMT: {
            ReturnStmt* ret = (ReturnStmt*)node;
            collect_used_identifiers(ret->value, used_map);
            break;
        }
        case NODE_EXPR_STMT: {
            ExprStmt* es = (ExprStmt*)node;
            collect_used_identifiers(es->expr, used_map);
            break;
        }
        case NODE_IF_STMT: {
            IfStmt* ifs = (IfStmt*)node;
            collect_used_identifiers(ifs->condition, used_map);
            collect_used_identifiers(ifs->then_branch, used_map);
            for (size_t i = 0; i < ifs->else_if_count; i++) {
                collect_used_identifiers(ifs->else_if_conditions[i], used_map);
                collect_used_identifiers(ifs->else_if_branches[i], used_map);
            }
            collect_used_identifiers(ifs->else_branch, used_map);
            break;
        }
        case NODE_WHILE_STMT: {
            WhileStmt* ws = (WhileStmt*)node;
            collect_used_identifiers(ws->condition, used_map);
            collect_used_identifiers(ws->body, used_map);
            break;
        }
        case NODE_DO_WHILE_STMT: {
            DoWhileStmt* dw = (DoWhileStmt*)node;
            collect_used_identifiers(dw->body, used_map);
            collect_used_identifiers(dw->condition, used_map);
            break;
        }
        case NODE_FOR_STMT: {
            ForStmt* fs = (ForStmt*)node;
            collect_used_identifiers(fs->range, used_map);
            collect_used_identifiers(fs->body, used_map);
            break;
        }
        case NODE_BLOCK: {
            Block* block = (Block*)node;
            for (size_t i = 0; i < block->stmt_count; i++) {
                collect_used_identifiers(block->statements[i], used_map);
            }
            break;
        }
        default:
            break;
    }
}

/*
 * Remove declarations of local variables that are never used (read).
 * Scans each function and deletes let declarations whose variable is not in the used set.
 */
static bool remove_unused_vars_in_function(FuncDecl* fd, bool* changed) {
    if (!fd || !fd->body) return false;
    VarMap* used_map = var_map_create();
    if (!used_map) return false;
    collect_used_identifiers((ASTNode*)fd->body, used_map);

    bool local_changed = false;
    Block* body = (Block*)fd->body;
    size_t write = 0;
    for (size_t i = 0; i < body->stmt_count; i++) {
        ASTNode* stmt = body->statements[i];
        if (stmt->type == NODE_LET_DECL) {
            LetDecl* ld = (LetDecl*)stmt;
            bool keep = false;
            for (size_t j = 0; j < ld->count; j++) {
                if (var_map_lookup(used_map, ld->names[j]) != NULL) {
                    keep = true;
                    break;
                }
            }
            if (!keep) {
                u__free_node(stmt);
                local_changed = true;
                continue;
            }
        }
        body->statements[write++] = stmt;
    }
    if (write < body->stmt_count) {
        body->stmt_count = write;
        local_changed = true;
    }

    var_map_free(used_map);
    if (local_changed && changed) *changed = true;
    return local_changed;
}

bool pass_remove_unused_vars(Program* ast) {
    if (!ast) return false;
    bool changed = false;
    for (size_t i = 0; i < ast->item_count; i++) {
        if (ast->items[i]->type == NODE_FUNC_DECL) {
            if (remove_unused_vars_in_function((FuncDecl*)ast->items[i], &changed)) {
                changed = true;
            }
        }
    }
    if (changed) DEBUG_PRINT("Removed unused variables.\n");
    return changed;
}

/* Interprocedural constant propagation (simple case) */

/*
 * Try to evaluate a function body that consists of a single return expression.
 * If all arguments are constants, compute the result.
 * For simplicity, we only handle functions with one parameter and body "return a + b" etc.
 */
static bool try_evaluate_simple_function(FuncDecl* fd, ASTNode** args, size_t arg_count, int64_t* result) {
    if (!fd || !fd->body) return false;
    /* Check that the body is a block with a single return statement */
    Block* block = (Block*)fd->body;
    if (block->stmt_count != 1) return false;
    ASTNode* stmt = block->statements[0];
    if (stmt->type != NODE_RETURN_STMT) return false;
    ReturnStmt* ret = (ReturnStmt*)stmt;
    ASTNode* expr = ret->value;
    if (!expr) return false;

    /* We only handle binary operations with two operands that are identifiers or literals */
    if (expr->type != NODE_BINARY) return false;
    BinaryExpr* bin = (BinaryExpr*)expr;
    if (arg_count != 2) return false; /* assume two parameters */

    /* Map parameters to constants */
    if (fd->param_count != 2) return false;
    Param* p1 = (Param*)fd->params[0];
    Param* p2 = (Param*)fd->params[1];
    ASTNode* arg1 = args[0];
    ASTNode* arg2 = args[1];
    if (arg1->type != NODE_LITERAL || arg2->type != NODE_LITERAL) return false;

    long long lv, rv;
    double lf, rf;
    bool l_float, r_float;
    if (!get_number_value((LiteralExpr*)arg1, &lv, &lf, &l_float)) return false;
    if (!get_number_value((LiteralExpr*)arg2, &rv, &rf, &r_float)) return false;

    /* Check that the expression uses the parameters */
    bool left_is_param = false, right_is_param = false;
    if (bin->left->type == NODE_IDENT) {
        IdentExpr* id = (IdentExpr*)bin->left;
        if (strcmp(id->name, p1->name) == 0) left_is_param = true;
        else if (strcmp(id->name, p2->name) == 0) left_is_param = true;
    }
    if (bin->right->type == NODE_IDENT) {
        IdentExpr* id = (IdentExpr*)bin->right;
        if (strcmp(id->name, p1->name) == 0) right_is_param = true;
        else if (strcmp(id->name, p2->name) == 0) right_is_param = true;
    }
    if (!left_is_param || !right_is_param) return false;

    /* Now evaluate the operation */
    bool use_float = l_float || r_float;
    long long int_result = 0;
    double float_result = 0.0;
    bool ok = true;

    switch (bin->op) {
        case BIN_ADD:
            if (use_float) {
                float_result = (l_float ? lf : (double)lv) + (r_float ? rf : (double)rv);
            } else {
                if (check_overflow_add(lv, rv, &int_result)) ok = false;
            }
            break;
        case BIN_SUB:
            if (use_float) {
                float_result = (l_float ? lf : (double)lv) - (r_float ? rf : (double)rv);
            } else {
                if (check_overflow_sub(lv, rv, &int_result)) ok = false;
            }
            break;
        case BIN_MUL:
            if (use_float) {
                float_result = (l_float ? lf : (double)lv) * (r_float ? rf : (double)rv);
            } else {
                if (check_overflow_mul(lv, rv, &int_result)) ok = false;
            }
            break;
        case BIN_DIV:
            if (use_float) {
                double divisor = r_float ? rf : (double)rv;
                if (divisor == 0.0) ok = false;
                else float_result = (l_float ? lf : (double)lv) / divisor;
            } else {
                if (rv == 0) ok = false;
                else int_result = lv / rv;
            }
            break;
        default:
            ok = false;
            break;
    }

    if (!ok) return false;
    if (use_float) {
        /* We only return integer result for now; convert float to int if within range */
        if (float_result < LLONG_MIN || float_result > LLONG_MAX) return false;
        *result = (long long)float_result;
    } else {
        *result = int_result;
    }
    return true;
}

bool pass_interprocedural_const_prop(Program* ast) {
    if (!ast) return false;
    bool changed = false;
    /* For simplicity, scan all functions and look for calls that can be inlined */
    /* We'll just check top-level items for call expressions */
    /* SemanticContext ctx = {0};  -- unused, removed */
    /* We'll use u__replace_expressions with a custom fold that detects calls to simple functions */
    /* For simplicity, we don't implement full replacement here; just a stub */
    (void)ast;
    DEBUG_PRINT("Interprocedural constant propagation stub.\n");
    return changed;
}

/* Loop invariant hoisting (stub) */

bool pass_loop_invariant_hoisting(Program* ast) {
    if (!ast) return false;
    DEBUG_PRINT("Loop invariant hoisting stub.\n");
    return false;
}

/* Dead assignment elimination (stub) */

bool pass_dead_assignment_elimination(Program* ast) {
    if (!ast) return false;
    DEBUG_PRINT("Dead assignment elimination stub.\n");
    return false;
}
