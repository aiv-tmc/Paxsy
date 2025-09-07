#include "lexer/lexer.h"
#include "parser/parser.h"
#include "error_manager/error_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#define GENERATION "beta 3.9"
#define NAME "Kiwi"
#define VERSION "v0.3.9"
#define DATE "20250907"

#define MODE_COMPILE 1

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

        if(token->type == TOKEN_ID ||
           token->type == TOKEN_NUMBER ||
           token->type == TOKEN_STRING ||
           token->type == TOKEN_CHAR ||
           token->type == TOKEN_TYPE ||
           token->type == TOKEN_MODIFIER ||
           token->type == TOKEN_ERROR ||
           token->type == TOKEN_PREPROC_MACRO)
            printf("%s:%s", token_names[token->type], token->value);
        else printf("%s", token_names[token->type]);
    }
    if(!first_token) printf("]\n");
}

int main(int argc, char* argv[]) {
    uint8_t main_mode = 0;
    uint8_t write_log = 0;
    uint8_t time_flag = 0;
    uint8_t mode_set = 0;
    char* version_str = NULL;
    char* program_name = NULL;
    char** filenames = NULL;
    size_t file_count = 0;
    int exit_code = 0;
    int i;
    size_t j;

    for(i = 1; i < argc; i++) {
        if(argv[i][0] == '-') {
            char* flag = argv[i];
            char* value = strchr(flag, '=');
            if(value != NULL) {
                *value = '\0';
                value++;
            }

            HANDLE_FLAG("--help", "-h", 0, {
                printf("USAGE: paxsi \033[1m[operations]\033[0m <source> ...\n"
                       "operations:\n"
                       "  \033[1m-h  --help\033[0m\t\t\tDisplay this information\n"
                       "  \033[1m-v  --version\033[0m\t\t\tDisplay compiler version information\n"
                       "  \033[1m-w  --write\033[0m\t\t\tDisplay code analysis\n"
                       "  \033[1m-t  --time\033[0m\t\t\tTime the execution of each subprocess\n"
                       "  \033[1m-s  --set-version\033[0m=[version]\tSet the compile version\n"
                       "  \033[1m-c  --compile\033[0m <source>\tCompile and assemble\n"
                       "  \033[1m-n  --name\033[0m=[name]\t\tSet the output name (only with -c)\n");
                exit_code = 0;
                goto cleanup;
            })
            
            HANDLE_FLAG("--version", "-v", 0, {
                printf("paxsi %s %s\n"
                       "\033[1m%s\033[0m - \033[1m%s\033[0m\n"
                       "\n"
                       "This is being developed by AIV\n"
                       "This free software is distributed under the MIT General Public License\n", 
                       GENERATION, NAME, VERSION, DATE);
                exit_code = 0;
                goto cleanup;
            })
            
            HANDLE_FLAG("--write", "-w", 0, write_log = 1)
            HANDLE_FLAG("--time", "-t", 0, time_flag = 1)
            
            HANDLE_FLAG("--set-version", "-s", 1, 
                version_str = value)
            
            HANDLE_FLAG("--compile", "-c", 0, {
                if(mode_set) {
                    report_error(0, 0, "Multiple mode flags specified");
                } else {
                    main_mode = MODE_COMPILE;
                    mode_set = 1;
                }
            })
            
            HANDLE_FLAG("--name", "-n", 1, {
                program_name = value;
            })
            
            report_error(0, 0, "unknown flag: %s", flag);
        } else {
            char* filename = argv[i];
            size_t len = strlen(filename);
            int duplicate = 0;
            
            if(len < 3 || strcmp(filename + len - 3, ".px") != 0) {
                report_error(0, 0, "File '%s' has invalid extension. Only .px files are supported.", filename);
                continue;
            }
            
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

    if(program_name != NULL && main_mode == 0) {
        report_error(0, 0, "Flag -n can only be used with -c");
    }

    if(file_count == 0 && (main_mode != 0 || write_log != 0)) {
        report_error(0, 0, "no input file specified");
    }
    
    if(main_mode == 0 && write_log == 0 && file_count > 0) {
        report_error(0, 0, "Files can only be processed with -c or -w flags");
    }
    
    if(time_flag != 0) report_warning(0, 0, "timing functionality is not implemented yet");

    if(has_errors() != 0) {
        printf("\033[93mATTENTION:\033[0m \033[4mcompilation messages:\033[0m\n");
        print_errors();
        print_warnings();
        exit_code = 1;
        goto cleanup;
    }

    if(version_str != NULL) printf("%s\n", version_str);

    for(i = 0; i < (int)file_count; i++) {
        char* filename = filenames[i];
        size_t file_size;
        char* buffer_file = read_file_contents(filename, &file_size);
        Lexer* lexer;
        AST* ast;

        if(buffer_file == NULL) continue;

        if(write_log != 0 && file_size == 0) {
            printf("\033[93mATTENTION:\033[0m \033[4mFile '%s' is empty, no tokens to display.\033[0m\n", filename);
            free(buffer_file);
            continue;
        }

        lexer = init_lexer(buffer_file);
        if(lexer == NULL) {
            free(buffer_file);
            continue;
        }
        tokenize(lexer);

        if(write_log != 0 && lexer->token_count <= 1) {
            printf("File '%s' processed but no significant tokens found.\n", filename);
        } else if(write_log != 0) {
            printf("File: %s\n", filename);
            print_tokens_in_lines(lexer);
        }

        ast = NULL;
        if(has_errors() == 0) ast = parse(lexer->tokens, lexer->token_count);

        if(write_log != 0 && has_errors() == 0 && ast != NULL) {
            printf("\033[93mATTENTION:\033[0m \033[4mabstract syntax tree for %s:\033[0m\n", filename);
            print_ast(ast);
        }

        if(ast != NULL) free_ast(ast);
        free_lexer(lexer);
        free(buffer_file);
    }

    if(has_errors() != 0 || has_warnings() != 0) {
        printf("\033[93mATTENTION:\033[0m \033[4mcompilation messages:\033[0m\n");
        if(has_errors() != 0) print_errors();
        if(has_warnings() != 0) print_warnings();
    }

    if(has_errors() != 0) exit_code = 1;

cleanup:
    free(filenames);
    free_error_manager();
    return exit_code;
}

