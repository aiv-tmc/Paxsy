#include "lexer.h"
#include "../error_manager/error_manager.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Mapping of token types to their string representations */
const char* token_names[] = {
    [TOKEN_NUMBER]              =   "NUMBER",
    [TOKEN_CHAR]                =   "CHAR",
    [TOKEN_STRING]              =   "STRING",
    [TOKEN_NULL]                =   "NULL",
    [TOKEN_NULLPTR]             =   "NULLPTR",
    [TOKEN_CONTINUE]            =   "CONTINUE",
    [TOKEN_JUMP]                =   "JUMP",
    [TOKEN_FUNC]                =   "FUNC",
    [TOKEN_STRUCT]              =   "STRUCT",
    [TOKEN_OBJ]                 =   "OBJ",
    [TOKEN_VAR]                 =   "VAR",
    [TOKEN_IF]                  =   "IF",
    [TOKEN_ELSE]                =   "ELSE",
    [TOKEN_RETURN]              =   "RETURN",
    [TOKEN_FREE]                =   "FREE",
    [TOKEN_BREAK]               =   "BREAK",
    [TOKEN_ORG]                 =   "ORG",
    [TOKEN_USE]                 =   "USE",
    [TOKEN_BIT]                 =   "BIT",
    [TOKEN_FAM]                 =   "FAM",
    [TOKEN_SER]                 =   "SER",
    [TOKEN_PERCENT]             =   "PERCENT",
    [TOKEN_DOLLAR]              =   "DOLLAR",
    [TOKEN_COLON]               =   "COLON",
    [TOKEN_DOUBLE_COLON]        =   "DOUBLE_COLON",
    [TOKEN_ELLIPSIS]            =   "ELLIPSIS",
    [TOKEN_DOT]                 =   "DOT",
    [TOKEN_MODIFIER]            =   "MODIFIER",
    [TOKEN_ID]                  =   "ID",
    [TOKEN_SEMICOLON]           =   "SEMICOLON",
    [TOKEN_EQUAL]               =   "EQUAL",
    [TOKEN_COMMA]               =   "COMMA",
    [TOKEN_PLUS]                =   "PLUS",
    [TOKEN_MINUS]               =   "MINUS",
    [TOKEN_STAR]                =   "STAR",
    [TOKEN_SLASH]               =   "SLASH",
    [TOKEN_QUESTION]            =   "QUESTION",
    [TOKEN_TILDE]               =   "TILDE",
    [TOKEN_NE_TILDE]            =   "NE_TILDE",
    [TOKEN_PIPE]                =   "PIPE",
    [TOKEN_AMPERSAND]           =   "AMPERSAND",
    [TOKEN_BANG]                =   "BANG",
    [TOKEN_CARET]               =   "CARET",
    [TOKEN_AT]                  =   "AT",
    [TOKEN_GT]                  =   "GT",
    [TOKEN_LT]                  =   "LT",
    [TOKEN_SHR]                 =   "SHR",
    [TOKEN_SHL]                 =   "SHL",
    [TOKEN_SAR]                 =   "SAR",
    [TOKEN_SAL]                 =   "SAL",
    [TOKEN_ROR]                 =   "ROR",
    [TOKEN_ROL]                 =   "ROL",
    [TOKEN_GE]                  =   "GE",
    [TOKEN_LE]                  =   "LE",
    [TOKEN_DOUBLE_EQ]           =   "DOUBLE_EQ",
    [TOKEN_NE]                  =   "NE",
    [TOKEN_PLUS_EQ]             =   "PLUS_EQ",
    [TOKEN_MINUS_EQ]            =   "MINUS_EQ",
    [TOKEN_STAR_EQ]             =   "STAR_EQ",
    [TOKEN_SLASH_EQ]            =   "SLASH_EQ",
    [TOKEN_PERCENT_EQ]          =   "PERCENT_EQ",
    [TOKEN_PIPE_EQ]             =   "PIPE_EQ",
    [TOKEN_AMPERSAND_EQ]        =   "AMPERSAND_EQ",
    [TOKEN_CARET_EQ]            =   "CARET_EQ",
    [TOKEN_DOUBLE_CARET]        =   "DOUBLE_CARET",
    [TOKEN_TILDE_EQ]            =   "TILDE_EQ",
    [TOKEN_NE_TILDE_EQ]         =   "NE_TILDE_EQ",
    [TOKEN_SHL_EQ]              =   "SHL_EQ",
    [TOKEN_SHR_EQ]              =   "SHR_EQ",
    [TOKEN_SAL_EQ]              =   "SAL_EQ",
    [TOKEN_SAR_EQ]              =   "SAR_EQ",
    [TOKEN_ROL_EQ]              =   "ROL_EQ",
    [TOKEN_ROR_EQ]              =   "ROR_EQ",
    [TOKEN_DOUBLE_PLUS]         =   "DOUBLE_PLUS",
    [TOKEN_DOUBLE_MINUS]        =   "DOUBLE_MINUS",
    [TOKEN_DOUBLE_AMPERSAND]    =   "DOUBLE_AMPERSAND",
    [TOKEN_DOUBLE_PIPE]         =   "DOUBLE_PIPE",
    [TOKEN_DOUBLE_AT]           =   "DOUBLE_AT",
    [TOKEN_ARROW]               =   "ARROW",
    [TOKEN_THEN]                =   "THEN",
    [TOKEN_LCURLY]              =   "LCURLY",
    [TOKEN_RCURLY]              =   "RCURLY",
    [TOKEN_LBRACKET]            =   "LBRACKET",
    [TOKEN_RBRACKET]            =   "RBRACKET",
    [TOKEN_LBRACE]              =   "LBRACE",
    [TOKEN_RBRACE]              =   "RBRACE",
    [TOKEN_LPAREN]              =   "LPAREN",
    [TOKEN_RPAREN]              =   "RPAREN",
    [TOKEN_SIZEOF]              =   "SIZEOF",
    [TOKEN_PARSEOF]             =   "PARSEOF",
    [TOKEN_REALLOC]             =   "REALLOC",
    [TOKEN_ALLOC]               =   "ALLOC",
    [TOKEN_TYPE]                =   "TYPE",
    [TOKEN_VA_START]            =   "VA_START",
    [TOKEN_VA_END]              =   "VA_END",
    [TOKEN_VA_ARG]              =   "VA_ARG",
    [TOKEN_STACK]               =   "STACK",
    [TOKEN_PUSH]                =   "PUSH",
    [TOKEN_POP]                 =   "POP",
    [TOKEN_SYSCALL]             =   "SYSCALL",
    [TOKEN_CPU]                 =   "CPU",
    [TOKEN_EOF]                 =   "EOF",
    [TOKEN_ERROR]               =   "ERROR"
};

/* Initialize a new lexer with the input string */
Lexer* init_lexer(const char* input) {
    Lexer* lexer = malloc(sizeof(Lexer));
    lexer->input = input;
    lexer->length = strlen(input);
    lexer->position = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->token_count = 0;
    lexer->token_capacity = 16;
    lexer->tokens = malloc(lexer->token_capacity * sizeof(Token));
    return lexer;
}

/* Free all resources used by the lexer */
void free_lexer(Lexer* lexer) {
    /* Free each token's value string */
    for (uint32_t i = 0; i < lexer->token_count; i++) {
        free(lexer->tokens[i].value);
    }
    free(lexer->tokens);
    free(lexer);
}

/* Add a new token to the lexer's token array */
static void add_token(Lexer* lexer, TokenType type, const char* value, uint32_t length) {
    /* Resize token array if necessary */
    if (lexer->token_count >= lexer->token_capacity) {
        lexer->token_capacity *= 2;
        lexer->tokens = realloc(lexer->tokens, lexer->token_capacity * sizeof(Token));
    }

    Token* token = &lexer->tokens[lexer->token_count++];
    token->type = type;
    token->value = malloc(length + 1);
    strncpy(token->value, value, length);
    token->value[length] = '\0';
    token->line = lexer->line;
    token->column = lexer->column - length;  // Column where token starts
    token->length = length;
}

/* Skip whitespace and track line/column numbers */
static void skip_whitespace(Lexer* lexer) {
    while (lexer->position < lexer->length) {
        char c = CHARACTER(lexer, 0);
        if (c == ' ' || c == '\t') {
            SHIFT(lexer, 1);
        } else if (c == '\n') {
            SHIFT(lexer, 1);
            lexer->line++;
            lexer->column = 1;
        } else break;
    }
}

/* Check if character is valid for the given numeric base */
static bool is_valid_digit(char c, uint8_t base) {
    if (base <= 10) return c >= '0' && c < '0' + base;
    return isdigit(c) || (c >= 'A' && c < 'A' + base - 10);
}

/* Parse numeric literals with different bases and formats */
static void parse_number(Lexer* lexer) {
    uint32_t start = lexer->position;
    uint8_t base = 10;
    bool has_prefix = false;
    bool has_suffix = false;
    bool has_exponent = false;
    bool has_dot = false;
    bool error = false;

    /* Check for base prefix * */
    if (CHARACTER(lexer, 0) == '0' && lexer->position + 1 < lexer->length) {
        switch (CHARACTER(lexer, 1)) {
            case 'b': base = 2; has_prefix = true; break;
            case 'q': base = 4; has_prefix = true; break;
            case 'o': base = 8; has_prefix = true; break;
            case 'd': base = 10; has_prefix = true; break;
            case 'x': base = 16; has_prefix = true; break;
            case 't': base = 32; has_prefix = true; break;
            case 's': base = 36; has_prefix = true; break;
        }
        if (has_prefix) SHIFT(lexer, 2);
    }

    /* Parse the number body */
    while (lexer->position < lexer->length && !error) {
        /* Skip underscores and backticks as digit separators */
        if (CHARACTER(lexer, 0) == '_' || CHARACTER(lexer, 0) == '`') {
            SHIFT(lexer, 1);
            continue;
        }
        if (!is_valid_digit(CHARACTER(lexer, 0), base)) {
            /* Handle exponent part for base 10 */
            if (base == 10 && CHARACTER(lexer, 0) == 'e') {
                if (has_exponent) {
                    report_error(lexer->line, lexer->column, "Duplicate exponent");
                    error = true;
                    break;
                }
                has_exponent = true;
                SHIFT(lexer, 1);
                if (lexer->position < lexer->length && 
                    (CHARACTER(lexer, 0) == '+' || CHARACTER(lexer, 0) == '-')) {
                    SHIFT(lexer, 1);
                }
                continue;
            }
            /* Handle decimal point */
            if (base == 10 && CHARACTER(lexer, 0) == '.') {
                if (has_dot) {
                    report_error(lexer->line, lexer->column, "Duplicate decimal point");
                    error = true;
                    break;
                }
                has_dot = true;
                SHIFT(lexer, 1);
                continue;
            }
            /* Check for base suffix */
            if (!has_prefix && !has_suffix) {
                switch (CHARACTER(lexer, 0)) {
                    case 'b': base = 2; has_suffix = true; break;
                    case 'q': base = 4; has_suffix = true; break;
                    case 'o': base = 8; has_suffix = true; break;
                    case 'd': base = 10; has_suffix = true; break;
                    case 'x': base = 16; has_suffix = true; break;
                    case 't': base = 32; has_suffix = true; break;
                    case 's': base = 36; has_suffix = true; break;
                }
                if (has_suffix) {
                    SHIFT(lexer, 1);
                    break;
                }
            }
            break;
        }
        SHIFT(lexer, 1);
    }

    uint32_t length = lexer->position - start;
    if (length == 0) {
        report_error(lexer->line, lexer->column, "Empty number literal");
        return;
    }
    add_token(lexer, TOKEN_NUMBER, lexer->input + start, length);
}

/* Parse character literals */
static void parse_char(Lexer* lexer) {
    SHIFT(lexer, 1);  // Skip opening quote
    char value = 0;

    if (lexer->position >= lexer->length) {
        report_error(lexer->line, lexer->column, "Unclosed character literal");
        return;
    }
    /* Handle escape sequences */
    if (CHARACTER(lexer, 0) == '\\') {
        SHIFT(lexer, 1);
        if (lexer->position >= lexer->length) {
            report_error(lexer->line, lexer->column, "Incomplete escape sequence");
            return;
        }
        switch (CHARACTER(lexer, 0)) {
            case 'b': value = '\b'; break;
            case 'f': value = '\f'; break;
            case 'n': value = '\n'; break;
            case 'r': value = '\r'; break;
            case 't': value = '\t'; break;
            case '0': value = '\0'; break;
            case '"': value = '"'; break;
            case '\'': value = '\''; break;
            case '\\': value = '\\'; break;
            default: value = CHARACTER(lexer, 0);
        }
        SHIFT(lexer, 1);
    } else {
        value = CHARACTER(lexer, 0);
        SHIFT(lexer, 1);
    }

    /* Check for closing quote */
    if (lexer->position >= lexer->length || CHARACTER(lexer, 0) != '\'') {
        report_error(lexer->line, lexer->column, "Unclosed character literal");
        return;
    }

    char str_val[2] = { value, '\0' };
    add_token(lexer, TOKEN_CHAR, str_val, 1);
    SHIFT(lexer, 1);  // Skip closing quote
}

/* Parse string literals */
static void parse_string(Lexer* lexer) {
    SHIFT(lexer, 1);  // Skip opening quote
    uint32_t buf_size = 128;
    char* buffer = malloc(buf_size);
    uint32_t buf_index = 0;

    while (lexer->position < lexer->length) {
        /* Resize buffer if needed */
        if (buf_index >= buf_size - 1) {
            buf_size *= 2;
            buffer = realloc(buffer, buf_size);
        }

        char c = CHARACTER(lexer, 0);
        if (c == '\\') {
            SHIFT(lexer, 1);
            if (lexer->position >= lexer->length) {
                report_error(lexer->line, lexer->column, "Incomplete escape sequence");
                free(buffer);
                return;
            }
            switch (CHARACTER(lexer, 0)) {
                case 'b': buffer[buf_index++] = '\b'; break;
                case 'f': buffer[buf_index++] = '\f'; break;
                case 'n': buffer[buf_index++] = '\n'; break;
                case 'r': buffer[buf_index++] = '\r'; break;
                case 't': buffer[buf_index++] = '\t'; break;
                case '0': buffer[buf_index++] = '\0'; break;
                case '"': buffer[buf_index++] = '"'; break;
                case '\'': buffer[buf_index++] = '\''; break;
                case '\\': buffer[buf_index++] = '\\'; break;
                default: buffer[buf_index++] = CHARACTER(lexer, 0);
            }
            SHIFT(lexer, 1);
        } else if (c == '"') {
            break;
        } else if (c == '\n') {
            report_error(lexer->line, lexer->column, "Unclosed string literal");
            free(buffer);
            return;
        } else {
            buffer[buf_index++] = c;
            SHIFT(lexer, 1);
        }
    }

    /* Check for closing quote */
    if (lexer->position >= lexer->length || CHARACTER(lexer, 0) != '"') {
        report_error(lexer->line, lexer->column, "Unclosed string literal");
        free(buffer);
        return;
    }

    buffer[buf_index] = '\0';
    add_token(lexer, TOKEN_STRING, buffer, buf_index);
    free(buffer);
    SHIFT(lexer, 1);  // Skip closing quote
}

/* Check if a word is a valid type modifier */
static bool is_valid_modifier(const char* modifier, uint32_t length) {
    const char* valid_modifiers[] = { "const", "unsigned", "signed", "extern", "static", "volatile" };
    for (uint8_t i = 0; i < 6; i++) {
        if (strlen(valid_modifiers[i]) == length && 
            strncmp(modifier, valid_modifiers[i], length) == 0) return true;
    }
    return false;
}

/* Check if a word is a valid type keyword */
static bool is_valid_type(const char* type, uint32_t length) {
    const char* valid_types[] = { "int", "real", "char", "void", "reg", "va_list" };
    for (uint8_t i = 0; i < 6; i++) {
        if (strlen(valid_types[i]) == length && 
            strncmp(type, valid_types[i], length) == 0) return true;
    }
    return false;
}

/* Main tokenization function */
void tokenize(Lexer* lexer) {
    while (lexer->position < lexer->length) {
        skip_whitespace(lexer);
        if (lexer->position >= lexer->length) break;

        /* Main switch for character processing */
        switch (CHARACTER(lexer, 0)) {
            /* Single character tokens */
            case '$': add_token(lexer, TOKEN_DOLLAR, "$", 1); SHIFT(lexer, 1); break;
            case '?': add_token(lexer, TOKEN_QUESTION, "?", 1); SHIFT(lexer, 1); break;
            case '{': add_token(lexer, TOKEN_LCURLY, "{", 1); SHIFT(lexer, 1); break;
            case '}': add_token(lexer, TOKEN_RCURLY, "}", 1); SHIFT(lexer, 1); break;
            case '[': add_token(lexer, TOKEN_LBRACKET, "[", 1); SHIFT(lexer, 1); break;
            case ']': add_token(lexer, TOKEN_RBRACKET, "]", 1); SHIFT(lexer, 1); break;
            case '(': add_token(lexer, TOKEN_LPAREN, "(", 1); SHIFT(lexer, 1); break;
            case ')': add_token(lexer, TOKEN_RPAREN, ")", 1); SHIFT(lexer, 1); break;
            case ',': add_token(lexer, TOKEN_COMMA, ",", 1); SHIFT(lexer, 1); break;
            case ';': add_token(lexer, TOKEN_SEMICOLON, ";", 1); SHIFT(lexer, 1); break;
            case '\'': parse_char(lexer); break;
            case '"': parse_string(lexer); break;

            /* Multi-character tokens and keywords */
            case 'i':
                if (strncmp(lexer->input + lexer->position, "if", 2) == 0) {
                    add_token(lexer, TOKEN_IF, "if", 2); SHIFT(lexer, 2);
                } else goto identifier;
                break;
            case 'j':
                if (strncmp(lexer->input + lexer->position, "jump", 4) == 0) {
                    add_token(lexer, TOKEN_JUMP, "jump", 4); SHIFT(lexer, 4);
                } else goto identifier;
                break;
            case 'f':
                if (strncmp(lexer->input + lexer->position, "free", 4) == 0) {
                    add_token(lexer, TOKEN_FREE, "free", 4); SHIFT(lexer, 4);
                } else if (strncmp(lexer->input + lexer->position, "fam", 3) == 0) {
                    add_token(lexer, TOKEN_FAM, "fam", 3); SHIFT(lexer, 3);
                } else if (strncmp(lexer->input + lexer->position, "func", 4) == 0) {
                    add_token(lexer, TOKEN_FUNC, "func", 4); SHIFT(lexer, 4);
                } else goto identifier;
                break;
            case 'e':
                if (strncmp(lexer->input + lexer->position, "else", 4) == 0) {
                    add_token(lexer, TOKEN_ELSE, "else", 4); SHIFT(lexer, 4);
                } else goto identifier;
                break;
            case 'u':
                if (strncmp(lexer->input + lexer->position, "use", 3) == 0) {
                    add_token(lexer, TOKEN_USE, "use", 3); SHIFT(lexer, 3);
                } else goto identifier;
                break;
            case 'c':
                if (strncmp(lexer->input + lexer->position, "continue", 8) == 0) {
                    add_token(lexer, TOKEN_CONTINUE, "continue", 8); SHIFT(lexer, 8);
                } else if (strncmp(lexer->input + lexer->position, "cpu", 3) == 0) {
                    add_token(lexer, TOKEN_CPU, "cpu", 3); SHIFT(lexer, 3);
                } else goto identifier;
                break;
            case 's':
                if (strncmp(lexer->input + lexer->position, "sizeof", 6) == 0) {
                    add_token(lexer, TOKEN_SIZEOF, "sizeof", 6); SHIFT(lexer, 6);
                } else if (strncmp(lexer->input + lexer->position, "stack", 5) == 0) {
                    add_token(lexer, TOKEN_STACK, "stack", 5); SHIFT(lexer, 5);
                } else if (strncmp(lexer->input + lexer->position, "syscall", 7) == 0) {
                    add_token(lexer, TOKEN_SYSCALL, "syscall", 7); SHIFT(lexer, 7);
                } else if (strncmp(lexer->input + lexer->position, "ser", 3) == 0) {
                    add_token(lexer, TOKEN_SER, "ser", 3); SHIFT(lexer, 3);
                } else if (strncmp(lexer->input + lexer->position, "struct", 6) == 0) {
                    add_token(lexer, TOKEN_STRUCT, "struct", 6); SHIFT(lexer, 6);
                } else goto identifier;
                break;
            case 'v':
                if (strncmp(lexer->input + lexer->position, "va_start", 8) == 0) {
                    add_token(lexer, TOKEN_VA_START, "va_start", 8); SHIFT(lexer, 8);
                } else if (strncmp(lexer->input + lexer->position, "va_end", 6) == 0) {
                    add_token(lexer, TOKEN_VA_END, "va_end", 6); SHIFT(lexer, 6);
                } else if (strncmp(lexer->input + lexer->position, "va_arg", 6) == 0) {
                    add_token(lexer, TOKEN_VA_ARG, "va_arg", 6); SHIFT(lexer, 6);
                } else if (strncmp(lexer->input + lexer->position, "var", 3) == 0) {
                    add_token(lexer, TOKEN_VAR, "var", 3); SHIFT(lexer, 3);
                } else goto identifier;
                break;
            case 'a':
                if (strncmp(lexer->input + lexer->position, "alloc", 5) == 0) {
                    add_token(lexer, TOKEN_ALLOC, "alloc", 5); SHIFT(lexer, 5);
                } else goto identifier;
                break;
            case 'p':
                if (strncmp(lexer->input + lexer->position, "parseof", 7) == 0) {
                    add_token(lexer, TOKEN_PARSEOF, "parseof", 7); SHIFT(lexer, 7);
                } else if (strncmp(lexer->input + lexer->position, "push", 4) == 0) {
                    add_token(lexer, TOKEN_PUSH, "push", 4); SHIFT(lexer, 4);
                } else if (strncmp(lexer->input + lexer->position, "pop", 3) == 0) {
                    add_token(lexer, TOKEN_POP, "pop", 3); SHIFT(lexer, 3);
                } else goto identifier;
                break;
            case 'b':
                if (strncmp(lexer->input + lexer->position, "break", 5) == 0) {
                    add_token(lexer, TOKEN_BREAK, "break", 5); SHIFT(lexer, 5);
                } else if (strncmp(lexer->input + lexer->position, "bit", 3) == 0) {
                    add_token(lexer, TOKEN_BIT, "bit", 3); SHIFT(lexer, 3); 
                } else goto identifier;
                break;
            case 'o':
                if (strncmp(lexer->input + lexer->position, "org", 3) == 0) {
                    add_token(lexer, TOKEN_ORG, "org", 3); SHIFT(lexer, 3);
                } else if (strncmp(lexer->input + lexer->position, "obj", 3) == 0) {
                    add_token(lexer, TOKEN_OBJ, "obj", 3); SHIFT(lexer, 3);
                } else goto identifier;
                break;
            case 'r':
                if (strncmp(lexer->input + lexer->position, "return", 6) == 0) {
                    add_token(lexer, TOKEN_RETURN, "return", 6); SHIFT(lexer, 6);
                } else if (strncmp(lexer->input + lexer->position, "realloc", 7) == 0) {
                    add_token(lexer, TOKEN_REALLOC, "realloc", 7); SHIFT(lexer, 7);
                } else goto identifier;
                break;
            case 'N':
                if (strncmp(lexer->input + lexer->position, "null", 4) == 0) {
                    add_token(lexer, TOKEN_NULL, "null", 4); SHIFT(lexer, 4);
                } else if (strncmp(lexer->input + lexer->position, "nullptr", 7) == 0) {
                    add_token(lexer, TOKEN_NULLPTR, "nullptr", 7); SHIFT(lexer, 7);
                } else goto identifier;
                break;
            case '+':
                if (CHARACTER(lexer, 1) == '+') {
                    add_token(lexer, TOKEN_DOUBLE_PLUS, "++", 2); SHIFT(lexer, 2);
                } else if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_PLUS_EQ, "+=", 2); SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_PLUS, "+", 1); SHIFT(lexer, 1);
                }
                break;
            case '-':
                if (CHARACTER(lexer, 1) == '>') {
                    add_token(lexer, TOKEN_ARROW, "->", 2); SHIFT(lexer, 2);
                } else if (CHARACTER(lexer, 1) == '-') {
                    add_token(lexer, TOKEN_DOUBLE_MINUS, "--", 2); SHIFT(lexer, 2);
                } else if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_MINUS_EQ, "-=", 2); SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_MINUS, "-", 1); SHIFT(lexer, 1);
                }
                break;
            case '@':
                if (CHARACTER(lexer, 1) == '@') {
                    add_token(lexer, TOKEN_DOUBLE_AT, "@@", 2); SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_AT, "@", 1); SHIFT(lexer, 1);
                }
                break;
            case ':':
                if (CHARACTER(lexer, 1) == ':') {
                    add_token(lexer, TOKEN_DOUBLE_COLON, "::", 2); SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_COLON, ":", 1); SHIFT(lexer, 1);
                }
                break;
            case '*':
                if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_STAR_EQ, "*=", 2); SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_STAR, "*", 1); SHIFT(lexer, 1);
                }
                break;
            case '/':
                if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_SLASH_EQ, "/=", 2); SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_SLASH, "/", 1); SHIFT(lexer, 1);
                }
                break;
            case '|':
                if (CHARACTER(lexer, 1) == '|') {
                    add_token(lexer, TOKEN_DOUBLE_PIPE, "||", 2); SHIFT(lexer, 2);
                } else if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_PIPE_EQ, "|=", 2); SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_PIPE, "|", 1); SHIFT(lexer, 1);
                }
                break;
            case '&':
                if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_AMPERSAND_EQ, "&=", 2); SHIFT(lexer, 2);
                } else if (CHARACTER(lexer, 1) == '&') {
                    add_token(lexer, TOKEN_DOUBLE_AMPERSAND, "&&", 2); SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_AMPERSAND, "&", 1); SHIFT(lexer, 1);
                }
                break;
            case '!':
                if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_NE, "!=", 2); SHIFT(lexer, 2);
                } else if (strncmp(lexer->input + lexer->position, "!~=", 3) == 0) {
                    add_token(lexer, TOKEN_NE_TILDE_EQ, "!~=", 3); SHIFT(lexer, 3);
                } else if (CHARACTER(lexer, 1) == '~') {
                    add_token(lexer, TOKEN_NE_TILDE, "!~", 2); SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_BANG, "!", 1); SHIFT(lexer, 1);
                }
                break;
            case '^':
                if (CHARACTER(lexer, 1) == '^') {
                    add_token(lexer, TOKEN_DOUBLE_CARET, "^^", 2); SHIFT(lexer, 2);
                } else if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_CARET_EQ, "^=", 2); SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_CARET, "^", 1); SHIFT(lexer, 1);
                }
                break;
            case '>':
                if (strncmp(lexer->input + lexer->position, ">>>>", 4) == 0) {
                    if (CHARACTER(lexer, 4) == '=') {
                        add_token(lexer, TOKEN_ROR_EQ, ">>>>=", 5); SHIFT(lexer, 5);
                    } else {
                        add_token(lexer, TOKEN_ROR, ">>>>", 4); SHIFT(lexer, 4);
                    }
                } else if (strncmp(lexer->input + lexer->position, ">>>", 3) == 0) {
                    if (CHARACTER(lexer, 3) == '=') {
                        add_token(lexer, TOKEN_SAR_EQ, ">>>=", 4); SHIFT(lexer, 4);
                    } else {
                        add_token(lexer, TOKEN_SAR, ">>>", 3); SHIFT(lexer, 3);
                    }
                } else if (CHARACTER(lexer, 1) == '>') {
                    if (CHARACTER(lexer, 2) == '=') {
                        add_token(lexer, TOKEN_SHR_EQ, ">>=", 3); SHIFT(lexer, 3);
                    } else {
                        add_token(lexer, TOKEN_SHR, ">>", 2); SHIFT(lexer, 2);
                    }
                } else if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_GE, ">=", 2); SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_GT, ">", 1); SHIFT(lexer, 1);
                }
                break;
            case '<':
                if (strncmp(lexer->input + lexer->position, "<<<<", 4) == 0) {
                    if (CHARACTER(lexer, 4) == '=') {
                        add_token(lexer, TOKEN_ROL_EQ, "<<<<=", 5); SHIFT(lexer, 5);
                    } else {
                        add_token(lexer, TOKEN_ROL, "<<<<", 4); SHIFT(lexer, 4);
                    }
                } else if (strncmp(lexer->input + lexer->position, "<<<", 3) == 0) {
                    if (CHARACTER(lexer, 3) == '=') {
                        add_token(lexer, TOKEN_SAL_EQ, "<<<=", 4); SHIFT(lexer, 4);
                    } else {
                        add_token(lexer, TOKEN_SAL, "<<<", 3); SHIFT(lexer, 3);
                    }
                } else if (CHARACTER(lexer, 1) == '<') {
                    if (CHARACTER(lexer, 2) == '=') {
                        add_token(lexer, TOKEN_SHL_EQ, "<<=", 3); SHIFT(lexer, 3);
                    } else {
                        add_token(lexer, TOKEN_SHL, "<<", 2); SHIFT(lexer, 2);
                    }
                } else if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_LE, "<=", 2); SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_LT, "<", 1); SHIFT(lexer, 1);
                }
                break;
            case '~':
                if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_TILDE_EQ, "~=", 2); SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_TILDE, "~", 1); SHIFT(lexer, 1);
                }
                break;
            case '.':
                if (strncmp(lexer->input + lexer->position, "...", 3) == 0) {
                    add_token(lexer, TOKEN_ELLIPSIS, "...", 3); SHIFT(lexer, 3);
                } 
                break;
            case '=':
                if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_DOUBLE_EQ, "==", 2); SHIFT(lexer, 2);
                } else if (CHARACTER(lexer, 1) == '>') {
                    add_token(lexer, TOKEN_THEN, "=>", 2); SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_EQUAL, "=", 1); SHIFT(lexer, 1);
                }
                break;
            case '%':
                if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_PERCENT_EQ, "%=", 2); SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_PERCENT, "%", 1); SHIFT(lexer, 1);
                }
                break;
            case '#':
                while (lexer->position < lexer->length && 
                       lexer->input[lexer->position] != '\n') {
                    SHIFT(lexer, 1);
                }
                break;

            /* Number parsing */
            default: 
                if (isalpha(NEXT(lexer, 0)) || NEXT(lexer, 0) == '_') {
identifier:
                    /* Handle identifiers and keywords */
                    uint32_t start = lexer->position;
                    while (lexer->position < lexer->length &&
                          (isalnum(NEXT(lexer, 0)) || NEXT(lexer, 0) == '_')) {
                        SHIFT(lexer, 1);
                    }
                    uint32_t length = lexer->position - start;
                    const char* word = lexer->input + start;
                    if (is_valid_type(word, length)) add_token(lexer, TOKEN_TYPE, word, length);
                    else if (is_valid_modifier(word, length)) add_token(lexer, TOKEN_MODIFIER, word, length);
                    else add_token(lexer, TOKEN_ID, word, length);
                } else if (isdigit(NEXT(lexer, 0)) || 
                          NEXT(lexer, 0) == '-' || 
                          NEXT(lexer, 0) == '+') { 
                    parse_number(lexer);
                } else {
                    report_error(lexer->line, lexer->column, "Unexpected character: '%c'", NEXT(lexer, 0));
                    SHIFT(lexer, 1);
                }
                break;
        }
    }
    add_token(lexer, TOKEN_EOF, "", 0);  // Add end-of-file token
}

