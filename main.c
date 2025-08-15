#include "lexer/lexer.h"
#include "parser/parser.h"
#include "error_manager/error_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#define VERSION "paxsi_0.3.5.14A"
#define DATE "20250815"

#define MODE_COMPILE 1
#define MODE_BIN_COMPILE 2
#define MODE_COMPILE_ASM 3

int main(int argc, char* argv[]) {
    int     main_mode   =   0;
    bool    write_log   =   false;
    bool    time_flag   =   false;
    char*   version_str =   NULL;
    char*   program_name=   NULL;
    char*   asm_type    =   NULL;
    char*   filename    =   NULL;
    bool    mode_set    =   false;

    // Process all flags
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            bool is_long = (argv[i][1] == '-');
            char* flag = argv[i] + 1 + (is_long ? 1 : 0);  // Skip '-' or '--'
            char* value = strchr(flag, '=');
            
            if (value) {
                *value = '\0';
                value++;
            }

            // Long flags (--flag) and short flags (-f) processing
            if (is_long) {
                if (strcmp(flag, "help") == 0) {
                    if (value) report_error(0, 0, "Flag '--help' doesn't take a value");
                    else {
                        printf("usage: pxi \033[1m[operations]\033[0m <source> ...\n");
                        printf("operations:\n"
                               "  \033[1m-h  --help\033[0m\t\t\tDisplay this information\n"
                               "  \033[1m-v  --version\033[0m\t\t\tDisplay compiler version information\n"
                               "  \033[1m-w  --write\033[0m\t\t\tDisplay code analysis\n"
                               "  \033[1m-t  --time\033[0m\t\t\tTime the execution of each subprocess\n"
                               "  \033[1m-n  --name\033[0m=[name]\t\tSet the name of your program\n"
                               "  \033[1m-s  --set-version\033[0m=[version]\tSet the compile version\n"
                               "  \033[1m-o  --asmlang\033[0m=[asm/arch]\tCompile into the Assembler language\n"
                               "  \033[1m-q  --binary\033[0m <source>\t\tCompile to binary file\n"
                               "  \033[1m-c  --compile\033[0m <source>\tCompile and assemble\n");
                        free_error_manager();
                        return 0;
                    }
                } else if (strcmp(flag, "version") == 0) {
                    if (value) report_error(0, 0, "Flag '--version' doesn't take a value");
                    else {
                        printf("pxi \033[1m%s - %s\033[0m\n"
                               "This is being developed by AIV\n"
                               "This free software is distributed under thre MIT General Public License\n", 
                               VERSION, DATE);
                        free_error_manager();
                        return 0;
                    }
                } else if (strcmp(flag, "write") == 0) {
                    if (value) report_error(0, 0, "Flag '--write' doesn't take a value");
                    else write_log = true;
                } else if (strcmp(flag, "time") == 0) {
                    if (value) report_error(0, 0, "Flag '--time' doesn't take a value");
                    else {
                        time_flag = true;
                        fprintf(stderr, "\033[93mWARNING\033[0m: timing is not implemented yet\n");
                    }
                } else if (strcmp(flag, "name") == 0) {
                    if (!value) report_error(0, 0, "Flag '--name' requires a value");
                    else {
                        program_name = value;
                        fprintf(stderr, "\033[93mWARNING\033[0m: program name setting is not implemented yet\n");
                    }
                } else if (strcmp(flag, "set-version") == 0) {
                    if (!value) report_error(0, 0, "Flag '--set-version' requires a value");
                    else version_str = value;
                } else if (strcmp(flag, "asmlang") == 0) {
                    if (mode_set) report_error(0, 0, "Multiple mode flags specified");
                    else if (!value) report_error(0, 0, "Flag '--asmlang' requires a value");
                    else {
                        main_mode = MODE_COMPILE_ASM;
                        asm_type = value;
                        mode_set = true;
                    }
                } else if (strcmp(flag, "binary") == 0) {
                    if (mode_set) report_error(0, 0, "Multiple mode flags specified");
                    else if (value) report_error(0, 0, "Flag '--binary' doesn't take a value");
                    else {
                        main_mode = MODE_BIN_COMPILE;
                        mode_set = true;
                    }
                } else if (strcmp(flag, "compile") == 0) {
                    if (mode_set) report_error(0, 0, "Multiple mode flags specified");
                    else if (value) report_error(0, 0, "Flag '--compile' doesn't take a value");
                    else {
                        main_mode = MODE_COMPILE;
                        mode_set = true;
                    }
                } else report_error(0, 0, "unknown flag: --%s", flag);
            } else {
                // Short flags processing
                if (strcmp(flag, "h") == 0) {
                    if (value) report_error(0, 0, "Flag '-h' doesn't take a value");
                    else {
                        printf("usage: pxi \033[1m[operations]\033[0m <source> ...\n");
                        printf("operations:\n"
                               "  \033[1m-h  --help\033[0m\t\t\tDisplay this information\n"
                               "  \033[1m-v  --version\033[0m\t\t\tDisplay compiler version information\n"
                               "  \033[1m-w  --write\033[0m\t\t\tDisplay code analysis\n"
                               "  \033[1m-t  --time\033[0m\t\t\tTime the execution of each subprocess\n"
                               "  \033[1m-n  --name\033[0m=[name]\t\tSet the name of your program\n"
                               "  \033[1m-s  --set-version\033[0m=[version]\tSet the compile version\n"
                               "  \033[1m-o  --asmlang\033[0m=[asm/arch]\tCompile into the Assembler language\n"
                               "  \033[1m-q  --binary\033[0m <source>\t\tCompile to binary file\n"
                               "  \033[1m-c  --compile\033[0m <source>\tCompile and assemble\n");
                        free_error_manager();
                        return 0;
                    }
                } else if (strcmp(flag, "v") == 0) {
                    if (value) report_error(0, 0, "Flag '-v' doesn't take a value");
                    else {
                        printf("LOGO\n"
                               "\n"
                               "pxi \033[1m%s - %s\033[0m\n"
                               "This is being developed by AIV\n"
                               "This free software is distributed under thre MIT General Public License\n", 
                               VERSION, DATE);
                        free_error_manager();
                        return 0;
                    }
                } else if (strcmp(flag, "w") == 0) {
                    if (value) report_error(0, 0, "Flag '-w' doesn't take a value");
                    else write_log = true;
                } else if (strcmp(flag, "t") == 0) {
                    if (value) report_error(0, 0, "Flag '-t' doesn't take a value");
                    else {
                        time_flag = true;
                        fprintf(stderr, "\033[93mWARNING\033[0m: timing is not implemented yet\n");
                    }
                } else if (strcmp(flag, "n") == 0) {
                    if (!value) report_error(0, 0, "Flag '-n' requires a value");
                    else {
                        program_name = value;
                        fprintf(stderr, "\033[93mWARNING\033[0m: program name setting is not implemented yet\n");
                    }
                } else if (strcmp(flag, "s") == 0) {
                    if (!value) report_error(0, 0, "Flag '-s' requires a value");
                    else version_str = value;
                } else if (strcmp(flag, "o") == 0) {
                    if (mode_set) report_error(0, 0, "Multiple mode flags specified");
                    else if (!value) report_error(0, 0, "Flag '-o' requires a value");
                    else {
                        main_mode = MODE_COMPILE_ASM;
                        asm_type = value;
                        mode_set = true;
                    }
                } else if (strcmp(flag, "q") == 0) {
                    if (mode_set) report_error(0, 0, "Multiple mode flags specified");
                    else if (value) report_error(0, 0, "Flag '-q' doesn't take a value");
                    else {
                        main_mode = MODE_BIN_COMPILE;
                        mode_set = true;
                    }
                } else if (strcmp(flag, "c") == 0) {
                    if (mode_set) report_error(0, 0, "Multiple mode flags specified");
                    else if (value) report_error(0, 0, "Flag '-c' doesn't take a value");
                    else {
                        main_mode = MODE_COMPILE;
                        mode_set = true;
                    }
                } else report_error(0, 0, "unknown flag: -%s", flag);
            }
        } else {
            // Non-flag argument (filename)
            if (filename) report_error(0, 0, "Multiple filenames specified: '%s' and '%s'", filename, argv[i]);
            else filename = argv[i];
        }
    }

    // Validation checks
    if (!filename) report_error(0, 0, "no input file specified");

    // Output timing warning if enabled
    if (time_flag) fprintf(stderr, "\033[93mWARNING\033[0m: timing functionality is not implemented yet\n");

    // Output program name warning if set
    if (program_name) fprintf(stderr, "\033[93mWARNING\033[0m: program name '%s' is set but not used\n", program_name);

    // Check for errors before proceeding
    if (has_errors()) {
        print_errors();
        free_error_manager();
        return 1;
    }

    // Handle mode-specific outputs (same as before)
    if (main_mode == MODE_BIN_COMPILE) printf("bin\n");
    else if (main_mode == MODE_COMPILE_ASM) {
        if (asm_type) {
            char* slash = strchr(asm_type, '/');
            if (slash) {
                *slash = '\0';
                printf("%s\t|\t%s\n", asm_type, slash + 1);
            } else printf("%s\t|\t(unknown)\n", asm_type);
        } else printf("PAS\t|\tx86_64\n");
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
                if (token->type == TOKEN_ID             ||
                    token->type == TOKEN_NUMBER         ||
                    token->type == TOKEN_STRING         ||
                    token->type == TOKEN_CHAR           ||
                    token->type == TOKEN_TYPE           ||
                    token->type == TOKEN_MODIFIER       ||
                    token->type == TOKEN_ERROR          ||
                    token->type == TOKEN_PREPROC_MACRO  ||
                    token->type == TOKEN_OUTSIDE_BUILD  ||
                    token->type == TOKEN_OUTSIDE_CODE) {
                    printf("%s:%s", token_names[token->type], token->value);
                } else printf("%s", token_names[token->type]);
                if (i < token_counts[line] - 1) printf(" ");
            }
            printf("]\n");
        }

        for (int i = 0; i <= max_line; i++) free(lines[i]);
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
            printf("\n\033[93mATTENTION:\033[0m \033[4mabstract syntax tree:\033[0m\n");
            print_ast(ast);
        }
    }

    if (has_errors()) {
        printf("\n\033[93mATTENTION:\033[0m \033[4mcompilation errors:\033[0m\n");
        print_errors();
    }

    if (ast) free_ast(ast);
    free_lexer(lexer);
    free(buffer_file);
    free_error_manager();
     
    return 0;
}
