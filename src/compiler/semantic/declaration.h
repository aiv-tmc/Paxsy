#ifndef DECLARATION_H
#define DECLARATION_H

#include "../parser/parser.h"
#include "symbol.h"
#include "type.h"

struct SemanticContext;

bool analyze_module_declaration(struct SemanticContext* ctx, ModuleDecl* mod);
bool analyze_use_declaration(struct SemanticContext* ctx, UseDecl* use);
bool analyze_func_declaration(struct SemanticContext* ctx, FuncDecl* func);
bool analyze_let_declaration(struct SemanticContext* ctx, LetDecl* let);
bool analyze_comptime_declaration(struct SemanticContext* ctx, ComptimeDecl* comp);
bool analyze_extern_declaration(struct SemanticContext* ctx, ExternDecl* ext);
bool analyze_proc_declaration(struct SemanticContext* ctx, ProcDecl* proc);
bool analyze_control_flow(struct SemanticContext* ctx);

#endif
