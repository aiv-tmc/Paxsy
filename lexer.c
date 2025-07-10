#include "lexer.h"
#include "parser.h"
#include "error_manager.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

const char* token_names[] = {
    [TOKEN_NUMBER]              =   "NUMBER",
    [TOKEN_CHAR]                =   "CHAR",
    [TOKEN_NONE]                =   "NONE",
    [TOKEN_NULL]                =   "NULL",
    [TOKEN_IF]                  =   "IF",
    [TOKEN_ELSE]                =   "ELSE"
    [TOKEN_RETURN]              =   "RETURN",
    [TOKEN_FREE]                =   "FREE",
    [TOKEN_BREAK]               =   "BREAK",
    [TOKEN_GOTO]                =   "GOTO",
    [TOKEN_CONTINUE]            =   "CONTINUE",
    [TOKEN_COMPILE]             =   "COMPILE",
    [TOKEN_SHART]               =   "SHARP",
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
    [TOKEN_OUTSIDE_COMPILE]     =   "OUTSIDE_COMPILE",
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
    [TOKEN_ALLOC]               =   "ALLOC",
    [TOKEN_REALLOC]             =   "REALLOC"
    [TOKEN_TYPE]                =   "TYPE",
    [TOKEN_VAR_SIZE]            =   "VAR_SIZE",
    [TOKEN_SYSCALL]             =   "SYSCALL",
    [TOKEN_STACK]               =   "STACK",
    [TOKEN_PUSH]                =   "PUSH",
    [TOKEN_POP]                 =   "POP", 
    [TOKEN_EOF]                 =   "EOF",
    [TOKEN_ERROR]               =   "ERROR"
};

typedef struct {
    char signature[4];
    uint16_t version;
    uint16_t reserved;
    uint32_t token_count;
} TokenFileHeader;

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
    for (int i = 0; i < lexer->token_count; i++) {
        free(lexer->tokens[i].value);
    }
    free(lexer->tokens);
    free(lexer);
}

static void add_token(Lexer* lexer, TokenType type, const char* value, int length) {
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
        if (CHARACTER(lexer, 0) == ' ' || CHARACTER(lexer, 0) == '\t') {
            lexer->position++;
            lexer->column++;
        } else if (CHARACTER(lexer, 0) == '\n') {
            lexer->position++;
            lexer->line++;
            lexer->column = 1;
        } else break;
    }
}

static void skip_comments(Lexer* lexer) {
    while (lexer->position < lexer->length) {
        if (CHARACTER(lexer, 0) == '/' &&
            CHARACTER(lexer, 1) == '/') {
            while (lexer->position < lexer->length && CHARACTER(lexer, 0) != '\n') {
                lexer->position++;
            }
            lexer->column = 1;
        } else if (lexer->position + 1 < lexer->length &&
                   CHARACTER(lexer, 0) == '/' && 
                   CHARACTER(lexer, 1) == '*') {
            lexer->position += 2;
            lexer->column += 2;
            int depth = 1;
            
            while (depth > 0 && lexer->position < lexer->length) {
                if (lexer->position + 1 < lexer->length &&
                    CHARACTER(lexer, 0) == '/' &&
                    CHARACTER(lexer, 1) == '*') {
                    depth++;
                    lexer->position += 2;
                    lexer->column += 2;
                } else if (lexer->position + 1 < lexer->length &&
                           CHARACTER(lexer, 0) == '*' &&
                           CHARACTER(lexer, 1) == '/') {
                    depth--;
                    lexer->position += 2;
                    lexer->column += 2;
                } else {
                    if (CHARACTER(lexer, 0) == '\n') {
                        lexer->line++;
                        lexer->column = 1;
                    } else lexer->column++; 
                    lexer->position++;
                }
            }
            
            if (depth > 0) report_error(lexer->line, lexer->column, "Unclosed comment");
        } else break;
    }
}

// Check if character is valid digit in given base
static bool is_valid_digit(char character, int base) {
    if (base > 36) return false
    if (base <= 10) return character >= '0' && character < '0' + base;

    if (character >= '0' && character <= '9') return true;
    if (character >= 'A' && character <= 'A' + base - 11) return true;
    return false;
}

void parse_number(Lexer* lexer) {
    int start = lexer->position;
    int base = 10;
    bool has_base = false;
    
    if (lexer->position < lexer->length - 1 &&
        lexer->input[lexer->position] == '0') {
        switch(CHARACTER(lexer, 1)) {
            case 'x': PREFIX_NUMBER(start, base, has_base, 16);
            case 'd': PREFIX_NUMBER(start, base, has_base, 10);
            case 'o': PREFIX_NUMBER(start, base, has_base, 8);
            case 'b': PREFIX_NUMBER(start, base, has_base, 4);
            case 'q': PREFIX_NUMBER(start, base, has_base, 2);
        }
    }

    if (has_base) {
        while (lexer->position < lexer->length && 
            is_valid_digit(CHARACTER(lexer, 0), base)) {
            SHIFT(lexer, 1);
        }
    } else {
        base = 10;
        lexer->position = start;
        bool has_dot = false;
        
        while (lexer->position < lexer->length) {
            if (CHARACTER(lexer, 0) == '.') {
                if (has_dot) report_error(lexer->line, lexer->column, "Duplicate decimal point"); has_dot = true;
            } else if (CHARACTER(lexer, 0) == 'e') {
                SHIFT(lexer, 1);
                if (lexer->position < lexer->length && 
                    (CHARACTER(lexer, 0) == '+' || CHARACTER(lexer, 0) == '-'))
                    SHIFT(lexer, 1);
                } else if (CHARACTER(lexer, 0) == '_') {
                    SHIFT(lexer, 1);
                    continue;
                } else if (!is_valid_digit(c, base)) break;
            
                SHIFT(lexer, 1);
                }
            }
        if (lexer->position - start == 0) {
            report_error(lexer->line, lexer->column, "Empty number literal");
            return;
        }
    
        add_token(lexer, TOKEN_NUMBER, lexer->input + start, lexer->position - start);
    }
}

// Parse character literals
void parse_char(Lexer* lexer) {
    SHIFT(lexer, 1); // Skip opening quote
    char value = 0;

    // Check for unclosed character
    if (lexer->position >= lexer->length) {
        report_error(lexer->line, lexer->column, "Unclosed character literal");
        return;
    }

    // Handle escape sequences
    if (NEXT(lexer, 0) == '\\') {
        SHIFT(lexer, 1); 
        if (lexer->position >= lexer->length) {
            report_error(lexer->line, lexer->column, "Incomplete escape sequence");
            return;
        }
        switch (NEXT(lexer, 0)) {
            case 'n': value = '\n'; break;
            case 't': value = '\t'; break;
            case 'r': value = '\r'; break;
            case '0': value = '\0'; break;
            case '%': value = "\\%"; break;
            case '\'': value = '\''; break;
            case '"': value = '\"'; break;
            case '\\': value = '\\'; break;
            default: value = NEXT(lexer, 0);
        }
        SHIFT(lexer, 1);
    } else {
        value = NEXT(lexer, 0);
        SHIFT(lexer, 1);
    }

    // Check for closing quote
    if (lexer->position >= lexer->length || NEXT(lexer, 0) != '\'') {
        report_error(lexer->line, lexer->column, "Unclosed character literal");

        return;
    }

    // Add character token
    char str_val[2] = { value, '\0' };
    add_token(lexer, TOKEN_CHAR, str_val, 1);
    SHIFT(lexer, 1); // Skip closing quote
}

// Parse string literals
void parse_string(Lexer* lexer) {
    SHIFT(lexer, 1); // Skip opening quote
    int buf_size = 128;
    char* buffer = malloc(lexer->length - lexer->position + 1);
    int buf_index = 0;

    while (lexer->position < lexer->length) {
        // Expand buffer if needed
        if (buf_index >= buf_size - 1) {
            buf_size *= 2;
            buffer = realloc(buffer, buf_size);
        }

        // Handle escape sequences
        if (NEXT(lexer, 0) == '\\') {
            SHIFT(lexer, 1);
            if (lexer->position >= lexer->length) {
                report_error(lexer->line, lexer->column, "Incomplete escape sequence");
                free(buffer);
                return;
            }
            switch (NEXT(lexer, 0)) {
                case 'n': buffer[buf_index++] = '\n'; break;
                case 't': buffer[buf_index++] = '\t'; break;
                case 'r': buffer[buf_index++] = '\r'; break;
                case '0': buffer[buf_index++] = '\0'; break;
                case '%': buffer[buf_index++] = "\%"; break;
                case '\'': buffer[buf_index++] = '\''; break;
                case '"': buffer[buf_index++] = '"'; break;
                case '\\': buffer[buf_index++] = '\\'; break;
                default: buffer[buf_index++] = NEXT(lexer, 0);
            }
            SHIFT(lexer, 1);
        } else if (NEXT(lexer, 0) == '"') break;
        // Check for newline in string (invalid)
        else if (NEXT(lexer, 0) == '\n') {
            report_error(lexer->line, lexer->column, "Unclosed string literal");
            free(buffer);
            return;
        } 
        // Normal character
        else {
            buffer[buf_index++] = NEXT(lexer, 0);
            SHIFT(lexer, 1);
        }
    }

    // Validate closing quote
    if (lexer->position >= lexer->length || NEXT(lexer, 0) != '"') {
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
bool is_valid_modifier(const char* modifier) {
    const char* valid_modifiers[] = { 
        "const", "unsig", "signed", "extern", "static", 
        "protected", "dynam", "regis", "local", "global"
    };
    int count = sizeof(valid_modifiers) / sizeof(valid_modifiers[0]);
    for (int i = 0; i < count; i++) {
        if (strcmp(modifier, valid_modifiers[i]) == 0) return true;
    }
    return false;
}
 
// Check if identifier is a valid type
bool is_valid_type(const char* type) {
    const char* valid_types[] = { "int", "real", "char" };
    int count = sizeof(valid_types) / sizeof(valid_types[0]);
    for (int i = 0; i < count; i++) {
        if (strcmp(type, valid_types[i]) == 0) return true;
    }
    return false;
}

void tokenize(Lexer* lexer) {
    while (lexer->position < lexer->length) {
        skip_whitespace(lexer);
        skip_comments(lexer);
        
        if (lexer->position >= lexer->length) break;
        
        switch (NEXT(lexer, 0)) {
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

            case 'i':
                if (strncmp(lexer->input + lexer->position, "if", 2) == 0) {
                    add_token(lexer, TOKEN_IF, "if", 2);
                    SHIFT(lexer, 2);
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
                } else if (strncmp(lexer->input + lexer->position, "syscall", 7) == 0) {
                    add_token(lexer, TOKEN_SYSCALL, "syscall", 7);
                    SHIFT(lexer, 7);
                } else goto identifier;
                break;

            case 'g':
                if (strncmp(lexer->input + lexer->position, "goto", 4) == 0) {
                    add_token(lexer, TOKEN_GOTO, "goto", 4);
                    SHIFT(lexer, 4);
                } else goto identifier;
                break;

            case 'c':
                if (strncmp(lexer->input + lexer->position, "continue", 8) == 0) {
                    add_token(lexer, TOKEN_CONTINUE, "continue", 8);
                    SHIFT(lexer, 8);
                } else if (strncmp(lexer->input + lexer->position, "compile", 7) == 0) {
                    add_token(lexer, TOKEN_COMPILE, "compile", 7);
                    SHIFT(lexer, 7);

                    skip_whitespace(lexer);

                    if (lexer->position >= lexer->length || NEXT(lexer, 0) != '(') report_error(lexer->line, lexer->column, "Expected '(' after 'compile'");
                    else {
                        SHIFT(lexer, 1);
                        skip_whitespace(lexer);

                        int start = lexer->position;
                        while (lexer->position < lexer->length && NEXT(lexer, 0) != ')') {
                            SHIFT(lexer, 1);
                        }

                        if (lexer->position >= lexer->length) report_error(lexer->line, lexer->column, "Unclosed '(' in compile");
                        else {
                            int length = lexer->position - start;
                            add_token(lexer, TOKEN_OUTSIDE_COMPILE, lexer->input + start, length);
                            SHIFT(lexer, 1);
                        }
                    }

                    skip_whitespace(lexer);

                    if (lexer->position >= lexer->length || NEXT(lexer, 0) != '{') report_error(lexer->line, lexer->column, "Expected '{' after compile directive");
                    else {
                        SHIFT(lexer, 1);
                        int start_brace = lexer->position;
                        int brace_depth = 1;
                        while (lexer->position < lexer->length && brace_depth > 0) {
                            if (NEXT(lexer, 0) == '{') brace_depth++;
                            else if (NEXT(lexer, 0) == '}') brace_depth--;
                            SHIFT(lexer, 1);
                        }

                        if (brace_depth != 0) report_error(lexer->line, lexer->column, "Unclosed '{' in compile");
                        else {
                            int length = (lexer->position - start_brace) - 1;
                            if (length > 0) add_token(lexer, TOKEN_OUTSIDE_CODE, lexer->input + start_brace, length);
                            else add_token(lexer, TOKEN_OUTSIDE_CODE, "", 0);
                        }
                    }
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
            } else goto identifier;
            break;

        case '+':
            if (NEXT(lexer, 1) == '+') {
                add_token(lexer, TOKEN_DOUBLE_PLUS, "++", 2);
                SHIFT(lexer, 2);
            } else if (NEXT(lexer, 1) == '=') {
                add_token(lexer, TOKEN_PLUS_EQ, "+=", 2);
                SHIFT(lexer, 2);
            } else {
                add_token(lexer, TOKEN_PLUS, "+", 1);
                SHIFT(lexer, 1);
            }
            break;

        case '-':
            if (NEXT(lexer, 1) == '-') {
                add_token(lexer, TOKEN_DOUBLE_MINUS, "--", 2);
                SHIFT(lexer, 2);
            } else if (NEXT(lexer, 1) == '=') {
                add_token(lexer, TOKEN_MINUS_EQ, "-=", 2);
                SHIFT(lexer, 2);
            } else {
                add_token(lexer, TOKEN_MINUS, "-", 1);
                SHIFT(lexer, 1);
            }
            break;

        case '*':
            if (NEXT(lexer, 1) == '*') {
                add_token(lexer, TOKEN_DOUBLE_STAR, "**", 2);
                SHIFT(lexer, 2);
            } else if (NEXT(lexer, 1) == '=') {
                add_token(lexer, TOKEN_STAR_EQ, "*=", 2);
                SHIFT(lexer, 2);
            } else {
                add_token(lexer, TOKEN_STAR, "*", 1);
                SHIFT(lexer, 1);
            }
            break;

        case '/':
            if (NEXT(lexer, 1) == '=') {
                add_token(lexer, TOKEN_SLASH_EQ, "/=", 2);
                SHIFT(lexer, 2);
            } else {
                add_token(lexer, TOKEN_SLASH, "/", 1);
                SHIFT(lexer, 1);
            }
            break;

        case '%':
            if (NEXT(lexer, 1) == '=') {
                add_token(lexer, TOKEN_PERCENT_EQ, "%=", 2);
                SHIFT(lexer, 2);
            } else {
                add_token(lexer, TOKEN_PERCENT, "%", 1);
                SHIFT(lexer, 1);
            }
            break;

        case '|':
            if (NEXT(lexer, 1) == '|') {
                add_token(lexer, TOKEN_DOUBLE_PIPE, "||", 2);
                SHIFT(lexer, 2);
            } else if (NEXT(lexer, 1) == '=') {
                add_token(lexer, TOKEN_PIPE_EQ, "|=", 2);
                SHIFT(lexer, 2);
            } else {
                add_token(lexer, TOKEN_PIPE, "|", 1);
                SHIFT(lexer, 1);
            }
            break;

        case '&':
            if (NEXT(lexer, 1) == '=') {
                add_token(lexer, TOKEN_AMPERSAND_EQ, "&=", 2);
                SHIFT(lexer, 2);
            } else if (NEXT(lexer, 1) == '&') {
                add_token(lexer, TOKEN_DOUBLE_AMPERSAND, "&&", 2);
                SHIFT(lexer, 2);
            } else {
                add_token(lexer, TOKEN_AMPERSAND, "&", 1);
                SHIFT(lexer, 1);
            }
            break;

        case '!':
            if (NEXT(lexer, 1) == '=') {
                add_token(lexer, TOKEN_NE, "!=", 2);
                SHIFT(lexer, 2);
            } else if (strncmp(lexer->input + lexer->position, "!~=", 3)) {
                add_token(lexer, TOKEN_NE_TILDE_EQ, "!~=", 3);
                SHIFT(lexer, 3);
            } else if (NEXT(lexer, 1) == '~') {
                add_token(lexer, TOKEN_NE_TILDE, "!~", 2);
                SHIFT(lexer, 2);
            } else {
                add_token(lexer, TOKEN_BANG, "!", 1);
                SHIFT(lexer, 1);
            }
            break;

        case '^':
            if (NEXT(lexer, 1) == '=') {
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
            } else if (NEXT(lexer, 1) == '>') {
                add_token(lexer, TOKEN_SHR, ">>", 2);
                SHIFT(lexer, 2);
            } else if (NEXT(lexer, 2) == '=') {
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
            } else if (NEXT(lexer, 1) == '<') {
                add_token(lexer, TOKEN_SHL, "<<", 2);
                SHIFT(lexer, 2);
            } else if (NEXT(lexer, 2) == '=') {
                add_token(lexer, TOKEN_LE, "<=", 2);
                SHIFT(lexer, 2);
            } else {
                add_token(lexer, TOKEN_LT, "<", 1);
                SHIFT(lexer, 1);
            }
            break;

        case '~':
            if (NEXT(lexer, 1) == '=') {
                add_token(lexer, TOKEN_TILDE_EQ, "~=", 2);
                SHIFT(lexer, 2);
            } else {
                add_token(lexer, TOKEN_TILDE, "~", 1);
                SHIFT(lexer, 1);
            }
            break;

        case ':':
            if (NEXT(lexer, 1) == ':') {
                add_token(lexer, TOKEN_DOUBLE_COLON, "::", 2);
                SHIFT(lexer, 2);
            } else {
                add_token(lexer, TOKEN_COLON, ":", 1);
                SHIFT(lexer, 1);
                
                skip_whitespace(lexer);
                if (isalpha(NEXT(lexer, 0)) || 
                    NEXT(lexer, 0) == '[') {
                    if (NEXT(lexer, 0) == '[') {
                        add_token(lexer, TOKEN_LBRACKET, "[", 1);
                        SHIFT(lexer, 1);

                        while (lexer->position < lexer->length) {
                            skip_whitespace(lexer);

                            int mod_start = lexer->position;
                            while (lexer->position < lexer->length &&
                                isalpha(NEXT(lexer, 0))) {
                                SHIFT(lexer, 1);
                            }

                            if (lexer->position > mod_start) {
                                int length = lexer->position - mod_start;
                                char* modifier = strndup(lexer->input + mod_start, length);

                                if (is_valid_modifier(modifier)) add_token(lexer, TOKEN_MODIFIER, modifier, length);
                                else report_error(lexer->line, lexer->column, "Invalid modifier: %s", modifier);
                                free(modifier);
                            }

                            skip_whitespace(lexer);

                            if (NEXT(lexer, 0) == ',') {
                                add_token(lexer, TOKEN_COMMA, ",", 1);
                                SHIFT(lexer, 1);
                                skip_whitespace(lexer);
                            } else if (NEXT(lexer, 0) == ']') break;
                            else {
                                report_error(lexer->line, lexer->column, "Expected ',' or ']' in modifier list");
                                return;
                            }
                        }

                        if (lexer->position >= lexer->length || 
                            NEXT(lexer, 0) != ']') {
                            report_error(lexer->line, lexer->column, "Expected ']' after modifiers");
                            return;
                        }

                        add_token(lexer, TOKEN_RBRACKET, "]", 1);
                        SHIFT(lexer, 1);
                        skip_whitespace(lexer);
                    }

                    int token_start = lexer->position;
                    while (lexer->position < lexer->length &&
                        isalpha(NEXT(lexer, 0))) {
                        SHIFT(lexer, 1);
                    }
                    if (lexer->position > token_start) {
                        int length = lexer->position - token_start;
                        char* type_str = strndup(lexer->input + token_start, length);
                        if (is_valid_type(type_str)) add_token(lexer, TOKEN_TYPE, type_str, length);
                        else report_error(lexer->line, lexer->column, "Invalid type: %s", type_str);
                        free(type_str);
                    } else {
                        report_error(lexer->line, lexer->column, "Expected type after colon");
                        return;
                    }

                    if (lexer->position < lexer->length && NEXT(lexer, 0) == ':') {
                        add_token(lexer, TOKEN_COLON, ":", 1);
                        SHIFT(lexer, 1);
                        token_start = lexer->position;

                        while (lexer->position < lexer->length &&
                            isdigit(NEXT(lexer, 0))) {
                            SHIFT(lexer, 1);
                        }

                        if (lexer->position > token_start) {
                            int length = lexer->position - token_start;
                            add_token(lexer, TOKEN_VAR_SIZE, lexer->input + token_start, length);
                        } else {
                            report_error(lexer->line, lexer->column, "Expected bit size after ':'");
                            return;
                        }
                    }
                }
            }
            break;

        case '.':
            if (strncmp(lexer->input + lexer->position, "...", 3) == 0) {
                add_token(lexer, TOKEN_ELLIPSIS, "...", 3);
                SHIFT(lexer, 3);
    lexer->column++;
            } else if (strncmp(lexer->input + lexer->position, "..", 2) == 0) {
                add_token(lexer, TOKEN_DOUBLE_DOT, "..", 2);
                SHIFT(lexer, 2);
            } else if (strncmp(lexer->input + lexer->position, ".", 1) == 0) {
                add_token(lexer, TOKEN_DOT, ".", 1);
                SHIFT(lexer, 1);
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

        case '#':
            add_token(lexer, TOKEN_SHARP, "#", 1);

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

        case '=':
            if (NEXT(lexer, 1) == '=') {
                add_token(lexer, TOKEN_DOUBLE_EQ, "==", 2);
                SHIFT(lexer, 2);
            } else {
                add_token(lexer, TOKEN_EQUAL, "=", 1);
                SHIFT(lexer, 1);
            }
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

        case '_':
            if (NEXT(lexer, 1) == '_') {
                add_token(lexer, TOKEN_DOUBLE_UNDERSCORE, "__", 2);
                SHIFT(lexer, 2);
            } else {
                add_token(lexer, TOKEN_UNDERSCORE, "_", 1);
                SHIFT(lexer, 1);
            }
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
                        NEXT(lexer, 0) == '+') 
                parse_number(lexer);
            else {
                report_error(lexer->line, lexer->column, "Unexpected character: '%c'", NEXT(lexer, 0));
                SHIFT(lexer, 1);
            }
            break;
        }
    }
    
    add_token(lexer, TOKEN_EOF, "", 0);
}
