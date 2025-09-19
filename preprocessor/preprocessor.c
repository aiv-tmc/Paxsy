#include "preprocessor.h"
#include "../error_manager/error_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Macro definition structure */
typedef struct {
    char* name;               // Macro identifier
    char* value;              // Replacement text
    unsigned char has_args;   // Flag for function-like macros
    char** args;              // Argument names for function-like macros
    unsigned char argc;       // Number of arguments
} Macro;

/* Conditional compilation state */
typedef struct {
    unsigned char condition_met;  // Whether condition was true
    unsigned char else_seen;      // Whether #else was encountered
    unsigned char active;         // Whether this block is active
} ConditionalState;

/* File inclusion tracking */
typedef struct {
    char* filename;   // Name of included file
    char* content;    // Content of included file
    size_t pos;       // Current read position
    size_t line;      // Current line number
} IncludeFile;

/* Global state variables */
static Macro* macros = NULL;              // Dynamic array of macros
static size_t macro_count = 0;            // Number of macros
static ConditionalState* cond_stack = NULL; // Stack for #if conditions
static size_t cond_stack_size = 0;        // Size of condition stack
static IncludeFile* include_stack = NULL;  // Stack for included files
static size_t include_stack_size = 0;      // Size of include stack
static char* current_filename = NULL;      // Current file being processed
static size_t current_line = 1;            // Current line number

/* Adds a new macro or updates existing one */
static void add_macro(const char* name, const char* value, unsigned char has_args, char** args, unsigned char argc) {
    /* Check for existing macro */
    for (size_t i = 0; i < macro_count; i++) {
        if (strcmp(macros[i].name, name) == 0) {
            /* Update existing macro */
            free(macros[i].value);
            macros[i].value = value ? strdup(value) : NULL;
            macros[i].has_args = has_args;

            /* Free existing arguments */
            if (macros[i].args) {
                for (unsigned char j = 0; j < macros[i].argc; j++) free(macros[i].args[j]);
                free(macros[i].args);
            }
            macros[i].args = args;
            macros[i].argc = argc;
            return;
        }
    }

    /* Add new macro */
    macros = realloc(macros, sizeof(Macro) * (macro_count + 1));
    if (!macros) return;
    
    macros[macro_count].name = strdup(name);
    macros[macro_count].value = value ? strdup(value) : NULL;
    macros[macro_count].has_args = has_args;
    macros[macro_count].args = args;
    macros[macro_count].argc = argc;
    macro_count++;
}

/* Removes a macro definition */
static void remove_macro(const char* name) {
    for (size_t i = 0; i < macro_count; i++) {
        if (strcmp(macros[i].name, name) == 0) {
            /* Free macro resources */
            free(macros[i].name);
            free(macros[i].value);
            if (macros[i].args) {
                for (unsigned char j = 0; j < macros[i].argc; j++) free(macros[i].args[j]);
                free(macros[i].args);
            }
            /* Remove from array */
            if (i < macro_count - 1) {
                macros[i] = macros[macro_count - 1];
            }
            macro_count--;
            macros = realloc(macros, sizeof(Macro) * macro_count);
            break;
        }
    }
}

/* Finds a macro by name */
static const Macro* find_macro(const char* name) {
    for (size_t i = 0; i < macro_count; i++) {
        if (strcmp(macros[i].name, name) == 0) return &macros[i];
    }
    return NULL;
}

/* Pushes a new conditional compilation state */
static void push_conditional_state(unsigned char condition_met, unsigned char active) {
    cond_stack = realloc(cond_stack, sizeof(ConditionalState) * (cond_stack_size + 1));
    if (!cond_stack) return;
    cond_stack[cond_stack_size] = (ConditionalState){condition_met, 0, active};
    cond_stack_size++;
}

/* Pops the top conditional state */
static void pop_conditional_state() {
    if (cond_stack_size > 0) {
        cond_stack_size--;
        cond_stack = realloc(cond_stack, sizeof(ConditionalState) * cond_stack_size);
    }
}

/* Checks if current context is active for compilation */
static unsigned char current_conditional_active() {
    for (size_t i = 0; i < cond_stack_size; i++) {
        if (!cond_stack[i].active) return 0;
    }
    return 1;
}

/* Reads entire file into memory */
static char* read_file_contents(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char* content = malloc(size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(content, 1, size, file);
    content[read_size] = '\0';
    fclose(file);
    return content;
}

/* Pushes a new file onto include stack */
static void push_include_file(const char* filename, const char* content) {
    include_stack = realloc(include_stack, sizeof(IncludeFile) * (include_stack_size + 1));
    if (!include_stack) return;
    include_stack[include_stack_size] = (IncludeFile){
        .filename = strdup(filename),
        .content = strdup(content),
        .pos = 0,
        .line = 1
    };
    include_stack_size++;
}

/* Pops the top file from include stack */
static void pop_include_file() {
    if (include_stack_size > 0) {
        include_stack_size--;
        free(include_stack[include_stack_size].filename);
        free(include_stack[include_stack_size].content);
        include_stack = realloc(include_stack, sizeof(IncludeFile) * include_stack_size);
    }
}

/* Gets next character from current include file */
static int get_next_char(char* c) {
    while (include_stack_size > 0) {
        IncludeFile* current = &include_stack[include_stack_size - 1];
        if (current->content[current->pos] == '\0') {
            pop_include_file();
            continue;
        }
        *c = current->content[current->pos++];
        if (*c == '\n') current->line++;
        return 1;
    }
    return 0;
}

/* Expands arguments in function-like macros */
static char* expand_macro_args(const Macro* macro, char* args) {
    char* expanded = strdup(macro->value);
    char* arg_list[macro->argc];
    char* saveptr;
    char* token = strtok_r(args, ",", &saveptr);
    
    /* Parse arguments */
    for (unsigned char i = 0; i < macro->argc; i++) {
        arg_list[i] = token ? strdup(token) : NULL;
        token = strtok_r(NULL, ",", &saveptr);
    }

    /* Replace parameters with actual arguments */
    for (unsigned char i = 0; i < macro->argc; i++) {
        if (!arg_list[i]) continue;
        char* pos = expanded;
        size_t arg_len = strlen(macro->args[i]);
        size_t val_len = strlen(arg_list[i]);
        while ((pos = strstr(pos, macro->args[i])) != NULL) {
            /* Check for isolated identifier */
            if ((pos == expanded || !isalnum(pos[-1])) &&
                (!isalnum(pos[arg_len]))) {
                size_t prefix_len = pos - expanded;
                size_t new_size = strlen(expanded) + val_len - arg_len + 1;
                char* new_expanded = malloc(new_size);
                memcpy(new_expanded, expanded, prefix_len);
                memcpy(new_expanded + prefix_len, arg_list[i], val_len);
                strcpy(new_expanded + prefix_len + val_len, pos + arg_len);
                free(expanded);
                expanded = new_expanded;
                pos = new_expanded + prefix_len + val_len;
            } else {
                pos += arg_len;
            }
        }
        free(arg_list[i]);
    }
    return expanded;
}

/* Expands all macros in input string */
static char* expand_macros(const char* input) {
    char* result = strdup(input);
    size_t result_len = strlen(result);

    for (size_t i = 0; i < macro_count; i++) {
        char* pos = result;
        size_t name_len = strlen(macros[i].name);
        while ((pos = strstr(pos, macros[i].name)) != NULL) {
            /* Check for isolated identifier */
            if ((pos == result || !isalnum(pos[-1])) && 
                !isalnum(pos[name_len])) {
                if (macros[i].has_args) {
                    /* Handle function-like macro */
                    char* args_start = pos + name_len;
                    while (*args_start && isspace(*args_start)) args_start++;
                    if (*args_start == '(') {
                        char* args_end = args_start + 1;
                        int depth = 1;
                        while (*args_end && depth > 0) {
                            if (*args_end == '(') depth++;
                            else if (*args_end == ')') depth--;
                            args_end++;
                        }
                        if (depth == 0) {
                            *args_end = '\0';
                            char* expanded = expand_macro_args(&macros[i], args_start + 1);
                            size_t new_len = result_len + strlen(expanded) - (args_end - pos);
                            char* new_result = malloc(new_len + 1);
                            size_t prefix_len = pos - result;
                            memcpy(new_result, result, prefix_len);
                            memcpy(new_result + prefix_len, expanded, strlen(expanded));
                            strcpy(new_result + prefix_len + strlen(expanded), args_end + 1);
                            free(result);
                            free(expanded);
                            result = new_result;
                            result_len = new_len;
                            pos = new_result + prefix_len + strlen(expanded);
                            continue;
                        }
                    }
                }
                /* Handle object-like macro */
                size_t new_len = result_len + strlen(macros[i].value) - name_len;
                char* new_result = malloc(new_len + 1);
                size_t prefix_len = pos - result;
                memcpy(new_result, result, prefix_len);
                memcpy(new_result + prefix_len, macros[i].value, strlen(macros[i].value));
                strcpy(new_result + prefix_len + strlen(macros[i].value), pos + name_len);
                free(result);
                result = new_result;
                result_len = new_len;
                pos = new_result + prefix_len + strlen(macros[i].value);
                continue;
            }
            pos += name_len;
        }
    }
    return result;
}

/* Processes preprocessor directives */
static void process_directive(char* directive, char** output, size_t* output_pos, int* error) {
    char* saveptr;
    char* token = strtok_r(directive, " \t", &saveptr);
    if (!token) return;

    if (strcmp(token, "#include") == 0) {
        /* Handle file inclusion */
        char* filename = strtok_r(NULL, " \t", &saveptr);
        if (filename) {
            /* Remove quotes or angle brackets */
            size_t len = strlen(filename);
            if ((filename[0] == '"' && filename[len-1] == '"') ||
                (filename[0] == '<' && filename[len-1] == '>')) {
                filename[len-1] = '\0';
                char* content = read_file_contents(filename + 1);
                if (content) {
                    push_include_file(filename + 1, content);
                    free(content);
                } else {
                    report_error(0, 0, "Cannot open include file: %s", filename + 1);
                    *error = 1;
                }
            }
        }
    }
    else if (strcmp(token, "#define") == 0) {
        /* Handle macro definition */
        char* name = strtok_r(NULL, " \t(", &saveptr);
        if (name) {
            unsigned char has_args = 0;
            char** args = NULL;
            unsigned char argc = 0;
            
            /* Check for function-like macro */
            if (saveptr && saveptr[-1] == '(') {
                has_args = 1;
                char* args_str = strtok_r(NULL, ")", &saveptr);
                if (args_str) {
                    char* arg_saveptr;
                    char* arg = strtok_r(args_str, ",", &arg_saveptr);
                    while (arg) {
                        args = realloc(args, sizeof(char*) * (argc + 1));
                        args[argc++] = strdup(arg);
                        arg = strtok_r(NULL, ",", &arg_saveptr);
                    }
                }
            }
            
            char* value = strtok_r(NULL, "", &saveptr);
            add_macro(name, value, has_args, args, argc);
        }
    }
    else if (strcmp(token, "#undef") == 0) {
        /* Undefine macro */
        char* name = strtok_r(NULL, " \t", &saveptr);
        if (name) remove_macro(name);
    }
    else if (strcmp(token, "#ifdef") == 0) {
        /* Conditional compilation - if defined */
        char* name = strtok_r(NULL, " \t", &saveptr);
        unsigned char condition_met = name && find_macro(name);
        push_conditional_state(condition_met, current_conditional_active() && condition_met);
    }
    else if (strcmp(token, "#ifndef") == 0) {
        /* Conditional compilation - if not defined */
        char* name = strtok_r(NULL, " \t", &saveptr);
        unsigned char condition_met = name && !find_macro(name);
        push_conditional_state(condition_met, current_conditional_active() && condition_met);
    }
    else if (strcmp(token, "#else") == 0) {
        /* Alternate conditional branch */
        if (cond_stack_size > 0) {
            ConditionalState* state = &cond_stack[cond_stack_size - 1];
            if (!state->else_seen) {
                state->else_seen = 1;
                state->active = current_conditional_active() && !state->condition_met;
            }
        }
    }
    else if (strcmp(token, "#endif") == 0) {
        /* End conditional block */
        pop_conditional_state();
    }
    else if (strcmp(token, "#error") == 0) {
        /* Error directive */
        char* message = strtok_r(NULL, "", &saveptr);
        report_error(0, 0, "Error directive: %s", message);
        *error = 1;
    }
    else if (strcmp(token, "#line") == 0) {
        /* Line control directive */
        char* line_str = strtok_r(NULL, " \t", &saveptr);
        if (line_str) {
            current_line = atoi(line_str);
            char* filename = strtok_r(NULL, " \t", &saveptr);
            if (filename) {
                free(current_filename);
                current_filename = strdup(filename);
            }
        }
    }
}

/* Main preprocessing function */
char* preprocess(const char* input, const char* filename, int* error) {
    *error = 0;
    free(current_filename);
    current_filename = strdup(filename);
    current_line = 1;
    
    push_include_file(filename, input);
    
    size_t output_size = strlen(input) * 2;
    char* output = malloc(output_size);
    size_t output_pos = 0;
    
    unsigned char state = 0;  // FSM state
    char c;
    char directive[256];
    size_t directive_pos = 0;
    
    /* Finite state machine for processing */
    while (get_next_char(&c)) {
        if (output_pos >= output_size - 1) {
            output_size *= 2;
            output = realloc(output, output_size);
        }
        
        switch (state) {
            case 0:  // Normal processing
                if (c == '"') state = 1;          // Enter string literal
                else if (c == '\'') state = 2;    // Enter char literal
                else if (c == '/') state = 3;     // Possible comment
                else if (c == '#' && output_pos == 0) state = 7; // Preprocessor directive
                output[output_pos++] = c;
                break;
                
            case 1:  // Inside string literal
                output[output_pos++] = c;
                if (c == '"') state = 0;
                else if (c == '\\') get_next_char(&c) && (output[output_pos++] = c); // Escape sequence
                break;
                
            case 2:  // Inside char literal
                output[output_pos++] = c;
                if (c == '\'') state = 0;
                else if (c == '\\') get_next_char(&c) && (output[output_pos++] = c); // Escape sequence
                break;
                
            case 3:  // Possible comment
                if (c == '/') state = 4;  // Line comment
                else if (c == '*') state = 5; // Block comment
                else {
                    output[output_pos++] = c;
                    state = 0;
                }
                break;
                
            case 4:  // Line comment
                if (c == '\n') {
                    output[output_pos++] = c;
                    state = 0;
                }
                break;
                
            case 5:  // Block comment
                if (c == '*') state = 6; // Possible end of comment
                break;
                
            case 6:  // Possible end of block comment
                if (c == '/') state = 0; // End comment
                else if (c != '*') state = 5; // Still in comment
                break;
                
            case 7:  // Preprocessor directive
                if (c == '\n') {
                    directive[directive_pos] = '\0';
                    process_directive(directive, &output, &output_pos, error);
                    output[output_pos++] = c;
                    state = 0;
                } else if (directive_pos < sizeof(directive) - 1) {
                    directive[directive_pos++] = c;
                }
                break;
        }
    }
    
    output[output_pos] = '\0';
    char* expanded = expand_macros(output);
    free(output);
    
    /* Cleanup include stack */
    while (include_stack_size > 0) pop_include_file();
    return expanded;
}

