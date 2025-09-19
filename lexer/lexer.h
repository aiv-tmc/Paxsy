#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
#include <stdint.h>

/* Macro to advance the lexer position by 'num' characters */
#define SHIFT(lexer, num) do { \
    (lexer)->position += (num); \
    (lexer)->column += (num); \
} while(0)

/* Macro to look ahead at a character without consuming it */
#define NEXT(lexer, num) \
    ((lexer)->position + (num) < (lexer)->length ? \
    (lexer)->input[(lexer)->position + (num)] : '\0')

/* Macro to get current character at offset */
#define CHARACTER(lexer, num) \
    ((lexer)->input[(lexer)->position + (num)])

/* Token types enumeration */
typedef enum {
    /* Literals */
    TOKEN_NUMBER,              // Numeric literals
    TOKEN_CHAR,                // Character literals
    TOKEN_STRING,              // String literals
    TOKEN_NULL,                // Null keyword
    TOKEN_NULLPTR,             // Nullptr keyword
    
    /* Control flow keywords */
    TOKEN_CONTINUE,
    TOKEN_JUMP,
    TOKEN_FUNC,
    TOKEN_STRUCT,
    TOKEN_OBJ,
    TOKEN_VAR,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_RETURN,
    TOKEN_FREE,
    TOKEN_BREAK,
    TOKEN_ORG,
    TOKEN_USE,
    TOKEN_BIT,
    TOKEN_FAM,
    TOKEN_SER,
    
    /* Operators and punctuation */
    TOKEN_PERCENT,
    TOKEN_DOLLAR,
    TOKEN_COLON,
    TOKEN_DOUBLE_COLON,
    TOKEN_ELLIPSIS,
    TOKEN_DOT,
    TOKEN_MODIFIER,
    TOKEN_ID,
    TOKEN_SEMICOLON,
    TOKEN_EQUAL,
    TOKEN_COMMA,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_QUESTION,
    TOKEN_TILDE,
    TOKEN_NE_TILDE,
    TOKEN_PIPE,
    TOKEN_AMPERSAND,
    TOKEN_BANG,
    TOKEN_CARET,
    TOKEN_AT,
    TOKEN_GT,                  // Greater than
    TOKEN_LT,                  // Less than
    
    /* Bitwise operators */
    TOKEN_SHR,                 // Shift right
    TOKEN_SHL,                 // Shift left
    TOKEN_SAR,                 // Arithmetic shift right
    TOKEN_SAL,                 // Arithmetic shift left
    TOKEN_ROR,                 // Rotate right
    TOKEN_ROL,                 // Rotate left
    
    /* Comparison operators */
    TOKEN_GE,                  // Greater or equal
    TOKEN_LE,                  // Less or equal
    TOKEN_DOUBLE_EQ,           // Equality
    TOKEN_NE,                  // Not equal
    
    /* Compound assignment operators */
    TOKEN_PLUS_EQ,
    TOKEN_MINUS_EQ,
    TOKEN_STAR_EQ,
    TOKEN_SLASH_EQ,
    TOKEN_PERCENT_EQ,
    TOKEN_PIPE_EQ,
    TOKEN_AMPERSAND_EQ,
    TOKEN_CARET_EQ,
    TOKEN_DOUBLE_CARET,
    TOKEN_TILDE_EQ,
    TOKEN_NE_TILDE_EQ,
    TOKEN_SHL_EQ,
    TOKEN_SHR_EQ,
    TOKEN_SAL_EQ,
    TOKEN_SAR_EQ,
    TOKEN_ROL_EQ,
    TOKEN_ROR_EQ,
    
    /* Increment/Decrement and logical operators */
    TOKEN_DOUBLE_PLUS,         // Increment
    TOKEN_DOUBLE_MINUS,        // Decrement
    TOKEN_DOUBLE_AMPERSAND,    // Logical AND
    TOKEN_DOUBLE_PIPE,         // Logical OR
    TOKEN_DOUBLE_AT,
    
    /* Special symbols */
    TOKEN_ARROW,               // ->
    TOKEN_THEN,                // =>
    TOKEN_LCURLY,              // {
    TOKEN_RCURLY,              // }
    TOKEN_LBRACKET,            // [
    TOKEN_RBRACKET,            // ]
    TOKEN_LBRACE,              // Note: Not used in .c file
    TOKEN_RBRACE,              // Note: Not used in .c file
    TOKEN_LPAREN,              // (
    TOKEN_RPAREN,              // )
    
    /* Memory management keywords */
    TOKEN_SIZEOF,
    TOKEN_PARSEOF,
    TOKEN_REALLOC,
    TOKEN_ALLOC,
    TOKEN_TYPE,                // Type keywords (int, real, etc.)
    
    /* Variadic functions support */
    TOKEN_VA_START,
    TOKEN_VA_END,
    TOKEN_VA_ARG,
    
    /* Stack operations */
    TOKEN_STACK,
    TOKEN_PUSH,
    TOKEN_POP,
    
    /* System operations */
    TOKEN_SYSCALL,
    TOKEN_CPU,
    
    /* Special tokens */
    TOKEN_EOF,                 // End of file
    TOKEN_ERROR                // Error token

} TokenType;

/* Token structure definition */
typedef struct {
    TokenType type;            // Type of the token
    char* value;               // String value of the token
    uint32_t line;             // Line number where token appears
    uint16_t column;           // Column number where token appears
    uint16_t length;           // Length of the token value
} Token;

/* Lexer structure definition */
typedef struct {
    const char* input;         // Input source code
    uint32_t length;           // Total length of input
    uint32_t position;         // Current position in input
    uint16_t line;             // Current line number
    uint16_t column;           // Current column number
    Token* tokens;             // Array of tokens
    uint16_t token_count;      // Number of tokens found
    uint16_t token_capacity;   // Capacity of tokens array
} Lexer;

/* Global array for converting token types to strings */
extern const char* token_names[];

/* Function prototypes */
Lexer* init_lexer(const char* input);
void free_lexer(Lexer* lexer);
void tokenize(Lexer* lexer);

#endif

