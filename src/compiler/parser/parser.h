#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "../lexer/lexer.h"          /* provides Token */
#include "../semantic/type.h"        /* provides BasicTypeKind, FamilyTypeKind, Type */

/* Assignment operator kinds (must be defined before AssignExpr) */
typedef enum {
    ASSIGN_NORMAL,
    ASSIGN_ADD,
    ASSIGN_SUB,
    ASSIGN_MUL,
    ASSIGN_DIV,
    ASSIGN_MOD,
    ASSIGN_BIT_AND,
    ASSIGN_BIT_OR,
    ASSIGN_BIT_XOR,
    ASSIGN_SHL,
    ASSIGN_SHR,
    ASSIGN_SHL_LOG,
    ASSIGN_SHR_LOG,
    ASSIGN_OR,
    ASSIGN_AND,
    ASSIGN_XOR
} AssignOp;

/* Node types */
typedef enum {
    NODE_PROGRAM,
    NODE_MODULE,
    NODE_USE,
    NODE_FUNC_DECL,
    NODE_LET_DECL,
    NODE_COMPTIME_DECL,
    NODE_EXTERN_DECL,
    NODE_PROC_DECL,
    NODE_BLOCK,
    NODE_IF_STMT,
    NODE_MATCH_STMT,
    NODE_MATCH_CASE,
    NODE_WHILE_STMT,
    NODE_DO_WHILE_STMT,
    NODE_FOR_STMT,
    NODE_BREAK_STMT,
    NODE_CONTINUE_STMT,
    NODE_RETURN_STMT,
    NODE_DEFER_STMT,
    NODE_RECEIVE_STMT,
    NODE_RECEIVE_CASE,
    NODE_ASM_STMT,
    NODE_VOLATILE_STMT,
    NODE_COMPTIME_STMT,
    NODE_PASS_STMT,
    NODE_EXPR_STMT,
    NODE_IDENT,
    NODE_LITERAL,
    NODE_UNARY,
    NODE_BINARY,
    NODE_ASSIGN,
    NODE_SEND,
    NODE_CALL,
    NODE_MEMBER_ACCESS,
    NODE_MODULE_ACCESS,
    NODE_INDEX,
    NODE_SLICE,
    NODE_STRUCT_INIT,
    NODE_RANGE,
    NODE_IF_EXPR,
    NODE_TRY_EXPR,
    NODE_CATCH_EXPR,
    NODE_ERROR_EXPR,
    NODE_SPAWN_EXPR,
    NODE_SELF_EXPR,
    NODE_INTERPOLATED_STRING,
    NODE_CAST,
    NODE_TUPLE_LITERAL,
    NODE_FAMILY_TYPE,
    NODE_BASIC_TYPE,
    NODE_POINTER_TYPE,
    NODE_ARRAY_TYPE,
    NODE_TUPLE_TYPE,
    NODE_NULLABLE_TYPE,
    NODE_ERROR_TYPE,
    NODE_FUNC_TYPE,
    NODE_IDENT_TYPE,
    NODE_STRUCT_TYPE,
    NODE_UNION_TYPE,
    NODE_ENUM_TYPE,
    NODE_ENUM_CONST,
    NODE_PARAM,
    NODE_FIELD,
    NODE_PATTERN_LITERAL,
    NODE_PATTERN_TUPLE,
    NODE_PATTERN_CAPTURE,
    NODE_PATTERN_PLACEHOLDER,
    /* New nodes for arrays, alloc, free */
    NODE_ARRAY_LITERAL,
    NODE_ALLOC_EXPR,
    NODE_FREE_STMT
} NodeType;

/* Operator kinds */
typedef enum {
    UNARY_PLUS,
    UNARY_MINUS,
    UNARY_LOGICAL_NOT,
    UNARY_BIT_NOT,
    UNARY_ADDR,
    UNARY_DEREF
} UnaryOp;

typedef enum {
    BIN_ADD,
    BIN_SUB,
    BIN_MUL,
    BIN_DIV,
    BIN_MOD,
    BIN_EQ,
    BIN_NE,
    BIN_GT,
    BIN_LT,
    BIN_GE,
    BIN_LE,
    BIN_AND,
    BIN_OR,
    BIN_XOR,
    BIN_SHL,
    BIN_SHR,
    BIN_SHL_LOG,
    BIN_SHR_LOG,
    BIN_LOGICAL_AND,
    BIN_LOGICAL_OR
} BinaryOp;

/* Literal kinds */
typedef enum {
    LIT_NUMBER,
    LIT_FLOAT,
    LIT_STRING,
    LIT_CHAR,
    LIT_BOOL,
    LIT_NONE,
    LIT_NULL
} LiteralKind;

/* Interpolated segment kinds */
typedef enum {
    SEG_TEXT,
    SEG_EXPR
} InterpolatedSegmentKind;

/* AST base node */
typedef struct ASTNode {
    NodeType type;
    uint16_t line;
    uint8_t column;
} ASTNode;

/* All AST node structures */

typedef struct Program {
    ASTNode base;
    struct ASTNode** items;
    size_t item_count;
} Program;

typedef struct ModuleDecl {
    ASTNode base;
    char* name;
    struct ASTNode** body;
    size_t body_count;
} ModuleDecl;

typedef struct UseDecl {
    ASTNode base;
    char** path;
    size_t path_len;
    char** imports;      /* imported symbols; if count==0, import all */
    size_t import_count;
} UseDecl;

typedef struct FuncDecl {
    ASTNode base;
    char* name;
    uint32_t modifiers;  /* e.g., MOD_EXTERN, MOD_VOLATILE */
    uint8_t is_comptime; /* 1 if this is a compile‑time function */
    uint8_t is_method;   /* 1 if this is a method definition inside a struct */
    struct ASTNode** params;
    size_t param_count;
    struct ASTNode* return_type;
    struct ASTNode* body;        /* block */
    struct ASTNode* expr_body;   /* optional expression body */
} FuncDecl;

/* LetDecl now includes an optional array size expression */
typedef struct LetDecl {
    ASTNode base;
    char** names;
    struct ASTNode** types;      /* may be NULL for Auto */
    size_t count;
    struct ASTNode* initializer;
    uint32_t modifiers;
    struct ASTNode* array_size;  /* non‑NULL if the variable is an array with explicit size */
} LetDecl;

typedef struct ComptimeDecl {
    ASTNode base;
    char* name;
    struct ASTNode* type_def;    /* for enum/struct/union */
    struct ASTNode* value;       /* for constant */
    uint32_t modifiers;
} ComptimeDecl;

typedef struct ExternDecl {
    ASTNode base;
    char* name;
    bool is_func;
    struct ASTNode* type;
    struct ASTNode** params;
    size_t param_count;
} ExternDecl;

typedef struct ProcDecl {
    ASTNode base;
    char* name;
    struct ASTNode* body;        /* Block */
} ProcDecl;

typedef struct Block {
    ASTNode base;
    struct ASTNode** statements;
    size_t stmt_count;
} Block;

typedef struct IfStmt {
    ASTNode base;
    struct ASTNode* condition;
    struct ASTNode* then_branch;
    struct ASTNode** else_if_conditions;
    struct ASTNode** else_if_branches;
    size_t else_if_count;
    struct ASTNode* else_branch;
} IfStmt;

typedef struct MatchStmt {
    ASTNode base;
    struct ASTNode* expr;
    struct ASTNode** cases;
    size_t case_count;
    struct ASTNode* else_case;
} MatchStmt;

typedef struct MatchCase {
    ASTNode base;
    struct ASTNode* pattern;
    struct ASTNode* body;
} MatchCase;

typedef struct WhileStmt {
    ASTNode base;
    char* label;
    struct ASTNode* condition;
    struct ASTNode* body;
} WhileStmt;

typedef struct DoWhileStmt {
    ASTNode base;
    char* label;
    struct ASTNode* body;
    struct ASTNode* condition;
} DoWhileStmt;

typedef struct ForStmt {
    ASTNode base;
    char* label;
    char* var_name;
    struct ASTNode* var_type;
    struct ASTNode* range;
    struct ASTNode* body;
} ForStmt;

typedef struct BreakStmt {
    ASTNode base;
    char* label;
} BreakStmt;

typedef struct ContinueStmt {
    ASTNode base;
    char* label;
} ContinueStmt;

typedef struct ReturnStmt {
    ASTNode base;
    struct ASTNode* value;
} ReturnStmt;

typedef struct DeferStmt {
    ASTNode base;
    struct ASTNode* stmt;
} DeferStmt;

typedef struct ReceiveStmt {
    ASTNode base;
    struct ASTNode** cases;
    size_t case_count;
    struct ASTNode* else_case;
} ReceiveStmt;

typedef struct ReceiveCase {
    ASTNode base;
    struct ASTNode* pattern;
    char** captures;
    size_t capture_count;
    struct ASTNode* body;
} ReceiveCase;

typedef struct AsmStmt {
    ASTNode base;
    struct ASTNode* code;         /* string literal expression node */
} AsmStmt;

typedef struct VolatileStmt {
    ASTNode base;
    struct ASTNode* stmt;
} VolatileStmt;

typedef struct ComptimeStmt {
    ASTNode base;
    char* name;
    struct ASTNode* stmt;
} ComptimeStmt;

typedef struct PassStmt {
    ASTNode base;
} PassStmt;

typedef struct ExprStmt {
    ASTNode base;
    struct ASTNode* expr;
} ExprStmt;

/* Expression nodes */

typedef struct IdentExpr {
    ASTNode base;
    char* name;
} IdentExpr;

typedef struct LiteralExpr {
    ASTNode base;
    LiteralKind kind;
    char* raw_value;          /* original text */
    char* string_val;         /* for string/char literals */
    bool bool_val;            /* for boolean literals */
} LiteralExpr;

typedef struct UnaryExpr {
    ASTNode base;
    UnaryOp op;
    struct ASTNode* operand;
} UnaryExpr;

typedef struct BinaryExpr {
    ASTNode base;
    BinaryOp op;
    struct ASTNode* left;
    struct ASTNode* right;
} BinaryExpr;

typedef struct AssignExpr {
    ASTNode base;
    AssignOp op;              /* ASSIGN_NORMAL or compound */
    struct ASTNode* left;
    struct ASTNode* right;
} AssignExpr;

typedef struct SendExpr {
    ASTNode base;
    struct ASTNode* target;
    struct ASTNode* message;
} SendExpr;

typedef struct CallExpr {
    ASTNode base;
    struct ASTNode* callee;
    struct ASTNode** args;
    size_t arg_count;
} CallExpr;

typedef struct MemberAccess {
    ASTNode base;
    struct ASTNode* object;
    char* member;
} MemberAccess;

typedef struct ModuleAccess {
    ASTNode base;
    char** path;
    size_t path_len;
    char* symbol;
} ModuleAccess;

typedef struct IndexExpr {
    ASTNode base;
    struct ASTNode* array;
    struct ASTNode* index;
} IndexExpr;

typedef struct SliceExpr {
    ASTNode base;
    struct ASTNode* array;
    struct ASTNode* range;
} SliceExpr;

/* StructInitExpr supports both positional and named initialisers */
typedef struct StructInitExpr {
    ASTNode base;
    struct ASTNode* struct_type;
    char** field_names;        /* NULL for positional */
    struct ASTNode** values;
    size_t count;
    bool named;                /* true if field_names are used */
} StructInitExpr;

typedef struct RangeExpr {
    ASTNode base;
    struct ASTNode* start;
    struct ASTNode* end;
    bool exclude_start;
    bool exclude_end;
} RangeExpr;

typedef struct IfExpr {
    ASTNode base;
    struct ASTNode* condition;
    struct ASTNode* then_expr;
    struct ASTNode* else_expr;
} IfExpr;

typedef struct TryExpr {
    ASTNode base;
    struct ASTNode* expr;
} TryExpr;

typedef struct CatchExpr {
    ASTNode base;
    struct ASTNode* expr;
    char* var_name;            /* optional capture */
    struct ASTNode* handler;   /* block or expression */
} CatchExpr;

typedef struct ErrorExpr {
    ASTNode base;
    struct ASTNode* code;
} ErrorExpr;

typedef struct SpawnExpr {
    ASTNode base;
    char* proc_name;
    struct ASTNode* proc_body; /* optional inline process body */
} SpawnExpr;

typedef struct SelfExpr {
    ASTNode base;
} SelfExpr;

typedef struct InterpolatedSegment {
    InterpolatedSegmentKind kind;
    char* text;                 /* literal text, valid if kind == SEG_TEXT */
    struct ASTNode* expr;       /* expression, valid if kind == SEG_EXPR */
} InterpolatedSegment;

typedef struct InterpolatedString {
    ASTNode base;
    InterpolatedSegment* segments;
    size_t segment_count;
} InterpolatedString;

typedef struct CastExpr {
    ASTNode base;
    struct ASTNode* expr;
    struct ASTNode* target_type;
} CastExpr;

typedef struct TupleLiteral {
    ASTNode base;
    struct ASTNode** elements;
    size_t count;
} TupleLiteral;

/* New node for array literals: [ expr, ... ] */
typedef struct ArrayLiteral {
    ASTNode base;
    struct ASTNode** elements;
    size_t count;
} ArrayLiteral;

/* Kind of alloc initialiser */
typedef enum {
    ALLOC_NONE,
    ALLOC_LIST,
    ALLOC_RANGE
} AllocKind;

/* New node for alloc expression: alloc [init] as Type */
typedef struct AllocExpr {
    ASTNode base;
    AllocKind kind;            /* which initialiser form is used */
    union {
        struct {
            struct ASTNode** list;   /* for ALLOC_LIST */
            size_t list_count;
        };
        struct ASTNode* range;       /* for ALLOC_RANGE */
    };
    struct ASTNode* type;      /* target type after 'as' */
} AllocExpr;

/* New node for free statement */
typedef struct FreeStmt {
    ASTNode base;
    struct ASTNode* expr;      /* expression to free */
} FreeStmt;

/* Type nodes */

typedef struct FamilyType {
    ASTNode base;
    FamilyTypeKind kind;       /* FAMILY_INT, etc. – defined in type.h */
} FamilyType;

typedef struct BasicType {
    ASTNode base;
    BasicTypeKind kind;        /* TYPE_BASIC_* – defined in type.h */
} BasicType;

typedef struct PointerType {
    ASTNode base;
    struct ASTNode* base_type;
} PointerType;

typedef struct ArrayType {
    ASTNode base;
    struct ASTNode* base_type;
    struct ASTNode* size;      /* expression for size, or NULL for unsized */
} ArrayType;

typedef struct TupleType {
    ASTNode base;
    struct ASTNode** elements;
    size_t count;
} TupleType;

typedef struct NullableType {
    ASTNode base;
    struct ASTNode* base_type;
} NullableType;

typedef struct ErrorType {
    ASTNode base;
    struct ASTNode* base_type;
} ErrorType;

typedef struct FuncType {
    ASTNode base;
    struct ASTNode** param_types;
    size_t param_count;
    struct ASTNode* return_type;
} FuncType;

typedef struct IdentType {
    ASTNode base;
    char* name;
} IdentType;

typedef struct StructType {
    ASTNode base;
    struct ASTNode** fields;
    size_t field_count;
    struct ASTNode** methods;
    size_t method_count;
} StructType;

typedef struct UnionType {
    ASTNode base;
    struct ASTNode** fields;
    size_t field_count;
} UnionType;

typedef struct EnumType {
    ASTNode base;
    struct ASTNode** constants;
    size_t const_count;
} EnumType;

typedef struct EnumConst {
    ASTNode base;
    char* name;
    struct ASTNode* value;     /* optional initializer */
} EnumConst;

typedef struct Param {
    ASTNode base;
    char* name;
    struct ASTNode* type;
    uint32_t modifiers;
    uint8_t is_volatile;
} Param;

typedef struct Field {
    ASTNode base;
    char* name;
    struct ASTNode* type;
    struct ASTNode* default_value;
} Field;

/* Pattern nodes */

#define PATTERN_LITERAL     0
#define PATTERN_TUPLE       1
#define PATTERN_CAPTURE     2
#define PATTERN_PLACEHOLDER 3

typedef struct Pattern {
    ASTNode base;
    uint8_t kind;
    union {
        struct ASTNode* literal;        /* NODE_PATTERN_LITERAL */
        struct {
            struct ASTNode** elements;
            size_t count;
        } tuple;                        /* NODE_PATTERN_TUPLE */
        char* capture_name;             /* NODE_PATTERN_CAPTURE */
    };
} Pattern;

/* Modifier flags */
#define MOD_EXTERN      (1U << 0)
#define MOD_VOLATILE    (1U << 1)
#define MOD_PUBLIC      (1U << 2)

/* Function prototypes */
Program* parse(Token* tokens, size_t count);
void parser_free_ast(Program* prog);

#endif
