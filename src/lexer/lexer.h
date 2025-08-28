#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
#include <stdint.h>

#define SHIFT(lexer, num) do { \
    (lexer)->position += (num); \
    (lexer)->column += (num); \
} while(0)

#define NEXT(lexer, num) \
    ((lexer)->position + (num) < (lexer)->length ? \
    (lexer)->input[(lexer)->position + (num)] : '\0')

#define CHARACTER(lexer, num) \
    ((lexer)->input[(lexer)->position + (num)])

typedef enum {
    TOKEN_NUMBER,
    TOKEN_CHAR,
    TOKEN_STRING,
    TOKEN_NONE,
    TOKEN_NULL,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_RETURN,
    TOKEN_FREE,
    TOKEN_BREAK,
    TOKEN_PERCENT,
    TOKEN_SHARP,
    TOKEN_PREPROC_MACRO,
    TOKEN_PREPROC_ORG,
    TOKEN_PREPROC_PROGRAM,
    TOKEN_PREPROC_INCLUDE,
    TOKEN_PREPROC_DEFINE,
    TOKEN_PREPROC_ASSIGN,
    TOKEN_PREPROC_UNDEF,
    TOKEN_PREPROC_IFDEF,
    TOKEN_PREPROC_IFNDEF,
    TOKEN_PREPROC_ENDIF,
    TOKEN_PREPROC_LINE,
    TOKEN_PREPROC_ERROR,
    TOKEN_PREPROC_PRAGMA,
    TOKEN_DOLLAR,
    TOKEN_COLON,
    TOKEN_ELLIPSIS,
    TOKEN_DOUBLE_DOT,
    TOKEN_DOT,
    TOKEN_DOUBLE_COLON,
    TOKEN_UNDERSCORE,
    TOKEN_MODIFIER,
    TOKEN_ID,
    TOKEN_SEMICOLON,
    TOKEN_EQUAL,
    TOKEN_COMMA,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_TILDE,
    TOKEN_NE_TILDE,
    TOKEN_PIPE,
    TOKEN_AMPERSAND,
    TOKEN_BANG,
    TOKEN_CARET,
    TOKEN_AT,
    TOKEN_GT,
    TOKEN_LT,
    TOKEN_SHR,
    TOKEN_SHL,
    TOKEN_SAR,
    TOKEN_SAL,
    TOKEN_ROR,
    TOKEN_ROL,
    TOKEN_GE,
    TOKEN_LE,
    TOKEN_DOUBLE_EQ,
    TOKEN_NE,
    TOKEN_PLUS_EQ,
    TOKEN_MINUS_EQ,
    TOKEN_STAR_EQ,
    TOKEN_SLASH_EQ,
    TOKEN_PERCENT_EQ,
    TOKEN_PIPE_EQ,
    TOKEN_AMPERSAND_EQ,
    TOKEN_DOUBLE_CARET,
    TOKEN_CARET_EQ,
    TOKEN_TILDE_EQ,
    TOKEN_NE_TILDE_EQ,
    TOKEN_DOUBLE_STAR_EQ,
    TOKEN_SHL_EQ,
    TOKEN_SHR_EQ,
    TOKEN_SAL_EQ,
    TOKEN_SAR_EQ,
    TOKEN_ROL_EQ,
    TOKEN_ROR_EQ,
    TOKEN_DOUBLE_PLUS,
    TOKEN_DOUBLE_MINUS,
    TOKEN_DOUBLE_AMPERSAND,
    TOKEN_DOUBLE_PIPE,
    TOKEN_DOUBLE_STAR,
    TOKEN_QUESTION,
    TOKEN_ARROW,
    TOKEN_LCURLY,
    TOKEN_RCURLY,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_SIZE,
    TOKEN_PARSE,
    TOKEN_REALLOC,
    TOKEN_ALLOC,
    TOKEN_TYPE,
    TOKEN_SELF,
    TOKEN_PUBLIC,
    TOKEN_STACK,
    TOKEN_PUSH,
    TOKEN_POP,
    TOKEN_SYSCALL,
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char* value;
    uint32_t line;
    uint32_t column;
    uint32_t length;
} Token;

typedef struct {
    const char* input;
    uint32_t length;
    uint32_t position;
    uint32_t line;
    uint32_t column;
    Token* tokens;
    uint32_t token_count;
    uint32_t token_capacity;
} Lexer;

extern const char* token_names[];
Lexer* init_lexer(const char* input);
void free_lexer(Lexer* lexer);
void tokenize(Lexer* lexer);

#endif

