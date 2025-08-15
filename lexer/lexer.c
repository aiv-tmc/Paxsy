#include "lexer.h"
#include "../parser/parser.h"
#include "../error_manager/error_manager.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

// Token names corresponding to TokenType enumeration
const char* token_names[] = {
    [TOKEN_NUMBER]              =   "NUMBER",
    [TOKEN_CHAR]                =   "CHAR",
    [TOKEN_STRING]              =   "STRING",
    [TOKEN_NONE]                =   "NONE",
    [TOKEN_NULL]                =   "NULL",
    [TOKEN_IF]                  =   "IF",
    [TOKEN_ELSE]                =   "ELSE",
    [TOKEN_RETURN]              =   "RETURN",
    [TOKEN_FREE]                =   "FREE",
    [TOKEN_BREAK]               =   "BREAK",
    [TOKEN_CONTINUE]            =   "CONTINUE",
    [TOKEN_BUILD]               =   "BUILD",
    [TOKEN_PERCENT]             =   "PERCENT",
    [TOKEN_SHARP]               =   "SHARP",
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
    [TOKEN_OUTSIDE_BUILD]       =   "OUTSIDE_BUILD",
    [TOKEN_OUTSIDE_CODE]        =   "OUTSIDE_CODE",
    [TOKEN_DOLLAR]              =   "DOLLAR",
    [TOKEN_COLON]               =   "COLON",
    [TOKEN_ELLIPSIS]            =   "ELLIPSIS",
    [TOKEN_DOUBLE_DOT]          =   "DOUBLE_DOT",
    [TOKEN_DOT]                 =   "DOT",
    [TOKEN_DOUBLE_COLON]        =   "DOUBLE_COLON",
    [TOKEN_UNDERSCORE]          =   "UNDERSCORE",
    [TOKEN_DOUBLE_UNDERSCORE]   =   "DOUBLE_UNDERSCORE",
    [TOKEN_MODIFIER]            =   "MODIFIER",
    [TOKEN_ID]                  =   "ID",
    [TOKEN_SEMICOLON]           =   "SEMICOLON",
    [TOKEN_EQUAL]               =   "EQUAL",
    [TOKEN_COMMA]               =   "COMMA",
    [TOKEN_PLUS]                =   "PLUS",
    [TOKEN_MINUS]               =   "MINUS",
    [TOKEN_STAR]                =   "STAR",
    [TOKEN_SLASH]               =   "SLASH",
    [TOKEN_PERCENT]             =   "PERCENT",
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
    [TOKEN_TILDE_EQ]            =   "TILDE_EQ",
    [TOKEN_NE_TILDE_EQ]         =   "NE_TILDE_EQ",
    [TOKEN_DOUBLE_PLUS]         =   "DOUBLE_PLUS",
    [TOKEN_DOUBLE_MINUS]        =   "DOUBLE_MINUS",
    [TOKEN_DOUBLE_AMPERSAND]    =   "DOUBLE_AMPERSAND",
    [TOKEN_DOUBLE_PIPE]         =   "DOUBLE_PIPE",
    [TOKEN_DOUBLE_STAR]         =   "DOUBLE_STAR",
    [TOKEN_QUESTION]            =   "QUESTION",
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
    [TOKEN_SELF]                =   "SELF",
    [TOKEN_PUBLIC]              =   "PUBLIC",
    [TOKEN_INVIS]               =   "INVIS",
    [TOKEN_STACK]               =   "STACK",
    [TOKEN_PUSH]                =   "PUSH",
    [TOKEN_POP]                 =   "POP",
    [TOKEN_SYSCALL]             =   "SYSCALL",
    [TOKEN_EOF]                 =   "EOF",
    [TOKEN_ERROR]               =   "ERROR"
};

// Initialize lexer with input string
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

// Free lexer and all allocated memory
void free_lexer(Lexer* lexer) {
    for (int i = 0; i < lexer->token_count; i++) {
        free(lexer->tokens[i].value);
    }
    free(lexer->tokens);
    free(lexer);
}

// Add token to lexer's token array
static void add_token(Lexer* lexer, TokenType type, const char* value, int length) {
    // Expand token array if needed
    if (lexer->token_count >= lexer->token_capacity) {
        lexer->token_capacity *= 2;
        lexer->tokens = realloc(lexer->tokens, lexer->token_capacity * sizeof(Token));
    }

    // Create new token
    Token* token = &lexer->tokens[lexer->token_count++];
    token->type = type;
    token->value = malloc(length + 1);
    strncpy(token->value, value, length);
    token->value[length] = '\0';
    token->line = lexer->line;
    token->column = lexer->column - length;
    token->length = length;
}

// Skip whitespace characters
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

// Skip single-line and multi-line comments
static void skip_comments(Lexer* lexer) {
    while (lexer->position < lexer->length) {
        // Single-line comments
        if (CHARACTER(lexer, 0) == '/' && CHARACTER(lexer, 1) == '/') {
            while (lexer->position < lexer->length && CHARACTER(lexer, 0) != '\n') {
                SHIFT(lexer, 1);
            }
            if (lexer->position < lexer->length) {
                SHIFT(lexer, 1); // Skip newline
                lexer->line++;
                lexer->column = 1;
            }
        } 
        // Multi-line comments
        else if (CHARACTER(lexer, 0) == '/' && CHARACTER(lexer, 1) == '*') {
            SHIFT(lexer, 2); // Skip /*
            int depth = 1;
            
            while (depth > 0 && lexer->position < lexer->length) {
                if (CHARACTER(lexer, 0) == '/' && CHARACTER(lexer, 1) == '*') {
                    depth++;
                    SHIFT(lexer, 2);
                } else if (CHARACTER(lexer, 0) == '*' && CHARACTER(lexer, 1) == '/') {
                    depth--;
                    SHIFT(lexer, 2);
                } else {
                    if (CHARACTER(lexer, 0) == '\n') {
                        lexer->line++;
                        lexer->column = 1;
                    } else {
                        lexer->column++;
                    }
                    SHIFT(lexer, 1);
                }
            }
            
            if (depth > 0) {
                report_error(lexer->line, lexer->column, "Unclosed comment");
            }
        } else break;
    }
}

// Check if character is valid digit in given base
static bool is_valid_digit(char character, int base) {
    if (base <= 10) return character >= '0' && character < ('0' + base);
    if (isdigit(character)) return true;
    if (base > 36) return false;
    return character >= 'A' && character < ('A' + base - 10);
}

// Parse number literals with support for different bases and suffixes
static void parse_number(Lexer* lexer) {
    int start = lexer->position;
    int base = 10;
    bool has_prefix = false;
    bool has_suffix = false;
    bool is_float = false;
    bool has_exponent = false;
    bool has_dot = false;
    bool error = false;

    // Check for base prefix
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
        if (has_prefix) {
            SHIFT(lexer, 2); // Skip prefix
        }
    }

    // Parse number digits
    while (lexer->position < lexer->length && !error) {
        // Skip underscores (digit separators)
        if (CHARACTER(lexer, 0) == '_' || CHARACTER(lexer, 0) == '`') {
            SHIFT(lexer, 1);
            continue;
        }
        
        // Check for valid digit
        if (!is_valid_digit(CHARACTER(lexer, 0), base)) {
            // Check for exponent in base 10
            if (base == 10 && CHARACTER(lexer, 0) == 'e') {
                if (has_exponent) {
                    report_error(lexer->line, lexer->column, "Duplicate exponent");
                    error = true;
                    break;
                }
                has_exponent = true;
                SHIFT(lexer, 1);
                
                // Check for exponent sign
                if (lexer->position < lexer->length && 
                    (CHARACTER(lexer, 0) == '+' || CHARACTER(lexer, 0) == '-')) {
                    SHIFT(lexer, 1);
                }
                continue;
            }
            
            // Check for decimal point in base 10
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
            
            // Check for base suffix
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
            
            // End of number
            break;
        }
        
        SHIFT(lexer, 1);
    }

    // Add number token
    int length = lexer->position - start;
    if (length == 0) {
        report_error(lexer->line, lexer->column, "Empty number literal");
        return;
    }
    
    add_token(lexer, TOKEN_NUMBER, lexer->input + start, length);
}

// Parse character literals with escape sequences
static void parse_char(Lexer* lexer) {
    SHIFT(lexer, 1); // Skip opening quote
    char value = 0;

    // Check for unclosed character
    if (lexer->position >= lexer->length) {
        report_error(lexer->line, lexer->column, "Unclosed character literal");
        return;
    }

    // Handle escape sequences
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

    // Check for closing quote
    if (lexer->position >= lexer->length || CHARACTER(lexer, 0) != '\'') {
        report_error(lexer->line, lexer->column, "Unclosed character literal");
        return;
    }

    // Add character token
    char str_val[2] = { value, '\0' };
    add_token(lexer, TOKEN_CHAR, str_val, 1);
    SHIFT(lexer, 1); // Skip closing quote
}

// Parse string literals with escape sequences
static void parse_string(Lexer* lexer) {
    SHIFT(lexer, 1); // Skip opening quote
    int buf_size = 128;
    char* buffer = malloc(buf_size);
    int buf_index = 0;

    while (lexer->position < lexer->length) {
        // Expand buffer if needed
        if (buf_index >= buf_size - 1) {
            buf_size *= 2;
            buffer = realloc(buffer, buf_size);
        }

        char c = CHARACTER(lexer, 0);
        
        // Handle escape sequences
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
        } 
        // Check for closing quote
        else if (c == '"') {
            break;
        }
        // Check for newline in string (invalid)
        else if (c == '\n') {
            report_error(lexer->line, lexer->column, "Unclosed string literal");
            free(buffer);
            return;
        } 
        // Normal character
        else {
            buffer[buf_index++] = c;
            SHIFT(lexer, 1);
        }
    }

    // Validate closing quote
    if (lexer->position >= lexer->length || CHARACTER(lexer, 0) != '"') {
        report_error(lexer->line, lexer->column, "Unclosed string literal");
        free(buffer);
        return;
    }

    // Add string token
    buffer[buf_index] = '\0';
    add_token(lexer, TOKEN_STRING, buffer, buf_index);
    free(buffer);
    SHIFT(lexer, 1); // Skip closing quote
}

// Check if identifier is a valid modifier
static bool is_valid_modifier(const char* modifier) {
    const char* valid_modifiers[] = { 
        "const", "unsigned", "signed", "extern", "static", "regis"
    };
    for (int i = 0; i < sizeof(valid_modifiers)/sizeof(valid_modifiers[0]); i++) {
        if (strcmp(modifier, valid_modifiers[i]) == 0) return true;
    }
    return false;
}

// Check if identifier is a valid type
static bool is_valid_type(const char* type) {
    const char* valid_types[] = { "int", "real", "char", "void" };
    for (int i = 0; i < sizeof(valid_types)/sizeof(valid_types[0]); i++) {
        if (strcmp(type, valid_types[i]) == 0) return true;
    }
    return false;
}

// Main tokenization function
void tokenize(Lexer* lexer) {
    while (lexer->position < lexer->length) {
        skip_whitespace(lexer);
        skip_comments(lexer);
        
        if (lexer->position >= lexer->length) break;
        
        char current = CHARACTER(lexer, 0);
        switch (current) {
            // Single-character tokens
            case '$': 
                add_token(lexer, TOKEN_DOLLAR, "$", 1);
                SHIFT(lexer, 1);
                break;
            case '@':
                add_token(lexer, TOKEN_AT, "@", 1);
                SHIFT(lexer, 1);
                break;
            case '?':
                add_token(lexer, TOKEN_QUESTION, "?", 1);
                SHIFT(lexer, 1);
                break;
            case '{':
                add_token(lexer, TOKEN_LCURLY, "{", 1);
                SHIFT(lexer, 1);
                break;
            case '}':
                add_token(lexer, TOKEN_RCURLY, "}", 1);
                SHIFT(lexer, 1);
                break;
            case '[':
                add_token(lexer, TOKEN_LBRACKET, "[", 1);
                SHIFT(lexer, 1);
                break;
            case ']':
                add_token(lexer, TOKEN_RBRACKET, "]", 1);
                SHIFT(lexer, 1);
                break;
            case '(':
                add_token(lexer, TOKEN_LPAREN, "(", 1);
                SHIFT(lexer, 1);
                break;
            case ')':
                add_token(lexer, TOKEN_RPAREN, ")", 1);
                SHIFT(lexer, 1);
                break;
            case ',':
                add_token(lexer, TOKEN_COMMA, ",", 1);
                SHIFT(lexer, 1);
                break;
            case ';':
                add_token(lexer, TOKEN_SEMICOLON, ";", 1);
                SHIFT(lexer, 1);
                break;
            case '\'':
                parse_char(lexer);
                break;
            case '"':
                parse_string(lexer);
                break;

            // Multi-character tokens and keywords
            case 'i':
                if (strncmp(lexer->input + lexer->position, "if", 2) == 0) {
                    add_token(lexer, TOKEN_IF, "if", 2);
                    SHIFT(lexer, 2);
                } else if (strncmp(lexer->input + lexer->position, "invis", 5) == 0) {
                    add_token(lexer, TOKEN_INVIS, "invis", 5);
                    SHIFT(lexer, 5);
                } else goto identifier;
                break;
            case 'f':
                if (strncmp(lexer->input + lexer->position, "free", 4) == 0) {
                    add_token(lexer, TOKEN_FREE, "free", 4);
                    SHIFT(lexer, 4);
                } else goto identifier;
                break;
            case 'e':
                if (strncmp(lexer->input + lexer->position, "else", 4) == 0) {
                    add_token(lexer, TOKEN_ELSE, "else", 4);
                    SHIFT(lexer, 4);
                } else goto identifier;
                break;
            case 's':
                if (strncmp(lexer->input + lexer->position, "size", 4) == 0) {
                    add_token(lexer, TOKEN_SIZE, "size", 4);
                    SHIFT(lexer, 4);
                } else if (strncmp(lexer->input + lexer->position, "self", 4) == 0) {
                    add_token(lexer, TOKEN_SELF, "self", 4);
                    SHIFT(lexer, 4);
                } else if (strncmp(lexer->input + lexer->position, "syscall", 7) == 0) {
                    add_token(lexer, TOKEN_SYSCALL, "syscall", 7);
                    SHIFT(lexer, 7);
                } else if (strncmp(lexer->input + lexer->position, "stack", 5) == 0) {
                    add_token(lexer, TOKEN_STACK, "stack", 5);
                    SHIFT(lexer, 5);
                } else goto identifier;
                break;
            case 'c':
                if (strncmp(lexer->input + lexer->position, "continue", 8) == 0) {
                    add_token(lexer, TOKEN_CONTINUE, "continue", 8);
                    SHIFT(lexer, 8);
                } else goto identifier;
                break;
            case 'a':
                if (strncmp(lexer->input + lexer->position, "alloc", 5) == 0) {
                    add_token(lexer, TOKEN_ALLOC, "alloc", 5);
                    SHIFT(lexer, 5);
                } else goto identifier;
                break;
            case 'p':
                if (strncmp(lexer->input + lexer->position, "parse", 5) == 0) {
                    add_token(lexer, TOKEN_PARSE, "parse", 5);
                    SHIFT(lexer, 5);
                } else if (strncmp(lexer->input + lexer->position, "public", 6) == 0) {
                    add_token(lexer, TOKEN_PUBLIC, "public", 6);
                    SHIFT(lexer, 6);
                } else if (strncmp(lexer->input + lexer->position, "push", 4) == 0) {
                    add_token(lexer, TOKEN_PUSH, "push", 4);
                    SHIFT(lexer, 4);
                } else if (strncmp(lexer->input + lexer->position, "pop", 3) == 0) {
                    add_token(lexer, TOKEN_POP, "pop", 3);
                    SHIFT(lexer, 3);
                } else goto identifier;
                break;
            case 'b':
                if (strncmp(lexer->input + lexer->position, "break", 5) == 0) {
                    add_token(lexer, TOKEN_BREAK, "break", 5);
                    SHIFT(lexer, 5);
                } else if (strncmp(lexer->input + lexer->position, "build", 5) == 0) {
                    add_token(lexer, TOKEN_BUILD, "build", 5);
                    SHIFT(lexer, 5);

                    skip_whitespace(lexer);

                    if (lexer->position >= lexer->length || NEXT(lexer, 0) != '(') report_error(lexer->line, lexer->column, "Expected '(' after 'build'");
                    else {
                        SHIFT(lexer, 1);
                        skip_whitespace(lexer);

                        int start = lexer->position;
                        while (lexer->position < lexer->length && NEXT(lexer, 0) != ')') {
                            SHIFT(lexer, 1);
                        }

                        if (lexer->position >= lexer->length) report_error(lexer->line, lexer->column, "Unclosed '(' in build");
                        else {
                            int length = lexer->position - start;
                            add_token(lexer, TOKEN_OUTSIDE_BUILD, lexer->input + start, length);
                            SHIFT(lexer, 1);
                        }
                    }

                    skip_whitespace(lexer);

                    if (lexer->position >= lexer->length || NEXT(lexer, 0) != '{') report_error(lexer->line, lexer->column, "Expected '{' after build directive");
                    else {
                        SHIFT(lexer, 1);
                        int start_brace = lexer->position;
                        int brace_depth = 1;
                        while (lexer->position < lexer->length && brace_depth > 0) {
                            if (NEXT(lexer, 0) == '{') brace_depth++;
                            else if (NEXT(lexer, 0) == '}') brace_depth--;
                            SHIFT(lexer, 1);
                        }

                        if (brace_depth != 0) report_error(lexer->line, lexer->column, "Unclosed '{' in build");
                        else {
                            int length = (lexer->position - start_brace) - 1;
                            if (length > 0) add_token(lexer, TOKEN_OUTSIDE_CODE, lexer->input + start_brace, length);
                            else add_token(lexer, TOKEN_OUTSIDE_CODE, "", 0);
                        }
                    } 
                } else goto identifier;
                break;
            case 'r':
                if (strncmp(lexer->input + lexer->position, "return", 6) == 0) {
                    add_token(lexer, TOKEN_RETURN, "return", 6);
                    SHIFT(lexer, 6);
                } else if (strncmp(lexer->input + lexer->position, "realloc", 7) == 0) {
                    add_token(lexer, TOKEN_REALLOC, "realloc", 7);
                    SHIFT(lexer, 7);
                } else goto identifier;
                break;
            case 'N':
                if (strncmp(lexer->input + lexer->position, "NONE", 4) == 0) {
                    add_token(lexer, TOKEN_NONE, "NONE", 4);
                    SHIFT(lexer, 4);
                } else if (strncmp(lexer->input + lexer->position, "NULL", 4) == 0) {
                    add_token(lexer, TOKEN_NULL, "NULL", 4);
                    SHIFT(lexer, 4);
                } else goto identifier;
                break;

            // Operators
            case '+':
                if (CHARACTER(lexer, 1) == '+') {
                    add_token(lexer, TOKEN_DOUBLE_PLUS, "++", 2);
                    SHIFT(lexer, 2);
                } else if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_PLUS_EQ, "+=", 2);
                    SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_PLUS, "+", 1);
                    SHIFT(lexer, 1);
                }
                break;
            case '-':
                if (CHARACTER(lexer, 1) == '-') {
                    add_token(lexer, TOKEN_DOUBLE_MINUS, "--", 2);
                    SHIFT(lexer, 2);
                } else if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_MINUS_EQ, "-=", 2);
                    SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_MINUS, "-", 1);
                    SHIFT(lexer, 1);
                }
                break;
            case '*':
                if (CHARACTER(lexer, 1) == '*') {
                    add_token(lexer, TOKEN_DOUBLE_STAR, "**", 2);
                    SHIFT(lexer, 2);
                } else if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_STAR_EQ, "*=", 2);
                    SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_STAR, "*", 1);
                    SHIFT(lexer, 1);
                }
                break;
            case '/':
                if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_SLASH_EQ, "/=", 2);
                    SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_SLASH, "/", 1);
                    SHIFT(lexer, 1);
                }
                break;
            case '|':
                if (CHARACTER(lexer, 1) == '|') {
                    add_token(lexer, TOKEN_DOUBLE_PIPE, "||", 2);
                    SHIFT(lexer, 2);
                } else if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_PIPE_EQ, "|=", 2);
                    SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_PIPE, "|", 1);
                    SHIFT(lexer, 1);
                }
                break;
            case '&':
                if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_AMPERSAND_EQ, "&=", 2);
                    SHIFT(lexer, 2);
                } else if (CHARACTER(lexer, 1) == '&') {
                    add_token(lexer, TOKEN_DOUBLE_AMPERSAND, "&&", 2);
                    SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_AMPERSAND, "&", 1);
                    SHIFT(lexer, 1);
                }
                break;
            case '!':
                if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_NE, "!=", 2);
                    SHIFT(lexer, 2);
                } else if (strncmp(lexer->input + lexer->position, "!~=", 3) == 0) {
                    add_token(lexer, TOKEN_NE_TILDE_EQ, "!~=", 3);
                    SHIFT(lexer, 3);
                } else if (CHARACTER(lexer, 1) == '~') {
                    add_token(lexer, TOKEN_NE_TILDE, "!~", 2);
                    SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_BANG, "!", 1);
                    SHIFT(lexer, 1);
                }
                break;
            case '^':
                if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_CARET_EQ, "^=", 2);
                    SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_CARET, "^", 1);
                    SHIFT(lexer, 1);
                }
                break;
            case '>':
                if (strncmp(lexer->input + lexer->position, ">>>>", 4) == 0) {
                    add_token(lexer, TOKEN_ROR, ">>>>", 4);
                    SHIFT(lexer, 4);
                } else if (strncmp(lexer->input + lexer->position, ">>>", 3) == 0) {
                    add_token(lexer, TOKEN_SAR, ">>>", 3);
                    SHIFT(lexer, 3);
                } else if (CHARACTER(lexer, 1) == '>') {
                    add_token(lexer, TOKEN_SHR, ">>", 2);
                    SHIFT(lexer, 2);
                } else if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_GE, ">=", 2);
                    SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_GT, ">", 1);
                    SHIFT(lexer, 1);
                }
                break;
            case '<':
                if (strncmp(lexer->input + lexer->position, "<<<<", 4) == 0) {
                    add_token(lexer, TOKEN_ROL, "<<<<", 4);
                    SHIFT(lexer, 4);
                } else if (strncmp(lexer->input + lexer->position, "<<<", 3) == 0) {
                    add_token(lexer, TOKEN_SAL, "<<<", 3);
                    SHIFT(lexer, 3);
                } else if (CHARACTER(lexer, 1) == '<') {
                    add_token(lexer, TOKEN_SHL, "<<", 2);
                    SHIFT(lexer, 2);
                } else if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_LE, "<=", 2);
                    SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_LT, "<", 1);
                    SHIFT(lexer, 1);
                }
                break;
            case '%':
                if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_PERCENT_EQ, "%=", 2);
                    SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_PERCENT, "%", 1);
                    SHIFT(lexer, 1);
                }
                break;
            case '~':
                if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_TILDE_EQ, "~=", 2);
                    SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_TILDE, "~", 1);
                    SHIFT(lexer, 1);
                }
                break;
            case ':':
                if (CHARACTER(lexer, 1) == ':') {
                    add_token(lexer, TOKEN_DOUBLE_COLON, "::", 2);
                    SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_COLON, ":", 1);
                    SHIFT(lexer, 1);
                }
                break;
            case '.':
                if (strncmp(lexer->input + lexer->position, "...", 3) == 0) {
                    add_token(lexer, TOKEN_ELLIPSIS, "...", 3);
                    SHIFT(lexer, 3);
                } else if (strncmp(lexer->input + lexer->position, "..", 2) == 0) {
                    add_token(lexer, TOKEN_DOUBLE_DOT, "..", 2);
                    SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_DOT, ".", 1);
                    SHIFT(lexer, 1);
                }
                break;
            case '=':
                if (CHARACTER(lexer, 1) == '=') {
                    add_token(lexer, TOKEN_DOUBLE_EQ, "==", 2);
                    SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_EQUAL, "=", 1);
                    SHIFT(lexer, 1);
                }
                break;
            case '_':
                if (CHARACTER(lexer, 1) == '_') {
                    add_token(lexer, TOKEN_DOUBLE_UNDERSCORE, "__", 2);
                    SHIFT(lexer, 2);
                } else {
                    add_token(lexer, TOKEN_UNDERSCORE, "_", 1);
                    SHIFT(lexer, 1);
                }
                break;
            case '#':
                add_token(lexer, TOKEN_SHARP, "#", 1);
                SHIFT(lexer, 1);

                if (strncmp(lexer->input + lexer->position, "#org", 4) == 0) {
                    add_token(lexer, TOKEN_PREPROC_ORG, "org", 3);
                    SHIFT(lexer, 4);
                } else if (strncmp(lexer->input + lexer->position, "#include", 8) == 0) {
                    add_token(lexer, TOKEN_PREPROC_INCLUDE, "include", 7);
                    SHIFT(lexer, 8);
                } else if (strncmp(lexer->input + lexer->position, "#define", 7) == 0) {
                    add_token(lexer, TOKEN_PREPROC_DEFINE, "define", 6);
                    SHIFT(lexer, 7);
                } else if (strncmp(lexer->input + lexer->position, "#assign", 7) == 0) {
                    add_token(lexer, TOKEN_PREPROC_ASSIGN, "assign", 6);
                    SHIFT(lexer, 7);
                } else if (strncmp(lexer->input + lexer->position, "#undef", 6) == 0) {
                    add_token(lexer, TOKEN_PREPROC_UNDEF, "undef", 5);
                    SHIFT(lexer, 6);
                } else if (strncmp(lexer->input + lexer->position, "#ifdef", 6) == 0) {
                    add_token(lexer, TOKEN_PREPROC_IFDEF, "ifdef", 5);
                    SHIFT(lexer, 6);
                } else if (strncmp(lexer->input + lexer->position, "#ifndef", 6) == 0) {
                    add_token(lexer, TOKEN_PREPROC_IFNDEF, "ifndef", 6);
                    SHIFT(lexer, 6);
                } else if (strncmp(lexer->input + lexer->position, "#endif", 6) == 0) {
                    add_token(lexer, TOKEN_PREPROC_ENDIF, "endif", 5);
                    SHIFT(lexer, 6);
                } else if (strncmp(lexer->input + lexer->position, "#line", 5) == 0) {
                    add_token(lexer, TOKEN_PREPROC_LINE, "line", 4);
                    SHIFT(lexer, 5);
                } else if (strncmp(lexer->input + lexer->position, "#error", 6) == 0) {
                    add_token(lexer, TOKEN_PREPROC_ERROR, "error", 5);
                    SHIFT(lexer, 6);
                } else if (strncmp(lexer->input + lexer->position, "#pragma", 7) == 0) {
                    add_token(lexer, TOKEN_PREPROC_PRAGMA, "pragma", 6);
                    SHIFT(lexer, 7);
                } else if (strncmp(lexer->input + lexer->position, "#macro", 6) == 0) {
                    add_token(lexer, TOKEN_PREPROC_MACRO, "macro", 5);
                    SHIFT(lexer, 6);
                }
                break;


            // Default cases (identifiers, numbers, errors)
            default:
                if ((NEXT(lexer, 0) == '/' && NEXT(lexer, 1) == '/') || 
                (lexer->position < lexer->length - 1 && 
                NEXT(lexer, 0) == '<' && 
                NEXT(lexer, 1) == '/')) {
                skip_comments(lexer);
                break;
            }

            if (isalpha(NEXT(lexer, 0)) || 
                NEXT(lexer, 0) == '_') {
            identifier:
                int start = lexer->position;
                while (lexer->position < lexer->length &&
                      (isalnum(NEXT(lexer, 0)) ||
                       NEXT(lexer, 0) == '_')) {
                    SHIFT(lexer, 1);
                }
                int length = lexer->position - start;
                char* word = strndup(lexer->input + start, length);
                if (!word) {
                    report_error(lexer->line, lexer->column, "Memory error");
                    return;
                }

                if (is_valid_type(word)) add_token(lexer, TOKEN_TYPE, word, length);
                else if (is_valid_modifier(word)) add_token(lexer, TOKEN_MODIFIER, word, length);
                else add_token(lexer, TOKEN_ID, word, length);
                free(word);
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
    
    // Add EOF token at the end
    add_token(lexer, TOKEN_EOF, "", 0);
}

