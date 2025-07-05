#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define SHIFT(lexer, num) { \
    lexer->position += (num); \
    lexer->column += (num); \
}

#define NEXT(lexer, num) ((lexer)->position + (num) < (lexer)->length ? (lexer)->input[(lexer)->position + (num)] : '\0')

#define CHARACTER(lexer, num) (lexer->input[lexer->position + (num)])

typedef enum {
    TOKEN_INT,
    TOKEN_REAL,
    TOKEN_CHAR,
    TOKEN_NONE,
    TOKEN_NULL,
    TOKEN_STRING,
    TOKEN_IF,
    TOKEN_ELIF,
    TOKEN_ELSE,
    TOKEN_RETURN,
    TOKEN_FREE,
    TOKEN_BREAK,
    TOKEN_GOTO,
    TOKEN_CONTINUE,
    TOKEN_COMPILE,
    TOKEN_SHARP,
    TOKEN_PREPROC_MACRO,
    TOKEN_PREPROC_ORG,
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
    TOKEN_OUTSIDE_COMPILE,
    TOKEN_OUTSIDE_CODE,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_DOUBLE_STAR,
    TOKEN_DOUBLE_AMPERSAND,
    TOKEN_DOUBLE_PIPE,
    TOKEN_BANG,
    TOKEN_TILDE,
    TOKEN_NE_TILDE,
    TOKEN_PIPE,
    TOKEN_AMPERSAND,
    TOKEN_CARET,
    TOKEN_SHR,
    TOKEN_SHL,
    TOKEN_SAR,
    TOKEN_SAL,
    TOKEN_ROR,
    TOKEN_ROL,
    TOKEN_AT,
    TOKEN_GT,
    TOKEN_LT,
    TOKEN_GE,
    TOKEN_LE,
    TOKEN_DOUBLE_EQ,
    TOKEN_NE,
    TOKEN_THIS,
    TOKEN_EQUAL,
    TOKEN_PLUS_EQ,
    TOKEN_MINUS_EQ,
    TOKEN_STAR_EQ,
    TOKEN_SLASH_EQ,
    TOKEN_PERCENT_EQ,
    TOKEN_PIPE_EQ,
    TOKEN_AMPERSAND_EQ,
    TOKEN_CARET_EQ,
    TOKEN_TILDE_EQ,
    TOKEN_NE_TILDE_EQ,
    TOKEN_DOUBLE_PLUS,
    TOKEN_DOUBLE_MINUS,
    TOKEN_DOLLAR,
    TOKEN_COLON,
    TOKEN_ELLIPSIS,
    TOKEN_DOUBLE_DOT,
    TOKEN_DOT,
    TOKEN_DOUBLE_COLON,
    TOKEN_COMMA,
    TOKEN_SEMICOLON,
    TOKEN_QUESTION,
    TOKEN_UNDERSCORE,
    TOKEN_DOUBLE_UNDERSCORE,
    TOKEN_LCURLY,
    TOKEN_RCURLY,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_MODIFIER,
    TOKEN_SIZE,
    TOKEN_PARSE,
    TOKEN_ALLOC,
    TOKEN_TYPE,
    TOKEN_VAR_SIZE,
    TOKEN_ID,
    TOKEN_SYSCALL,
    TOKEN_STACK,
    TOKEN_PUSH,
    TOKEN_POP,
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char* value;
    int line;
    int column;
    int length;
} Token;

extern const char* token_names[];

typedef struct {
    const char* input;
    int length;
    int position;
    int line;
    int column;
    Token* tokens;
    int token_count;
    int token_capacity;
} Lexer;

Lexer* init_lexer(const char* input);
void free_lexer(Lexer* lexer);
void tokenize(Lexer* lexer);
Token* read_tokens_from_file(const char* filename, int* token_count);
void free_tokens(Token* tokens, int token_count);
void tokenize(Lexer* lexer);

#endif

