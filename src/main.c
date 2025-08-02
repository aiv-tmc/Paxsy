#include "lexer.h"
#include "parser.h"
#include "error_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

// Compilation mode definitions
#define MODE_COMPILE 1
#define MODE_BIN_COMPILE 2
#define MODE_COMPILE_ASM 3

int main(int argc, char* argv[]) {
    int main_mode = 0;               // Main compilation mode
    bool write_log = false;          // Flag for -w (write log)
    char* version_str = NULL;        // Version string for -s flag
    char* asm_type = NULL;           // Assembly type for -o flag
    char* filename = NULL;           // Input filename
    bool mode_set = false;           // Track if mode flag is set

    // Validate minimum arguments
    if (argc < 3) {
        report_error(0, 0, "Usage: %s [-o[=assembler/type] | -q | -c] [-w] [-s=version] <source.px>", argv[0]);
        print_errors();
        free_error_manager();
        return 1;
    }

    // Process all flags
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            char* flag = argv[i] + 1;  // Skip '-'
            char* value = strchr(flag, '=');
            
            // Split flag if it has value
            if (value) {
                *value = '\0';
                value++;
            }

            // Handle mode flags (-o, -q, -c)
            if (strcmp(flag, "o") == 0) {
                if (mode_set) {
                    report_error(0, 0, "Multiple mode flags specified");
                }
                main_mode = MODE_COMPILE_ASM;
                asm_type = value;  // NULL if no value
                mode_set = true;
            }
            else if (strcmp(flag, "q") == 0) {
                if (mode_set) {
                    report_error(0, 0, "Multiple mode flags specified");
                }
                if (value) {
                    report_error(0, 0, "Flag -q doesn't take a value");
                }
                main_mode = MODE_BIN_COMPILE;
                mode_set = true;
            }
            else if (strcmp(flag, "c") == 0) {
                if (mode_set) {
                    report_error(0, 0, "Multiple mode flags specified");
                }
                if (value) {
                    report_error(0, 0, "Flag -c doesn't take a value");
                }
                main_mode = MODE_COMPILE;
                mode_set = true;
            }
            // Handle -w flag
            else if (strcmp(flag, "w") == 0) {
                if (value) {
                    report_error(0, 0, "Flag -w doesn't take a value");
                } else {
                    write_log = true;
                }
            }
            // Handle -s flag
            else if (strcmp(flag, "s") == 0) {
                if (value) {
                    version_str = value;
                } else {
                    report_error(0, 0, "Flag -s requires a value");
                }
            }
            else {
                report_error(0, 0, "Unknown flag: -%s", flag);
            }
        } else {
            // Non-flag argument (filename)
            if (filename) {
                report_error(0, 0, "Multiple filenames specified: '%s' and '%s'", filename, argv[i]);
            } else {
                filename = argv[i];
            }
        }
    }

    // Validation checks
    if (!mode_set) {
        report_error(0, 0, "No compilation mode specified (use -o, -q, or -c)");
    }
    if (!filename) {
        report_error(0, 0, "No input file specified");
    }

    // Check for errors before proceeding
    if (has_errors()) {
        print_errors();
        free_error_manager();
        return 1;
    }

    // Handle mode-specific outputs (same as before)
    if (main_mode == MODE_BIN_COMPILE) {
        printf("bin\n");
    } else if (main_mode == MODE_COMPILE_ASM) {
        if (asm_type) {
            char* slash = strchr(asm_type, '/');
            if (slash) {
                *slash = '\0';
                printf("%s\t|\t%s\n", asm_type, slash + 1);
            } else {
                printf("%s\t|\t(unknown)\n", asm_type);
            }
        } else {
            printf("PAS\t|\tx86_64\n");
        }
    }

    // Output version if provided
    if (version_str) {
        printf("%s\n", version_str);
    }

    // Check file extension
    size_t len = strlen(filename);
    if (len < 3 || strcmp(filename + len - 3, ".px") != 0) {
        report_error(0, 0, "File '%s' has invalid extension. Only .px files are supported.", filename);
        print_errors();
        free_error_manager();
        return 1;
    }

    // Open and read source file (remainder of the code remains the same as before)
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        report_error(0, 0, "Couldn't open file '%s': %s", filename, strerror(errno));
        print_errors();
        free_error_manager();
        return 1;
    }

    // Example continuation (actual implementation should follow your original logic):
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    char* buffer_file = (char*)malloc(file_size + 1);
    if (!buffer_file) {
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

    Lexer* lexer = init_lexer(buffer_file);
    tokenize(lexer);

    // Handle write log mode
    if (write_log) {
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
                    token->type == TOKEN_NUMBER ||
                    token->type == TOKEN_STRING ||
                    token->type == TOKEN_CHAR ||
                    token->type == TOKEN_TYPE ||
                    token->type == TOKEN_MODIFIER ||
                    // token->type == TOKEN_VAR_SIZE ||
                    token->type == TOKEN_ERROR ||
                    token->type == TOKEN_PREPROC_MACRO ||
                    token->type == TOKEN_OUTSIDE_COMPILE ||
                    token->type == TOKEN_OUTSIDE_CODE) {
                    printf("%s:%s", token_names[token->type], token->value);
                } else {
                    printf("%s", token_names[token->type]);
                }
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

    AST* ast = NULL;
    bool parse_errors = has_errors();

    if (!parse_errors) {
        ast = parse(lexer->tokens, lexer->token_count);
        parse_errors = has_errors();
    }

    if (write_log) {
        if (!parse_errors && ast) {
            printf("\nAbstract Syntax Tree:\n");
            print_ast(ast);
        }
    }

    if (has_errors()) {
        printf("\nCompilation Errors:\n");
        print_errors();
    }

    if (ast) free_ast(ast);
    free_lexer(lexer);
    free(buffer_file);
    free_error_manager();
    
    return 0;
}

