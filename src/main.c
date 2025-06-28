#include "lexer.h"
#include "parser.h"
#include "error_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MODE_COMPILE_ONLY 1
#define MODE_VERBOSE 2
#define MODE_LEXER_OUT 3
#define MODE_PARSER_OUT 4
#define MODE_BIN_COMPILE 5

int main(int argc, char* argv[]) {
    int mode = 0;
    char* filename = NULL;

    // Validate command-line arguments
    if (argc < 2 || argc > 3 || argc != 3) {
        report_error(0, 0, "Usage: %s [-c | -w | -l | -p | -o] <source_file>", argv[0]);
        print_errors();
        free_error_manager();
        return 1;
    } 

    // Process command-line flags
    if (argc == 2) {
        // No flag provided - treat as filename
        filename = argv[1];
    } else {
        // Handle flags using switch-case
        switch (argv[1][1]) {
            case 'c': mode = MODE_COMPILE_ONLY; break;
            case 'w': mode = MODE_VERBOSE; break;
            case 'l': mode = MODE_LEXER_OUT; break;
            case 'p': mode = MODE_PARSER_OUT; break;
            case 'o': mode = MODE_BIN_COMPILE; break;
            default:
                report_error(0, 0, "Unknown flag: %s", argv[1]);
                print_errors();
                free_error_manager();
                return 1;
        }
        filename = argv[2];
    }

    // Check file extension
    int len = strlen(filename);
    if (len < 4 || strcmp(filename + len - 4, ".ptx") != 0) {
        report_error(0, 0, "File '%s' has invalid extension. Only .ptx files are supported.", filename);
        print_errors();
        free_error_manager();
        return 1;
    }

    if (mode == MODE_BIN_COMPILE) printf("bin\n");

    // File handling
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        report_error(0, 0, "Couldn't open file '%s': %s", filename, strerror(errno));
        print_errors();
        free_error_manager();
        return 1;
    }

    // Read file contents
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    char* buffer_file = (char*)malloc(file_size + 1);
    if (buffer_file == NULL) {
        report_error(0, 0, "Memory allocation failed");
        fclose(file);
        print_errors();
        free_error_manager();
        return 1;
    }

    size_t bytes_read = fread(buffer_file, 1, file_size, file);
    if (bytes_read != (size_t)file_size) {
        report_error(0, 0, "File read error: %s", strerror(errno));
        free(buffer_file);
        fclose(file);
        print_errors();
        free_error_manager();
        return 1;
    }
    buffer_file[file_size] = '\0';
    fclose(file);

    // Initialize lexer and tokenize
    Lexer* lexer = init_lexer(buffer_file);
    tokenize(lexer);

    // Lexer output for verbose/lexer modes
    if (mode == MODE_VERBOSE || mode == MODE_LEXER_OUT) {
        int max_line = 0;
        for (int i = 0; i < lexer->token_count; i++) {
            if (lexer->tokens[i].line > max_line) max_line = lexer->tokens[i].line;
        }

        Token*** lines = calloc(max_line + 1, sizeof(Token**));
        int* token_counts = calloc(max_line + 1, sizeof(int));
        int* capacities = calloc(max_line + 1, sizeof(int));

        for (int i = 0; i < lexer->token_count; i++) {
            Token* token = &lexer->tokens[i];
            if (token->type == TOKEN_EOF) continue;
            
            int line = token->line;
            if (line >= 0 && line <= max_line) {
                if (token_counts[line] >= capacities[line]) {
                    capacities[line] = capacities[line] ? capacities[line] * 2 : 10;
                    lines[line] = realloc(lines[line], capacities[line] * sizeof(Token*));
                }
                lines[line][token_counts[line]++] = token;
            }
        }

        for (int line = 0; line <= max_line; line++) {
            if (token_counts[line] == 0) continue;
            
            for (int i = 0; i < token_counts[line]; i++) {
                Token* token = lines[line][i];
                
                if (token->type == TOKEN_ID ||
                    token->type == TOKEN_INT ||
                    token->type == TOKEN_REAL ||
                    token->type == TOKEN_STRING ||
                    token->type == TOKEN_CHAR ||
                    token->type == TOKEN_TYPE ||
                    token->type == TOKEN_MODIFIER ||
                    token->type == TOKEN_VAR_SIZE ||
                    token->type == TOKEN_ERROR ||
                    token->type == TOKEN_PREPROC_MACRO ||
                    token->type == TOKEN_OUTSIDE_COMPILE ||
                    token->type == TOKEN_OUTSIDE_CODE) {
                    printf("%s:%s", token_names[token->type], token->value);
                } else printf("%s", token_names[token->type]);
                
                if (i < token_counts[line] - 1) printf(" ");
            }
            printf("]\n");
        }

        for (int i = 0; i <= max_line; i++) {
            free(lines[i]);
        }
        free(lines);
        free(token_counts);
        free(capacities);
    }

    // Parsing and AST generation
    AST* ast = NULL;
    bool parse_errors = has_errors();

    if (!parse_errors) {
        ast = parse(lexer->tokens, lexer->token_count);
        parse_errors = has_errors();
    }

    // AST output for verbose/parser modes
    if (mode == MODE_VERBOSE || mode == MODE_PARSER_OUT) {
        printf("\n\n----- Parser Output -----\n");
        if (!parse_errors && ast) {
            printf("\nAbstract Syntax Tree:\n");
            print_ast(ast);
        }
    }

    // Error reporting
    if (has_errors()) {
        printf("\nCompilation Errors:\n");
        print_errors();
    }

    // Cleanup resources
    if (ast) free_ast(ast);
    free_lexer(lexer);
    free(buffer_file);
    free_error_manager();
    
    return 0;
}

