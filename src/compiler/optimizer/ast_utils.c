#include "ast_utils.h"
#include <stdlib.h>
#include <string.h>

/* ----- Free functions for each node type ----- */

static void free_program(Program* prog) {
    if (!prog) return;
    for (size_t i = 0; i < prog->item_count; ++i) {
        u__free_node(prog->items[i]);
    }
    free(prog);
}

static void free_module(ModuleDecl* md) {
    if (!md) return;
    free(md->name);
    for (size_t i = 0; i < md->body_count; ++i) {
        u__free_node(md->body[i]);
    }
    free(md);
}

static void free_func_decl(FuncDecl* fd) {
    if (!fd) return;
    free(fd->name);
    for (size_t i = 0; i < fd->param_count; ++i) {
        u__free_node(fd->params[i]);
    }
    free(fd->params);
    u__free_node(fd->return_type);
    u__free_node(fd->body);
    u__free_node(fd->expr_body);
    free(fd);
}

static void free_let_decl(LetDecl* ld) {
    if (!ld) return;
    for (size_t i = 0; i < ld->count; ++i) {
        free(ld->names[i]);
        u__free_node(ld->types[i]);
    }
    free(ld->names);
    free(ld->types);
    u__free_node(ld->initializer);
    u__free_node(ld->array_size);
    free(ld);
}

static void free_block(Block* block) {
    if (!block) return;
    for (size_t i = 0; i < block->stmt_count; ++i) {
        u__free_node(block->statements[i]);
    }
    free(block->statements);
    free(block);
}

static void free_if_stmt(IfStmt* ifs) {
    if (!ifs) return;
    u__free_node(ifs->condition);
    u__free_node(ifs->then_branch);
    for (size_t i = 0; i < ifs->else_if_count; ++i) {
        u__free_node(ifs->else_if_conditions[i]);
        u__free_node(ifs->else_if_branches[i]);
    }
    free(ifs->else_if_conditions);
    free(ifs->else_if_branches);
    u__free_node(ifs->else_branch);
    free(ifs);
}

static void free_while_stmt(WhileStmt* ws) {
    if (!ws) return;
    free(ws->label);
    u__free_node(ws->condition);
    u__free_node(ws->body);
    free(ws);
}

static void free_do_while_stmt(DoWhileStmt* dw) {
    if (!dw) return;
    free(dw->label);
    u__free_node(dw->body);
    u__free_node(dw->condition);
    free(dw);
}

static void free_for_stmt(ForStmt* fs) {
    if (!fs) return;
    free(fs->label);
    free(fs->var_name);
    u__free_node(fs->range);
    u__free_node(fs->var_type);
    u__free_node(fs->body);
    free(fs);
}

static void free_return_stmt(ReturnStmt* rs) {
    if (!rs) return;
    u__free_node(rs->value);
    free(rs);
}

static void free_defer_stmt(DeferStmt* ds) {
    if (!ds) return;
    u__free_node(ds->stmt);
    free(ds);
}

static void free_expr_stmt(ExprStmt* es) {
    if (!es) return;
    u__free_node(es->expr);
    free(es);
}

static void free_binary_expr(BinaryExpr* bin) {
    if (!bin) return;
    u__free_node(bin->left);
    u__free_node(bin->right);
    free(bin);
}

static void free_unary_expr(UnaryExpr* un) {
    if (!un) return;
    u__free_node(un->operand);
    free(un);
}

static void free_assign_expr(AssignExpr* as) {
    if (!as) return;
    u__free_node(as->left);
    u__free_node(as->right);
    free(as);
}

static void free_send_expr(SendExpr* se) {
    if (!se) return;
    u__free_node(se->target);
    u__free_node(se->message);
    free(se);
}

static void free_call_expr(CallExpr* ce) {
    if (!ce) return;
    u__free_node(ce->callee);
    for (size_t i = 0; i < ce->arg_count; ++i) {
        u__free_node(ce->args[i]);
    }
    free(ce->args);
    free(ce);
}

static void free_member_access(MemberAccess* ma) {
    if (!ma) return;
    u__free_node(ma->object);
    free(ma->member);
    free(ma);
}

static void free_index_expr(IndexExpr* ie) {
    if (!ie) return;
    u__free_node(ie->array);
    u__free_node(ie->index);
    free(ie);
}

static void free_slice_expr(SliceExpr* sl) {
    if (!sl) return;
    u__free_node(sl->array);
    u__free_node(sl->range);
    free(sl);
}

static void free_struct_init(StructInitExpr* si) {
    if (!si) return;
    u__free_node(si->struct_type);
    for (size_t i = 0; i < si->count; ++i) {
        u__free_node(si->values[i]);
        free(si->field_names[i]);
    }
    free(si->field_names);
    free(si->values);
    free(si);
}

static void free_range_expr(RangeExpr* re) {
    if (!re) return;
    u__free_node(re->start);
    u__free_node(re->end);
    free(re);
}

static void free_if_expr(IfExpr* ie) {
    if (!ie) return;
    u__free_node(ie->condition);
    u__free_node(ie->then_expr);
    u__free_node(ie->else_expr);
    free(ie);
}

static void free_try_expr(TryExpr* te) {
    if (!te) return;
    u__free_node(te->expr);
    free(te);
}

static void free_catch_expr(CatchExpr* ce) {
    if (!ce) return;
    u__free_node(ce->expr);
    free(ce->var_name);
    u__free_node(ce->handler);
    free(ce);
}

static void free_error_expr(ErrorExpr* ee) {
    if (!ee) return;
    u__free_node(ee->code);
    free(ee);
}

static void free_spawn_expr(SpawnExpr* se) {
    if (!se) return;
    free(se->proc_name);
    u__free_node(se->proc_body);
    free(se);
}

static void free_self_expr(SelfExpr* se) {
    free(se);
}

static void free_interpolated_string(InterpolatedString* is) {
    if (!is) return;
    for (size_t i = 0; i < is->segment_count; ++i) {
        InterpolatedSegment* seg = &is->segments[i];
        if (seg->kind == SEG_TEXT) free(seg->text);
        else u__free_node(seg->expr);
    }
    free(is->segments);
    free(is);
}

static void free_cast_expr(CastExpr* ce) {
    if (!ce) return;
    u__free_node(ce->expr);
    u__free_node(ce->target_type);
    free(ce);
}

static void free_tuple_literal(TupleLiteral* tl) {
    if (!tl) return;
    for (size_t i = 0; i < tl->count; ++i) {
        u__free_node(tl->elements[i]);
    }
    free(tl->elements);
    free(tl);
}

static void free_literal(LiteralExpr* lit) {
    if (!lit) return;
    free(lit->raw_value);
    if (lit->kind == LIT_STRING || lit->kind == LIT_CHAR)
        free(lit->string_val);
    free(lit);
}

/* Main free function – dispatches based on node type. */
void u__free_node(ASTNode* node) {
    if (!node) return;
    switch (node->type) {
        case NODE_PROGRAM:          free_program((Program*)node); break;
        case NODE_MODULE:           free_module((ModuleDecl*)node); break;
        case NODE_FUNC_DECL:        free_func_decl((FuncDecl*)node); break;
        case NODE_LET_DECL:         free_let_decl((LetDecl*)node); break;
        case NODE_BLOCK:            free_block((Block*)node); break;
        case NODE_IF_STMT:          free_if_stmt((IfStmt*)node); break;
        case NODE_WHILE_STMT:       free_while_stmt((WhileStmt*)node); break;
        case NODE_DO_WHILE_STMT:    free_do_while_stmt((DoWhileStmt*)node); break;
        case NODE_FOR_STMT:         free_for_stmt((ForStmt*)node); break;
        case NODE_RETURN_STMT:      free_return_stmt((ReturnStmt*)node); break;
        case NODE_DEFER_STMT:       free_defer_stmt((DeferStmt*)node); break;
        case NODE_EXPR_STMT:        free_expr_stmt((ExprStmt*)node); break;
        case NODE_BINARY:           free_binary_expr((BinaryExpr*)node); break;
        case NODE_UNARY:            free_unary_expr((UnaryExpr*)node); break;
        case NODE_ASSIGN:           free_assign_expr((AssignExpr*)node); break;
        case NODE_SEND:             free_send_expr((SendExpr*)node); break;
        case NODE_CALL:             free_call_expr((CallExpr*)node); break;
        case NODE_MEMBER_ACCESS:    free_member_access((MemberAccess*)node); break;
        case NODE_INDEX:            free_index_expr((IndexExpr*)node); break;
        case NODE_SLICE:            free_slice_expr((SliceExpr*)node); break;
        case NODE_STRUCT_INIT:      free_struct_init((StructInitExpr*)node); break;
        case NODE_RANGE:            free_range_expr((RangeExpr*)node); break;
        case NODE_IF_EXPR:          free_if_expr((IfExpr*)node); break;
        case NODE_TRY_EXPR:         free_try_expr((TryExpr*)node); break;
        case NODE_CATCH_EXPR:       free_catch_expr((CatchExpr*)node); break;
        case NODE_ERROR_EXPR:       free_error_expr((ErrorExpr*)node); break;
        case NODE_SPAWN_EXPR:       free_spawn_expr((SpawnExpr*)node); break;
        case NODE_SELF_EXPR:        free_self_expr((SelfExpr*)node); break;
        case NODE_INTERPOLATED_STRING: free_interpolated_string((InterpolatedString*)node); break;
        case NODE_CAST:             free_cast_expr((CastExpr*)node); break;
        case NODE_TUPLE_LITERAL:    free_tuple_literal((TupleLiteral*)node); break;
        case NODE_LITERAL:          free_literal((LiteralExpr*)node); break;
        default:
            free(node);
            break;
    }
}

/* ----- Traversal and replacement ----- */

/* Forward declarations for recursive traversal helpers. */
static void traverse_replace(ASTNode** node_ptr,
                             ASTNode* (*fold)(ASTNode*, SemanticContext*),
                             SemanticContext* ctx);

/* Recursively traverse children of a statement node. */
static void traverse_statement_children(ASTNode* stmt,
                                        ASTNode* (*fold)(ASTNode*, SemanticContext*),
                                        SemanticContext* ctx) {
    if (!stmt) return;
    switch (stmt->type) {
        case NODE_BLOCK: {
            Block* block = (Block*)stmt;
            for (size_t i = 0; i < block->stmt_count; ++i) {
                traverse_replace(&block->statements[i], fold, ctx);
            }
            break;
        }
        case NODE_IF_STMT: {
            IfStmt* ifs = (IfStmt*)stmt;
            traverse_replace(&ifs->condition, fold, ctx);
            traverse_replace(&ifs->then_branch, fold, ctx);
            for (size_t i = 0; i < ifs->else_if_count; ++i) {
                traverse_replace(&ifs->else_if_conditions[i], fold, ctx);
                traverse_replace(&ifs->else_if_branches[i], fold, ctx);
            }
            traverse_replace(&ifs->else_branch, fold, ctx);
            break;
        }
        case NODE_WHILE_STMT: {
            WhileStmt* ws = (WhileStmt*)stmt;
            traverse_replace(&ws->condition, fold, ctx);
            traverse_replace(&ws->body, fold, ctx);
            break;
        }
        case NODE_DO_WHILE_STMT: {
            DoWhileStmt* dw = (DoWhileStmt*)stmt;
            traverse_replace(&dw->body, fold, ctx);
            traverse_replace(&dw->condition, fold, ctx);
            break;
        }
        case NODE_FOR_STMT: {
            ForStmt* fs = (ForStmt*)stmt;
            traverse_replace(&fs->range, fold, ctx);
            traverse_replace(&fs->var_type, fold, ctx);
            traverse_replace(&fs->body, fold, ctx);
            break;
        }
        case NODE_RETURN_STMT: {
            ReturnStmt* rs = (ReturnStmt*)stmt;
            traverse_replace(&rs->value, fold, ctx);
            break;
        }
        case NODE_DEFER_STMT: {
            DeferStmt* ds = (DeferStmt*)stmt;
            traverse_replace(&ds->stmt, fold, ctx);
            break;
        }
        case NODE_EXPR_STMT: {
            ExprStmt* es = (ExprStmt*)stmt;
            traverse_replace(&es->expr, fold, ctx);
            break;
        }
        default:
            break;
    }
}

/* Recursively traverse children of an expression node. */
static void traverse_expr_children(ASTNode* expr,
                                   ASTNode* (*fold)(ASTNode*, SemanticContext*),
                                   SemanticContext* ctx) {
    if (!expr) return;
    switch (expr->type) {
        case NODE_BINARY: {
            BinaryExpr* bin = (BinaryExpr*)expr;
            traverse_replace(&bin->left, fold, ctx);
            traverse_replace(&bin->right, fold, ctx);
            break;
        }
        case NODE_UNARY: {
            UnaryExpr* un = (UnaryExpr*)expr;
            traverse_replace(&un->operand, fold, ctx);
            break;
        }
        case NODE_ASSIGN: {
            AssignExpr* as = (AssignExpr*)expr;
            traverse_replace(&as->left, fold, ctx);
            traverse_replace(&as->right, fold, ctx);
            break;
        }
        case NODE_SEND: {
            SendExpr* se = (SendExpr*)expr;
            traverse_replace(&se->target, fold, ctx);
            traverse_replace(&se->message, fold, ctx);
            break;
        }
        case NODE_CALL: {
            CallExpr* ce = (CallExpr*)expr;
            traverse_replace(&ce->callee, fold, ctx);
            for (size_t i = 0; i < ce->arg_count; ++i) {
                traverse_replace(&ce->args[i], fold, ctx);
            }
            break;
        }
        case NODE_MEMBER_ACCESS: {
            MemberAccess* ma = (MemberAccess*)expr;
            traverse_replace(&ma->object, fold, ctx);
            break;
        }
        case NODE_INDEX: {
            IndexExpr* ie = (IndexExpr*)expr;
            traverse_replace(&ie->array, fold, ctx);
            traverse_replace(&ie->index, fold, ctx);
            break;
        }
        case NODE_SLICE: {
            SliceExpr* sl = (SliceExpr*)expr;
            traverse_replace(&sl->array, fold, ctx);
            traverse_replace(&sl->range, fold, ctx);
            break;
        }
        case NODE_STRUCT_INIT: {
            StructInitExpr* si = (StructInitExpr*)expr;
            traverse_replace(&si->struct_type, fold, ctx);
            for (size_t i = 0; i < si->count; ++i) {
                traverse_replace(&si->values[i], fold, ctx);
            }
            break;
        }
        case NODE_RANGE: {
            RangeExpr* re = (RangeExpr*)expr;
            traverse_replace(&re->start, fold, ctx);
            traverse_replace(&re->end, fold, ctx);
            break;
        }
        case NODE_IF_EXPR: {
            IfExpr* ie = (IfExpr*)expr;
            traverse_replace(&ie->condition, fold, ctx);
            traverse_replace(&ie->then_expr, fold, ctx);
            traverse_replace(&ie->else_expr, fold, ctx);
            break;
        }
        case NODE_TRY_EXPR: {
            TryExpr* te = (TryExpr*)expr;
            traverse_replace(&te->expr, fold, ctx);
            break;
        }
        case NODE_CATCH_EXPR: {
            CatchExpr* ce = (CatchExpr*)expr;
            traverse_replace(&ce->expr, fold, ctx);
            traverse_replace(&ce->handler, fold, ctx);
            break;
        }
        case NODE_ERROR_EXPR: {
            ErrorExpr* ee = (ErrorExpr*)expr;
            traverse_replace(&ee->code, fold, ctx);
            break;
        }
        case NODE_SPAWN_EXPR: {
            SpawnExpr* se = (SpawnExpr*)expr;
            traverse_replace(&se->proc_body, fold, ctx);
            break;
        }
        case NODE_INTERPOLATED_STRING: {
            InterpolatedString* is = (InterpolatedString*)expr;
            for (size_t i = 0; i < is->segment_count; ++i) {
                if (is->segments[i].kind == SEG_EXPR) {
                    traverse_replace(&is->segments[i].expr, fold, ctx);
                }
            }
            break;
        }
        case NODE_CAST: {
            CastExpr* ce = (CastExpr*)expr;
            traverse_replace(&ce->expr, fold, ctx);
            traverse_replace(&ce->target_type, fold, ctx);
            break;
        }
        case NODE_TUPLE_LITERAL: {
            TupleLiteral* tl = (TupleLiteral*)expr;
            for (size_t i = 0; i < tl->count; ++i) {
                traverse_replace(&tl->elements[i], fold, ctx);
            }
            break;
        }
        default:
            break;
    }
}

/* Main traversal function: process children, then apply fold, and replace if needed. */
static void traverse_replace(ASTNode** node_ptr,
                             ASTNode* (*fold)(ASTNode*, SemanticContext*),
                             SemanticContext* ctx) {
    if (!node_ptr || !*node_ptr) return;
    ASTNode* node = *node_ptr;

    switch (node->type) {
        case NODE_PROGRAM: {
            Program* prog = (Program*)node;
            for (size_t i = 0; i < prog->item_count; ++i) {
                traverse_replace(&prog->items[i], fold, ctx);
            }
            break;
        }
        case NODE_MODULE: {
            ModuleDecl* md = (ModuleDecl*)node;
            for (size_t i = 0; i < md->body_count; ++i) {
                traverse_replace(&md->body[i], fold, ctx);
            }
            break;
        }
        case NODE_FUNC_DECL: {
            FuncDecl* fd = (FuncDecl*)node;
            for (size_t i = 0; i < fd->param_count; ++i) {
                traverse_replace(&fd->params[i], fold, ctx);
            }
            traverse_replace(&fd->return_type, fold, ctx);
            traverse_replace(&fd->body, fold, ctx);
            traverse_replace(&fd->expr_body, fold, ctx);
            break;
        }
        case NODE_LET_DECL: {
            LetDecl* ld = (LetDecl*)node;
            for (size_t i = 0; i < ld->count; ++i) {
                traverse_replace(&ld->types[i], fold, ctx);
            }
            traverse_replace(&ld->initializer, fold, ctx);
            traverse_replace(&ld->array_size, fold, ctx);
            break;
        }
        case NODE_BLOCK:
        case NODE_IF_STMT:
        case NODE_WHILE_STMT:
        case NODE_DO_WHILE_STMT:
        case NODE_FOR_STMT:
        case NODE_RETURN_STMT:
        case NODE_DEFER_STMT:
        case NODE_EXPR_STMT:
            traverse_statement_children(node, fold, ctx);
            break;
        default:
            traverse_expr_children(node, fold, ctx);
            break;
    }

    ASTNode* new_node = fold(node, ctx);
    if (new_node && new_node != node) {
        *node_ptr = new_node;
        u__free_node(node);
    } else if (new_node == NULL && new_node != node) {
        /* If fold returns NULL, remove the node entirely. */
        *node_ptr = NULL;
        u__free_node(node);
    }
}

/* Public API: start traversal from the given node pointer. */
void u__replace_expressions(ASTNode** node,
                            ASTNode* (*fold)(ASTNode*, SemanticContext*),
                            SemanticContext* ctx) {
    if (!node) return;
    traverse_replace(node, fold, ctx);
}
