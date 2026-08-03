#ifndef SEMANTIC_H
#define SEMANTIC_H

#include <stdbool.h>
#include <stdint.h>

#include "../parser/parser.h"
#include "symbol.h"
#include "type.h"

/*
 * Function pointer type for resolving module names to scopes.
 * Used by 'use' declarations to import symbols from other modules.
 */
typedef struct Scope* (*ModuleResolver)(const char* module_name);

/*
 * Label stack entry.
 */
typedef struct LabelEntry {
    char* name;
    bool is_loop;
    struct LabelEntry* next;
} LabelEntry;

/*
 * Defer stack entry.
 */
typedef struct DeferEntry {
    ASTNode* stmt;
    struct DeferEntry* next;
} DeferEntry;

/*
 * Semantic analysis context.
 * Holds all state during the analysis of a translation unit.
 */
typedef struct SemanticContext {
    Scope* current_scope;
    Scope* global_scope;
    Scope* file_scope;
    bool in_function;
    bool in_proc;
    bool in_loop;
    bool in_match;
    Type* return_type;
    bool return_has_error;
    const char* current_func_name;
    ModuleResolver resolver;
    bool extra_warnings;

    bool has_return;
    DeferEntry* defer_stack;
    LabelEntry* label_stack;
} SemanticContext;

/*
 * Create a new semantic analysis context.
 * Returns a pointer to the allocated context, or NULL on failure.
 */
SemanticContext* semantic__create_context(void);

/*
 * Destroy a semantic analysis context and free all associated resources.
 */
void semantic__destroy_context(SemanticContext* ctx);

/*
 * Enable or disable extra warnings for the context.
 */
void semantic__set_extra_warnings(SemanticContext* ctx, bool enable);

/*
 * Perform semantic analysis on the given AST.
 * Returns 0 on success, non‑zero on error.
 */
int semantic__analyze(Program* ast, ModuleResolver resolver);

/*
 * Retrieve the global scope from the semantic context.
 */
Scope* semantic__get_global_scope(SemanticContext* ctx);

/* Statement analysis functions */
bool analyze_block(SemanticContext* ctx, Block* block);
bool analyze_if(SemanticContext* ctx, IfStmt* ifstmt);
bool analyze_match(SemanticContext* ctx, MatchStmt* match);
bool analyze_while(SemanticContext* ctx, WhileStmt* while_stmt);
bool analyze_do_while(SemanticContext* ctx, DoWhileStmt* do_while);
bool analyze_for(SemanticContext* ctx, ForStmt* for_stmt);
bool analyze_break(SemanticContext* ctx, BreakStmt* brk);
bool analyze_continue(SemanticContext* ctx, ContinueStmt* cont);
bool analyze_return(SemanticContext* ctx, ReturnStmt* ret);
bool analyze_defer(SemanticContext* ctx, DeferStmt* defer);
bool analyze_receive(SemanticContext* ctx, ReceiveStmt* receive);
bool analyze_asm(SemanticContext* ctx, AsmStmt* asm_stmt);
bool analyze_volatile(SemanticContext* ctx, VolatileStmt* volatile_stmt);
bool analyze_comptime_statement(SemanticContext* ctx, ComptimeStmt* comp);
bool analyze_pass(SemanticContext* ctx, PassStmt* pass);
bool analyze_expr_statement(SemanticContext* ctx, ExprStmt* expr_stmt);
bool analyze_free_stmt(SemanticContext* ctx, FreeStmt* free_stmt);

/* Declaration analysis functions */
bool analyze_module_declaration(SemanticContext* ctx, ModuleDecl* mod);
bool analyze_use_declaration(SemanticContext* ctx, UseDecl* use);
bool analyze_func_declaration(SemanticContext* ctx, FuncDecl* func);
bool analyze_let_declaration(SemanticContext* ctx, LetDecl* let);
bool analyze_comptime_declaration(SemanticContext* ctx, ComptimeDecl* comp);
bool analyze_extern_declaration(SemanticContext* ctx, ExternDecl* ext);
bool analyze_proc_declaration(SemanticContext* ctx, ProcDecl* proc);

/* Expression analysis */
Type* analyze_expression(SemanticContext* ctx, ASTNode* node);

/* Control‑flow analysis */
bool analyze_control_flow(SemanticContext* ctx);

#endif
