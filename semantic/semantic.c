#include "semantic.h"
#include "../error_manager/error_manager.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static Scope *global_scope = NULL;
static Scope *current_scope = NULL;

// Add this forward declaration near the top of the file
static Type *analyze_expression(ASTNode *node);

// Static type objects for built-in types
static Type int_type = {"int", 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0};
static Type char_type = {"char", 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0};
static Type char_ptr_type = {"char", 1, 0, NULL, 0, 0, 0, 0, 0, 0, 0};
static Type void_type = {"void", 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0};
static Type real_type = {"real", 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0};
static Type va_list_type = {"va_list", 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0};
static Type reg_type = {"reg", 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0};

// Create a new symbol
static Symbol *create_symbol(char *name, SymbolKind kind, Type *type, unsigned int modifiers) {
    Symbol *symbol = malloc(sizeof(Symbol));
    if (!symbol) return NULL;
    symbol->name = malloc(strlen(name) + 1);
    if (!symbol->name) {
        free(symbol);
        return NULL;
    }
    strcpy(symbol->name, name);
    symbol->kind = kind;
    symbol->type = type;
    symbol->modifiers = modifiers;
    symbol->next = NULL;
    return symbol;
}

// Free a symbol
static void free_symbol(Symbol *symbol) {
    if (!symbol) return;
    free(symbol->name);
    free(symbol);
}

// Enter a new scope
static void enter_scope(ScopeKind kind) {
    Scope *new_scope = malloc(sizeof(Scope));
    if (!new_scope) return;
    new_scope->parent = current_scope;
    new_scope->symbols = NULL;
    new_scope->kind = kind;
    new_scope->is_variadic = false;
    current_scope = new_scope;
}

// Leave current scope
static void leave_scope(void) {
    if (!current_scope) return;
    Scope *parent = current_scope->parent;
    Symbol *symbol = current_scope->symbols;
    while (symbol) {
        Symbol *next = symbol->next;
        free_symbol(symbol);
        symbol = next;
    }
    free(current_scope);
    current_scope = parent;
}

// Add symbol to current scope
static bool add_symbol(Symbol *symbol) {
    if (!current_scope || !symbol) return false;
    symbol->next = current_scope->symbols;
    current_scope->symbols = symbol;
    return true;
}

// Find symbol in current and parent scopes
static Symbol *find_symbol(const char *name) {
    Scope *scope = current_scope;
    while (scope) {
        Symbol *symbol = scope->symbols;
        while (symbol) {
            if (strcmp(symbol->name, name) == 0)
                return symbol;
            symbol = symbol->next;
        }
        scope = scope->parent;
    }
    return NULL;
}

// Find symbol in specific scope
static Symbol *find_symbol_in_scope(Scope *scope, const char *name) {
    if (!scope) return NULL;
    Symbol *symbol = scope->symbols;
    while (symbol) {
        if (strcmp(symbol->name, name) == 0)
            return symbol;
        symbol = symbol->next;
    }
    return NULL;
}

// Get current function scope
static Scope *get_current_function_scope(void) {
    Scope *scope = current_scope;
    while (scope != NULL) {
        if (scope->kind == SCOPE_FUNCTION) {
            return scope;
        }
        scope = scope->parent;
    }
    return NULL;
}

// Check if inside variadic function
static bool is_inside_variadic_function(void) {
    Scope *func_scope = get_current_function_scope();
    return func_scope && func_scope->is_variadic;
}

// Check if two types are compatible
static bool type_compatible(Type *a, Type *b) {
    if (!a || !b) return false;
    if (strcmp(a->name, b->name) != 0) return false;
    if (a->pointer_level != b->pointer_level) return false;
    if (a->is_reference != b->is_reference) return false;
    return true;
}

// Check if type is void
static bool is_void_type(Type *type) {
    return type && strcmp(type->name, "void") == 0 && type->pointer_level == 0;
}

// Handle default void type
static Type *get_type_or_void(Type *type) {
    return type ? type : &void_type;
}

// Validate left number for supported types
static bool validate_left_number(Type *type, int left_number) {
    if (left_number < 0) return false;
    if (type->pointer_level > 0) return true;
    if (strcmp(type->name, "real") == 0) return true;
    if (strcmp(type->name, "int") == 0) return true;
    if (strcmp(type->name, "reg") == 0) return true;
    if (strcmp(type->name, "va_list") == 0) return true;
    if (strcmp(type->name, "char") == 0) return true;
    return false;
}

// Validate right number based on type
static bool validate_right_number(Type *type, int right_number) {
    if (right_number < 0) return false;
    if (type->pointer_level > 0) return true;
    if (strcmp(type->name, "void") == 0) return false;
    if (strcmp(type->name, "reg") == 0) return true;
    if (strcmp(type->name, "real") == 0) return true;
    if (strcmp(type->name, "int") == 0) return true;
    if (strcmp(type->name, "va_list") == 0) return true;
    if (strcmp(type->name, "char") == 0) return true;
    return false;
}

// Check for duplicate symbol in current scope
static bool check_duplicate_symbol(const char *name) {
    return find_symbol_in_scope(current_scope, name) != NULL;
}

// Check for duplicate global symbol
static bool check_duplicate_global_symbol(const char *name) {
    return find_symbol_in_scope(global_scope, name) != NULL;
}

// Check if type is numeric
static bool is_numeric_type(Type *type) {
    if (!type) return false;
    if (type->pointer_level > 0) return false;
    if (strcmp(type->name, "int") == 0) return true;
    if (strcmp(type->name, "real") == 0) return true;
    if (strcmp(type->name, "char") == 0) return true;
    if (strcmp(type->name, "reg") == 0) return true;
    return false;
}

// Check if cast is valid
static bool is_valid_cast(Type *from, Type *to) {
    if (type_compatible(from, to)) return true;
    if (is_numeric_type(from) && is_numeric_type(to)) return true;
    if (from->pointer_level > 0 && to->pointer_level > 0) {
        if (strcmp(from->name, "void") == 0 || strcmp(to->name, "void") == 0) {
            return true;
        }
        return false;
    }
    return false;
}

// Handle explicit type casting
static Type *handle_cast(Type *target_type, ASTNode *expr_node) {
    Type *expr_type = analyze_expression(expr_node);
    if (!target_type || !expr_type) return NULL;

    if (is_void_type(target_type) && target_type->pointer_level == 0) {
        report_error(0, 0, "Cannot cast to void");
        return NULL;
    }

    if (!is_valid_cast(expr_type, target_type)) {
        report_error(0, 0, "Invalid cast from %s to %s", expr_type->name, target_type->name);
        return NULL;
    }

    return target_type;
}

// Semantic analysis for expressions
static Type *analyze_expression(ASTNode *node) {
    if (!node) return NULL;
    
    switch (node->type) {
        case AST_IDENTIFIER: {
            Symbol *symbol = find_symbol(node->value);
            if (!symbol) {
                report_error(0, 0, "Undefined variable: %s", node->value);  
                return NULL;
            }
            return symbol->type;
        }
        case AST_LITERAL_VALUE:
            if (node->operation_type == TOKEN_NUMBER) {
                if (strchr(node->value, '.') != NULL) {
                    return &real_type;
                } else {
                    return &int_type;
                }
            } else if (node->operation_type == TOKEN_STRING) {
                return &char_ptr_type;
            } else if (node->operation_type == TOKEN_CHAR) {
                return &char_type;
            }
            return NULL;
        case AST_BINARY_OPERATION: {
            Type *left_type = analyze_expression(node->left);
            Type *right_type = analyze_expression(node->right);
            if (!left_type || !right_type) return NULL;
            
            if (!type_compatible(left_type, right_type)) {
                report_error(0, 0, "Type mismatch in binary operation");
                return NULL;
            }
            return left_type;
        }
        case AST_FUNCTION_CALL: {
            Symbol *func = find_symbol(node->value);
            if (!func) {
                report_error(0, 0, "Undefined function: %s", node->value);
                return NULL;
            }
            if (func->kind != SYMBOL_FUNCTION) {
                report_error(0, 0, "Not a function: %s", node->value);
                return NULL;
            }
            return func->type;
        }
        case AST_VA_ARG: {
            if (!is_inside_variadic_function()) {
                report_error(0, 0, "va_arg used in non-variadic function");
                return NULL;
            }
            Type *va_list_type_arg = analyze_expression(node->left);
            if (!va_list_type_arg || !type_compatible(va_list_type_arg, &va_list_type)) {
                report_error(0, 0, "First argument to va_arg must be of type va_list");
                return NULL;
            }
            if (!node->variable_type) {
                report_error(0, 0, "va_arg requires a type");
                return NULL;
            }
            if (is_void_type(node->variable_type)) {
                report_error(0, 0, "va_arg cannot be used with void type");
                return NULL;
            }
            return node->variable_type;
        }
        case AST_CAST: {
            return handle_cast(node->variable_type, node->left);
        }
        default:
            return NULL;
    }
}

// Check return statements in function
static bool check_function_returns(ASTNode *node, Type *return_type) {
    if (!node) return false;
    
    switch (node->type) {
        case AST_RETURN:
            if (node->left) {
                Type *expr_type = analyze_expression(node->left);
                if (!expr_type || !type_compatible(return_type, expr_type)) {
                    report_error(0, 0, "Return type mismatch");
                    return false;
                }
            } else if (!is_void_type(return_type)) {
                report_error(0, 0, "Non-void function must return a value");
                return false;
            }
            return true;
            
        case AST_BLOCK:
            if (node->extra) {
                AST *block_ast = (AST*)node->extra;
                for (int i = 0; i < block_ast->count; i++) {
                    if (check_function_returns(block_ast->nodes[i], return_type))
                        return true;
                }
            }
            return false;
            
        case AST_IF_STATEMENT:
            if (node->left && node->right) {
                bool then_returns = check_function_returns(node->left, return_type);
                bool else_returns = check_function_returns(node->right, return_type);
                return then_returns && else_returns;
            }
            return false;
            
        default:
            return false;
    }
}

// Semantic analysis for statements
static void analyze_statement(ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_VARIABLE_DECLARATION: {
            if (check_duplicate_symbol(node->value)) {
                report_error(0, 0, "Duplicate variable declaration: %s", node->value);
                return;
            }
            if (!node->variable_type) {
                report_error(0, 0, "Variable declaration without type");
                return;
            }
            Type *var_type = node->variable_type;
            
            if (is_void_type(var_type) && var_type->pointer_level == 0) {
                report_error(0, 0, "Variable cannot be of type void");
                return;
            }
            
            if (!validate_left_number(var_type, var_type->left_number)) {
                report_error(0, 0, "Invalid left number for type %s", var_type->name);
            }
            if (!validate_right_number(var_type, var_type->right_number)) {
                report_error(0, 0, "Invalid right number for type %s", var_type->name);
            }
            
            // Check const modifier
            if (0 && !node->left) {
                report_error(0, 0, "Const variable %s must be initialized", node->value);
            }
            
            if (node->left) {
                Type *expr_type = analyze_expression(node->left);
                if (expr_type && !type_compatible(var_type, expr_type)) {
                    report_error(0, 0, "Type mismatch in variable initialization");
                }
            }
            Symbol *symbol = create_symbol(node->value, SYMBOL_VARIABLE, var_type, 0);
            add_symbol(symbol);
            break;
        }
        case AST_ASSIGNMENT: {
            // Check if assigning to const variable
            if (node->left->type == AST_IDENTIFIER) {
                Symbol *symbol = find_symbol(node->left->value);
                if (symbol && (symbol->modifiers & MODIFIER_CONST)) {
                    report_error(0, 0, "Cannot assign to const variable %s", node->left->value);
                }
            }
            
            Type *left_type = analyze_expression(node->left);
            Type *right_type = analyze_expression(node->right);
            if (!left_type || !right_type) return;
            
            if (!type_compatible(left_type, right_type)) {
                report_error(0, 0, "Type mismatch in assignment");
            }
            break;
        }
        case AST_FUNCTION: {
            if (check_duplicate_global_symbol(node->value)) {
                report_error(0, 0, "Duplicate function declaration: %s", node->value);
                return;
            }
            Type *return_type = get_type_or_void(node->return_type);
            Symbol *symbol = create_symbol(node->value, SYMBOL_FUNCTION, return_type, 0);
            add_symbol(symbol);
            
            enter_scope(SCOPE_FUNCTION);
            current_scope->is_variadic = node->is_variadic;
            
            if (node->left && node->left->extra) {
                AST *params = (AST*)node->left->extra;
                for (int i = 0; i < params->count; i++) {
                    ASTNode *param = params->nodes[i];
                    if (check_duplicate_symbol(param->value)) {
                        report_error(0, 0, "Duplicate parameter name: %s", param->value);
                        continue;
                    }
                    if (!param->variable_type) {
                        report_error(0, 0, "Parameter without type");
                        continue;
                    }
                    Type *param_type = param->variable_type;
                    if (is_void_type(param_type) && param_type->pointer_level == 0) {
                        report_error(0, 0, "Parameter cannot be of type void");
                        continue;
                    }
                    Symbol *param_symbol = create_symbol(param->value, SYMBOL_PARAMETER, param_type, 0);
                    add_symbol(param_symbol);
                }
            }
            
            if (node->right) {
                analyze_statement(node->right);
                if (!is_void_type(return_type)) {
                    if (!check_function_returns(node->right, return_type)) {
                        report_error(0, 0, "Function %s must return a value", node->value);
                    }
                }
            }
            
            leave_scope();
            break;
        }
        case AST_STRUCT_DECLARATION: {
            if (check_duplicate_global_symbol(node->value)) {
                report_error(0, 0, "Duplicate struct declaration: %s", node->value);
                return;
            }
            Symbol *symbol = create_symbol(node->value, SYMBOL_STRUCT, NULL, 0);
            add_symbol(symbol);
            break;
        }
        case AST_BLOCK: {
            enter_scope(SCOPE_BLOCK);
            if (node->extra) {
                AST *block_ast = (AST*)node->extra;
                for (int i = 0; i < block_ast->count; i++)
                    analyze_statement(block_ast->nodes[i]);
            }
            leave_scope();
            break;
        }
        case AST_RETURN: {
            Scope *scope = current_scope;
            bool in_function = false;
            while (scope) {
                Symbol *symbol = scope->symbols;
                while (symbol) {
                    if (symbol->kind == SYMBOL_FUNCTION) {
                        in_function = true;
                        break;
                    }
                    symbol = symbol->next;
                }
                if (in_function) break;
                scope = scope->parent;
            }
            
            if (!in_function) {
                report_error(0, 0, "Return statement outside function");
            }
            break;
        }
        case AST_VA_START: {
            if (!is_inside_variadic_function()) {
                report_error(0, 0, "va_start used in non-variadic function");
                break;
            }
            Type *va_list_type_arg = analyze_expression(node->left);
            if (!va_list_type_arg || !type_compatible(va_list_type_arg, &va_list_type)) {
                report_error(0, 0, "First argument to va_start must be of type va_list");
            }
            if (node->right->type != AST_IDENTIFIER) {
                report_error(0, 0, "Second argument to va_start must be an identifier");
                break;
            }
            Scope *func_scope = get_current_function_scope();
            Symbol *param = find_symbol_in_scope(func_scope, node->right->value);
            if (!param || param->kind != SYMBOL_PARAMETER) {
                report_error(0, 0, "Second argument to va_start must be a parameter");
            }
            break;
        }
        case AST_VA_END: {
            if (!is_inside_variadic_function()) {
                report_error(0, 0, "va_end used in non-variadic function");
                break;
            }
            Type *va_list_type_arg = analyze_expression(node->left);
            if (!va_list_type_arg || !type_compatible(va_list_type_arg, &va_list_type)) {
                report_error(0, 0, "Argument to va_end must be of type va_list");
            }
            break;
        }
        default:
            break;
    }
}

void reset_semantic_analysis(void) {
    if (global_scope) {
        Symbol *symbol = global_scope->symbols;
        while (symbol) {
            Symbol *next = symbol->next;
            free(symbol->name);
            free(symbol);
            symbol = next;
        }
        free(global_scope);
        global_scope = NULL;
    }
    current_scope = NULL;
}

void print_symbol_table(Scope *scope) {
    if (!scope) return;
    
    printf("Symbol table:\n");
    Symbol *symbol = scope->symbols;
    while (symbol) {
        const char *kind_str;
        switch (symbol->kind) {
            case SYMBOL_VARIABLE: kind_str = "variable"; break;
            case SYMBOL_FUNCTION: kind_str = "function"; break;
            case SYMBOL_STRUCT: kind_str = "struct"; break;
            case SYMBOL_PARAMETER: kind_str = "parameter"; break;
            default: kind_str = "unknown"; break;
        }
        
        if (symbol->type) {
            printf("  %s: %s (type: %s, pointer_level: %d, left: %d, right: %d, modifiers: %u)\n", 
                   kind_str, symbol->name, symbol->type->name, symbol->type->pointer_level,
                   symbol->type->left_number, symbol->type->right_number, symbol->modifiers);
        } else {
            printf("  %s: %s (type: none, modifiers: %u)\n", kind_str, symbol->name, symbol->modifiers);
        }
        symbol = symbol->next;
    }
}

void semantic_analysis(AST *ast) {
    if (!ast) return;
    
    reset_semantic_analysis();
    
    global_scope = malloc(sizeof(Scope));
    global_scope->parent = NULL;
    global_scope->symbols = NULL;
    global_scope->kind = SCOPE_GLOBAL;
    global_scope->is_variadic = false;
    current_scope = global_scope;
    
    for (int i = 0; i < ast->count; i++) {
        analyze_statement(ast->nodes[i]);
    }
}

Scope *get_global_scope(void) {
    return global_scope;
}

