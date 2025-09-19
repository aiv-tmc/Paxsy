#include "preprocessor/preprocessor.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h" 
#include "error_manager/error_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#define GENERATION "beta 4"
#define NAME "Blackberry"
#define VERSION "v0.4.0_2"
#define DATE "20250917"

#define MODE_COMPILE 1

/* Macro to handle command-line flags with validation */
#define HANDLE_FLAG(flag_name, short_name, has_value, action) \
    if(strcmp(flag, flag_name) == 0 || strcmp(flag, short_name) == 0) { \
        if(has_value && value == NULL) { \
            report_error(0, 0, "Flag '%s' requires a value", flag); \
            continue; \
        } \
        if(!has_value && value != NULL) { \
            report_error(0, 0, "Flag '%s' doesn't take a value", flag); \
            continue; \
        } \
        action; \
        continue; \
    }

/* Reads entire file into a dynamically allocated buffer */
static char* read_file_contents(const char* filename, size_t* file_size) {
    FILE* file = fopen(filename, "r");
    if(!file) {
        report_error(0, 0, "Couldn't open file '%s': %s", filename, strerror(errno));
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    *file_size = ftell(file);
    rewind(file);

    char* buffer = malloc(*file_size + 1);
    if(!buffer) {
        fclose(file);
        report_error(0, 0, "Memory allocation failed");
        return NULL;
    }

    size_t read_size = fread(buffer, 1, *file_size, file);
    if(read_size != *file_size) {
        free(buffer);
        fclose(file);
        report_error(0, 0, "File read error: %s", strerror(errno));
        return NULL;
    }

    fclose(file);
    buffer[*file_size] = '\0';
    return buffer;
}

/* Prints tokens grouped by line with formatted output */
static void print_tokens_in_lines(Lexer* lexer) {
    uint16_t current_line = 0;
    uint8_t first_token = 1;
    size_t i;
    Token* token;
    
    printf("\033[93mATTENTION:\033[0m \033[4mlexer tokens:\033[0m\n");
    for(i = 0; i < lexer->token_count; i++) {
        token = &lexer->tokens[i];
        if(token->type == TOKEN_EOF) continue;
        
        if(token->line != current_line) {
            if(!first_token) printf("]\n");
            current_line = token->line;
            first_token = 1;
        }
        
        if(!first_token) printf(" ");
        first_token = 0;

        /* Handle token types with values */
        if(token->type == TOKEN_ID ||
           token->type == TOKEN_NUMBER ||
           token->type == TOKEN_STRING ||
           token->type == TOKEN_CHAR ||
           token->type == TOKEN_TYPE ||
           token->type == TOKEN_MODIFIER ||
           token->type == TOKEN_ERROR)
            printf("%s:%s", token_names[token->type], token->value);
        else printf("%s", token_names[token->type]);
    }
    if(!first_token) printf("]\n");
}

int main(int argc, char* argv[]) {
    uint8_t main_mode = 0;
    uint8_t write_log = 0;
    uint8_t write_lexer = 0;
    uint8_t write_parser = 0;
    uint8_t write_semantic = 0;
    uint8_t time_flag = 0;
    uint8_t arm_flag = 0;
    uint8_t mode_set = 0;
    char** filenames = NULL;
    size_t file_count = 0;
    int exit_code = 0;
    int i;
    size_t j;

    /* Parse command-line arguments */
    for(i = 1; i < argc; i++) {
        if(argv[i][0] == '-') {
            char* flag = argv[i];
            char* value = strchr(flag, '=');
            if(value != NULL) {
                *value = '\0';
                value++;
            }

            HANDLE_FLAG("--help", "-h", 0, {
                printf("USAGE: paxsy \033[1m[operations]\033[0m <source> ...\n"
                       "operations:\n"
                       "  \033[1m -h  --help\033[0m\t\t\tDisplay this information\n"
                       "  \033[1m -v  --version\033[0m\t\t\tDisplay compiler version information\n"
                       "  \033[1m -w  --write\033[0m\t\t\tDisplay code analysis\n"
                       "  \033[1m -wl --write-lexer\033[0m\t\tDisplay lexer output only\n"
                       "  \033[1m -wp --write-parser\033[0m\t\tDisplay parser output only\n"
                       "  \033[1m -ws --write-semantic\033[0m\t\tDisplay semantic analysis output only\n"
                       "  \033[1m -t  --time\033[0m\t\t\tTime the execution of each subprocess\n"
                       "  \033[1m -a  --arm\033[0m\t\t\tCompile for ARM processors (requires -c)\n"
                       "  \033[1m -c  --compile\033[0m <source>\tCompile and assemble\n");
                exit_code = 0;
                goto cleanup;
            })
            
            HANDLE_FLAG("--version", "-v", 0, {
                printf("paxsy %s %s\n"
                       "\033[1m%s\033[0m - \033[1m%s\033[0m\n"
                       "\n"
                       "This is being developed by AIV\n"
                       "This free software is distributed under the MIT General Public License\n", 
                       GENERATION, NAME, VERSION, DATE);
                exit_code = 0;
                goto cleanup;
            })
            
            HANDLE_FLAG("--write", "-w", 0, {
                if (write_lexer || write_parser || write_semantic) {
                    report_error(0, 0, "Flag -w cannot be used with -wl, -wp, or -ws");
                } else {
                    write_log = 1;
                }
            })
            
            HANDLE_FLAG("--write-lexer", "-wl", 0, {
                if (write_log || write_parser || write_semantic) {
                    report_error(0, 0, "Flag -wl cannot be used with -w, -wp, or -ws");
                } else {
                    write_lexer = 1;
                }
            })
            
            HANDLE_FLAG("--write-parser", "-wp", 0, {
                if (write_log || write_lexer || write_semantic) {
                    report_error(0, 0, "Flag -wp cannot be used with -w, -wl, or -ws");
                } else {
                    write_parser = 1;
                }
            })
            
            HANDLE_FLAG("--write-semantic", "-ws", 0, {
                if (write_log || write_lexer || write_parser) {
                    report_error(0, 0, "Flag -ws cannot be used with -w, -wl, or -wp");
                } else {
                    write_semantic = 1;
                }
            })
            
            HANDLE_FLAG("--time", "-t", 0, time_flag = 1)
            HANDLE_FLAG("--arm", "-a", 0, arm_flag = 1)
            HANDLE_FLAG("--compile", "-c", 0, {
                if(mode_set) {
                    report_error(0, 0, "Multiple mode flags specified");
                } else {
                    main_mode = MODE_COMPILE;
                    mode_set = 1;
                }
            })
            
            report_error(0, 0, "unknown flag: %s", flag);
        } else {
            char* filename = argv[i];
            size_t len = strlen(filename);
            int duplicate = 0;
            
            /* Validate file extension */
            if(len < 3 || strcmp(filename + len - 3, ".px") != 0) {
                report_error(0, 0, "File '%s' has invalid extension. Only .px files are supported.", filename);
                continue;
            }
            
            /* Check for duplicate files */
            for(j = 0; j < file_count; j++) {
                if(strcmp(filenames[j], filename) == 0) {
                    report_error(0, 0, "Duplicate file: %s", filename);
                    duplicate = 1;
                    break;
                }
            }
            
            if(!duplicate) {
                file_count++;
                filenames = realloc(filenames, file_count * sizeof(char*));
                filenames[file_count - 1] = filename;
            }
        }
    }

    /* Validate flag combinations */
    if(arm_flag && main_mode != MODE_COMPILE) {
        report_error(0, 0, "Flag -a can only be used with -c");
    }

    if(file_count == 0 && (main_mode != 0 || write_log != 0 || write_lexer != 0 || write_parser != 0 || write_semantic != 0)) {
        report_error(0, 0, "no input file specified");
    }
    
    if(main_mode == 0 && write_log == 0 && write_lexer == 0 && write_parser == 0 && write_semantic == 0 && file_count > 0) {
        report_error(0, 0, "Files can only be processed with -c, -w, -wl, -wp or -ws flags");
    }
    
    if(time_flag != 0) report_warning(0, 0, "timing functionality is not implemented yet");

    /* Exit early if errors were found during argument parsing */
    if(has_errors() != 0) {
        printf("\033[93mATTENTION:\033[0m \033[4mcompilation messages:\033[0m\n");
        print_errors();
        print_warnings();
        exit_code = 1;
        goto cleanup;
    }

    /* Process each file through compilation pipeline */
    for(i = 0; i < (int)file_count; i++) {
        char* filename = filenames[i];
        size_t file_size;
        char* buffer_file = read_file_contents(filename, &file_size);
        Lexer* lexer;
        AST* ast;

        if(buffer_file == NULL) continue;

        /* Preprocessing stage */
        int preprocessor_error = 0;
        char* processed = preprocess(buffer_file, filename, &preprocessor_error);
        free(buffer_file);
        if(preprocessor_error) {
            report_error(0, 0, "Preprocessing failed for file: %s", filename);
            continue;
        }

        reset_semantic_analysis();

        /* Handle empty files */
        if((write_log || write_lexer || write_parser) && strlen(processed) == 0) {
            printf("\033[93mATTENTION:\033[0m \033[4mFile '%s' is empty, no tokens to display.\033[0m\n", filename);
            free(processed);
            continue;
        }

        /* Lexical analysis */
        lexer = init_lexer(processed);
        if(lexer == NULL) {
            free(processed);
            continue;
        }
        tokenize(lexer);

        /* Output tokens if requested */
        if((write_log || write_lexer) && lexer->token_count <= 1) {
            printf("File '%s' processed but no significant tokens found.\n", filename);
        } else if(write_log || write_lexer) {
            printf("File: %s\n", filename);
            print_tokens_in_lines(lexer);
        }

        /* Parsing stage */
        ast = NULL;
        if(has_errors() == 0) ast = parse(lexer->tokens, lexer->token_count);

        /* Semantic analysis */
        if(ast != NULL && has_errors() == 0) {
            semantic_analysis(ast);
        }

        /* Output AST if requested */
        if((write_log || write_parser) && has_errors() == 0 && ast != NULL) {
            printf("\033[93mATTENTION:\033[0m \033[4mabstract syntax tree for %s:\033[0m\n", filename);
            print_ast(ast);
        }

        /* Output symbol table if requested */
        if((write_log || write_semantic) && has_errors() == 0 && ast != NULL) {
            printf("\033[93mATTENTION:\033[0m \033[4msemantic analysis for %s:\033[0m\n", filename);
            print_symbol_table(get_global_scope());
        }

        /* Cleanup resources */
        if(ast != NULL) free_ast(ast);
        free_lexer(lexer);
        free(processed);
    }

    /* Final error/warning report */
    if(has_errors() != 0 || has_warnings() != 0) {
        printf("\033[93mATTENTION:\033[0m \033[4mcompilation messages:\033[0m\n");
        if(has_errors() != 0) print_errors();
        if(has_warnings() != 0) print_warnings();
    }

    if(has_errors() != 0) exit_code = 1;

cleanup:
    /* Release allocated resources */
    reset_semantic_analysis();
    free(filenames);
    free_error_manager();
    return exit_code;
}

