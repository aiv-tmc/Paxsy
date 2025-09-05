#include "lexer.h"
#include "../error_manager/error_manager.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

const char* token_names[] = {
    [TOKEN_NUMBER]              =   "NUMBER",
    [TOKEN_CHAR]                =   "CHAR",
    [TOKEN_STRING]              =   "STRING",
    [TOKEN_NULL]                =   "NULL",
    [TOKEN_NULLPTR]             =   "NULLPTR",
    [TOKEN_CONTINUE]            =   "CONTINUE",
    [TOKEN_GOTO]                =   "GOTO",
    [TOKEN_IF]                  =   "IF",
    [TOKEN_ELSE]                =   "ELSE",
    [TOKEN_RETURN]              =   "RETURN",
    [TOKEN_FREE]                =   "FREE",
    [TOKEN_BREAK]               =   "BREAK",
    [TOKEN_PERCENT]             =   "PERCENT",
    [TOKEN_PREPROC_MACRO]       =   "PREPROC_MACRO",
    [TOKEN_PREPROC_ORG]         =   "PREPROC_ORG",
    [TOKEN_PREPROC_INCLUDE]     =   "PREPROC_INCLUDE",
    [TOKEN_PREPROC_DEFINE]      =   "PREPROC_DEFINE",
    [TOKEN_PREPROC_ASSIGN]      =   "PREPROC_ASSIGN",
    [TOKEN_PREPROC_UNDEF]       =   "PREPROC_UNDEF",
    [TOKEN_PREPROC_IFDEF]       =   "PREPROC_IFDEF",
    [TOKEN_PREPROC_IFNDEF]      =   "PREPROC_IFNDEF",
    [TOKEN_PREPROC_ENDIF]       =   "PREPROC_ENDIF",
    [TOKEN_PREPROC_LINE]        =   "PREPROC_LINE",
    [TOKEN_PREPROC_ERROR]       =   "PREPROC_ERROR",
    [TOKEN_PREPROC_PRAGMA]      =   "PREPROC_PRAGMA",
    [TOKEN_DOLLAR]              =   "DOLLAR",
    [TOKEN_DOUBLE_DOLLAR]       =   "DOUBLE_DOLLAR",
    [TOKEN_COLON]               =   "COLON",
    [TOKEN_ELLIPSIS]            =   "ELLIPSIS",
    [TOKEN_DOUBLE_DOT]          =   "DOUBLE_DOT",
    [TOKEN_DOT]                 =   "DOT",
    [TOKEN_DOUBLE_COLON]        =   "DOUBLE_COLON",
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
    [TOKEN_DOUBLE_STAR_EQ]      =   "DOUBLE_STAR_EQ",
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
    [TOKEN_DOUBLE_STAR]         =   "DOUBLE_STAR",
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
    [TOKEN_SIZE]                =   "SIZE",
    [TOKEN_PARSE]               =   "PARSE",
    [TOKEN_REALLOC]             =   "REALLOC",
    [TOKEN_ALLOC]               =   "ALLOC",
    [TOKEN_TYPE]                =   "TYPE",
    [TOKEN_VA_START]            =   "VA_START",
    [TOKEN_VA_END]              =   "VA_END",
    [TOKEN_VA_ARG]              =   "VA_ARG",
    [TOKEN_SELF]                =   "SELF",
    [TOKEN_PUBLIC]              =   "PUBLIC",
    [TOKEN_STACK]               =   "STACK",
    [TOKEN_PUSH]                =   "PUSH",
    [TOKEN_POP]                 =   "POP",
    [TOKEN_INT]                 =   "INT",
    [TOKEN_EOF]                 =   "EOF",
    [TOKEN_ERROR]               =   "ERROR"
};

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

void free_lexer(Lexer* lexer) {
    for (uint32_t i = 0; i < lexer->token_count; i++) {
        free(lexer->tokens[i].value);
    }
    free(lexer->tokens);
    free(lexer);
}

static void add_token(Lexer* lexer, TokenType type, const char* value, uint32_t length) {
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
    token->column = lexer->column - length;
    token->length = length;
}

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

static void skip_comments(Lexer* lexer) {
    while (lexer->position < lexer->length) {
        if (CHARACTER(lexer, 0) == '#') {
            while (lexer->position < lexer->length && CHARACTER(lexer, 0) != '\n') SHIFT(lexer, 1);
            if (lexer->position < lexer->length) {
                SHIFT(lexer, 1);
                lexer->line++;
                lexer->column = 1;
            }
        } else if (CHARACTER(lexer, 0) == '<' && CHARACTER(lexer, 1) == '/') {
            uint32_t start_line = lexer->line;
            uint32_t start_column = lexer->column;
            SHIFT(lexer, 2);
            uint32_t depth = 1;
            
            while (depth > 0 && lexer->position < lexer->length) {
                if (CHARACTER(lexer, 0) == '/' && CHARACTER(lexer, 1) == '>') {
                    depth++;
                    SHIFT(lexer, 2);
                } else if (CHARACTER(lexer, 0) == '/' && CHARACTER(lexer, 1) == '>') {
                    depth--;
                    SHIFT(lexer, 2);
                } else {
                    if (CHARACTER(lexer, 0) == '\n') {
                        lexer->line++;
                        lexer->column = 0;
                    }
                    SHIFT(lexer, 1);
                    lexer->column++;
                }
            }
            
            if (depth > 0) report_error(start_line, start_column, "Unclosed comment");
        } else break;
    }
}

static bool is_valid_digit(char c, uint8_t base) {
    if (base <= 10) return c >= '0' && c < '0' + base;
    return isdigit(c) || (c >= 'A' && c < 'A' + base - 10);
}

static void parse_number(Lexer* lexer) {
    uint32_t start = lexer->position;
    uint8_t base = 10;
    bool has_prefix = false;
    bool has_suffix = false;
    bool has_exponent = false;
    bool has_dot = false;
    bool error = false;

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

    while (lexer->position < lexer->length && !error) {
        if (CHARACTER(lexer, 0) == '_' || CHARACTER(lexer, 0) == '`') {
            SHIFT(lexer, 1);
            continue;
        }
        if (!is_valid_digit(CHARACTER(lexer, 0), base)) {
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

static void parse_char(Lexer* lexer) {
    SHIFT(lexer, 1);
    char value = 0;

    if (lexer->position >= lexer->length) {
        report_error(lexer->line, lexer->column, "Unclosed character literal");
        return;
    }

    if (CHARACTER(lexer, 0) == '\\') {
        SHIFT(lexer, 1);
        if (lexer->position >= lexer->length) {
            report_error(lexer->line, lexer->column, "Incomplete escape sequence");
            return;
        }
        switch (CHARACTER(lexer, 0)) {
            case 'n': value = '\n'; break;
            case 't': value = '\t'; break;
            case 'r': value = '\r'; break;
            case '0': value = '\0'; break;
            case '\'': value = '\''; break;
            case '"': value = '\"'; break;
            case '\\': value = '\\'; break;
            default: value = CHARACTER(lexer, 0);
        }
        SHIFT(lexer, 1);
    } else {
        value = CHARACTER(lexer, 0);
        SHIFT(lexer, 1);
    }

    if (lexer->position >= lexer->length || CHARACTER(lexer, 0) != '\'') {
        report_error(lexer->line, lexer->column, "Unclosed character literal");
        return;
    }

    char str_val[2] = { value, '\0' };
    add_token(lexer, TOKEN_CHAR, str_val, 1);
    SHIFT(lexer, 1);
}

static void parse_string(Lexer* lexer) {
    SHIFT(lexer, 1);
    uint32_t buf_size = 128;
    char* buffer = malloc(buf_size);
    uint32_t buf_index = 0;

    while (lexer->position < lexer->length) {
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
                case 'n': buffer[buf_index++] = '\n'; break;
                case 't': buffer[buf_index++] = '\t'; break;
                case 'r': buffer[buf_index++] = '\r'; break;
                case '0': buffer[buf_index++] = '\0'; break;
                case '\'': buffer[buf_index++] = '\''; break;
                case '"': buffer[buf_index++] = '"'; break;
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

    if (lexer->position >= lexer->length || CHARACTER(lexer, 0) != '"') {
        report_error(lexer->line, lexer->column, "Unclosed string literal");
        free(buffer);
        return;
    }

    buffer[buf_index] = '\0';
    add_token(lexer, TOKEN_STRING, buffer, buf_index);
    free(buffer);
    SHIFT(lexer, 1);
}

static bool is_valid_modifier(const char* modifier, uint32_t length) {
    const char* valid_modifiers[] = { "const", "unsigned", "signed", "extern", "static", "volatile" };
    for (uint8_t i = 0; i < 6; i++) {
        if (strlen(valid_modifiers[i]) == length && 
            strncmp(modifier, valid_modifiers[i], length) == 0) return true;
    }
    return false;
}

static bool is_valid_type(const char* type, uint32_t length) {
    const char* valid_types[] = { "dig", "real", "char", "reg", "void", "va_list" };
    for (uint8_t i = 0; i < 4; i++) {
        if (strlen(valid_types[i]) == length && 
            strncmp(type, valid_types[i], length) == 0) return true;
    }
    return false;
}

void tokenize(Lexer* lexer) {
    while (lexer->position < lexer->length) {
        skip_whitespace(lexer);
        skip_comments(lexer);
        if (lexer->position >= lexer->length) break;

        switch (CHARACTER(lexer, 0)) {
            case '@': add_token(lexer, TOKEN_AT, "@", 1); SHIFT(lexer, 1); break;
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
            case 'i':
                if (strncmp(lexer->input + lexer->position, "if", 2) == 0) {
                    add_token(lexer, TOKEN_IF, "if", 2); SHIFT(lexer, 2);
                } else if (strncmp(lexer->input + lexer->position, "int", 3) == 0) {
                    add_token(lexer, TOKEN_INT, "int", 3); SHIFT(lexer, 3);
                } else goto identifier;
                break;
            case 'g':
                if (strncmp(lexer->input + lexer->position, "goto", 4) == 0) {
                    add_token(lexer, TOKEN_GOTO, "goto", 4); SHIFT(lexer, 4);
                } else goto identifier;
                break;
            case 'f':
                if (strncmp(lexer->input + lexer->position, "free", 4) == 0) {
                    add_token(lexer, TOKEN_FREE, "free", 4); SHIFT(lexer, 4);
                } else goto identifier;
                break;
            case 'e':
                if (strncmp(lexer->input + lexer->position, "else", 4) == 0) {
                    add_token(lexer, TOKEN_ELSE, "else", 4); SHIFT(lexer, 4);
                } else goto identifier;
                break;
            case 'c':
                if (strncmp(lexer->input + lexer->position, "continue", 8) == 0) {
                    add_token(lexer, TOKEN_CONTINUE, "continue", 8); SHIFT(lexer, 8);
                } else goto identifier;
                break;
            case 's':
                if (strncmp(lexer->input + lexer->position, "size", 4) == 0) {
                    add_token(lexer, TOKEN_SIZE, "size", 4); SHIFT(lexer, 4);
                } else if (strncmp(lexer->input + lexer->position, "self", 4) == 0) {
                    add_token(lexer, TOKEN_SELF, "self", 4); SHIFT(lexer, 4);
                } else if (strncmp(lexer->input + lexer->position, "stack", 5) == 0) {
                    add_token(lexer, TOKEN_STACK, "stack", 5); SHIFT(lexer, 5);
                } else goto identifier;
                break;
            case 'v':
                if (strncmp(lexer->input + lexer->position, "va_start", 8) == 0) {
                    add_token(lexer, TOKEN_VA_START, "va_start", 8); SHIFT(lexer, 8);
                } else if (strncmp(lexer->input + lexer->position, "va_end", 6) == 0) {
                    add_token(lexer, TOKEN_VA_END, "va_end", 6); SHIFT(lexer, 6);
                } else if (strncmp(lexer->input + lexer->position, "va_arg", 6) == 0) {
                    add_token(lexer, TOKEN_VA_ARG, "va_arg", 6); SHIFT(lexer, 6);
                } else goto identifier;
                break;
            case 'a':
                if (strncmp(lexer->input + lexer->position, "alloc", 5) == 0) {
                    add_token(lexer, TOKEN_ALLOC, "alloc", 5); SHIFT(lexer, 5);
                } else goto identifier;
                break;
            case 'p':
                if (strncmp(lexer->input + lexer->position, "parse", 5) == 0) {
                    add_token(lexer, TOKEN_PARSE, "parse", 5); SHIFT(lexer, 5);
                } else if (strncmp(lexer->input + lexer->position, "public", 6) == 0) {
                    add_token(lexer, TOKEN_PUBLIC, "public", 6); SHIFT(lexer, 6);
                } else if (strncmp(lexer->input + lexer->position, "push", 4) == 0) {
                    add_token(lexer, TOKEN_PUSH, "push", 4); SHIFT(lexer, 4);
                } else if (strncmp(lexer->input + lexer->position, "pop", 3) == 0) {
                    add_token(lexer, TOKEN_POP, "pop", 3); SHIFT(lexer, 3);
                } else goto identifier;
                break;
            case 'b':
                if (strncmp(lexer->input + lexer->position, "break", 5) == 0) {
                    add_token(lexer, TOKEN_BREAK, "break", 5); SHIFT(lexer, 5); 
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
            case '$': 
                if (CHARACTER(lexer, 1) == '$') { add_token(lexer, TOKEN_DOUBLE_DOLLAR, "$$", 2); SHIFT(lexer, 2); }
                else { add_token(lexer, TOKEN_DOLLAR, "$", 1); SHIFT(lexer, 1); }
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
            case '*':
                if (CHARACTER(lexer, 1) == '*') {
                    if (CHARACTER(lexer, 2) == '=') {
                        add_token(lexer, TOKEN_DOUBLE_STAR_EQ, "**=", 3); SHIFT(lexer, 3);
                    } else {
                        add_token(lexer, TOKEN_DOUBLE_STAR, "**", 2); SHIFT(lexer, 2);
                    }
                } else if (CHARACTER(lexer, 1) == '=') {
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
            case ':':
                if (CHARACTER(lexer, 1) == ':') {
                    add_token(lexer, TOKEN_DOUBLE_COLON, "::", 2); SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_COLON, ":", 1); SHIFT(lexer, 1);
                }
                break;
            case '.':
                if (strncmp(lexer->input + lexer->position, "...", 3) == 0) {
                    add_token(lexer, TOKEN_ELLIPSIS, "...", 3); SHIFT(lexer, 3);
                } else if (strncmp(lexer->input + lexer->position, "..", 2) == 0) {
                    add_token(lexer, TOKEN_DOUBLE_DOT, "..", 2); SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_DOT, ".", 1); SHIFT(lexer, 1);
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
                    
                    if (strncmp(lexer->input + lexer->position, "org", 3) == 0) {
                        add_token(lexer, TOKEN_PREPROC_ORG, "org", 3); SHIFT(lexer, 3);
                    } else if (strncmp(lexer->input + lexer->position, "include", 7) == 0) {
                        add_token(lexer, TOKEN_PREPROC_INCLUDE, "include", 7); SHIFT(lexer, 7);
                    } else if (strncmp(lexer->input + lexer->position, "define", 6) == 0) {
                        add_token(lexer, TOKEN_PREPROC_DEFINE, "define", 6); SHIFT(lexer, 6);
                    } else if (strncmp(lexer->input + lexer->position, "assign", 6) == 0) {
                        add_token(lexer, TOKEN_PREPROC_ASSIGN, "assign", 6); SHIFT(lexer, 6);
                    } else if (strncmp(lexer->input + lexer->position, "undef", 5) == 0) {
                        add_token(lexer, TOKEN_PREPROC_UNDEF, "undef", 5); SHIFT(lexer, 5);
                    } else if (strncmp(lexer->input + lexer->position, "ifdef", 5) == 0) {
                        add_token(lexer, TOKEN_PREPROC_IFDEF, "ifdef", 5); SHIFT(lexer, 5);
                    } else if (strncmp(lexer->input + lexer->position, "ifndef", 6) == 0) {
                       add_token(lexer, TOKEN_PREPROC_IFNDEF, "ifndef", 6); SHIFT(lexer, 6);
                    } else if (strncmp(lexer->input + lexer->position, "endif", 5) == 0) {
                        add_token(lexer, TOKEN_PREPROC_ENDIF, "endif", 5); SHIFT(lexer, 5);
                    } else if (strncmp(lexer->input + lexer->position, "line", 4) == 0) {
                        add_token(lexer, TOKEN_PREPROC_LINE, "line", 4); SHIFT(lexer, 4);
                    } else if (strncmp(lexer->input + lexer->position, "error", 5) == 0) {
                        add_token(lexer, TOKEN_PREPROC_ERROR, "error", 5); SHIFT(lexer, 5);
                    } else if (strncmp(lexer->input + lexer->position, "pragma", 6) == 0) {
                        add_token(lexer, TOKEN_PREPROC_PRAGMA, "pragma", 6); SHIFT(lexer, 6);
                    } else if (strncmp(lexer->input + lexer->position, "macro", 5) == 0) {
                        add_token(lexer, TOKEN_PREPROC_MACRO, "macro", 5); SHIFT(lexer, 5);
                    }
                }
                break;
            default:
                if ((NEXT(lexer, 0) == '/' && NEXT(lexer, 1) == '/') || 
                    (lexer->position < lexer->length - 1 && 
                    NEXT(lexer, 0) == '<' && 
                    NEXT(lexer, 1) == '/')) {
                    skip_comments(lexer);
                    break;
                }
                if (isalpha(NEXT(lexer, 0)) || NEXT(lexer, 0) == '_') {
identifier:
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
    add_token(lexer, TOKEN_EOF, "", 0);
}

