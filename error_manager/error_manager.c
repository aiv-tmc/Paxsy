#include "error_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef struct {
    char* message;
    int line;
    int column;
} ErrorEntry;

static struct {
    ErrorEntry* errors;
    int count;
    int capacity;
} error_manager = {0};

void report_error(int line, int column, const char* format, ...) {
    if (error_manager.capacity == 0) {
        error_manager.capacity = 10;
        error_manager.errors = malloc(error_manager.capacity * sizeof(ErrorEntry));
    }

    if (error_manager.count >= error_manager.capacity) {
        error_manager.capacity *= 2;
        error_manager.errors = realloc(error_manager.errors, 
                                     error_manager.capacity * sizeof(ErrorEntry));
    }

    va_list args;
    va_start(args, format);
    int len = vsnprintf(NULL, 0, format, args);
    va_end(args);
    
    char* buffer = malloc(len + 1);
    va_start(args, format);
    vsnprintf(buffer, len + 1, format, args);
    va_end(args);

    ErrorEntry* entry = &error_manager.errors[error_manager.count++];
    entry->line = line;
    entry->column = column;
    entry->message = buffer;
}

void print_errors() {
    for (int i = 0; i < error_manager.count; i++) {
        ErrorEntry* entry = &error_manager.errors[i];
        if (entry->line > 0) fprintf(stderr, "\033[1;91mERROR\033[0m: \033[31m%d\033[0m:\033[31m%d\033[0m: %s\n", entry->line, entry->column, entry->message);
        else fprintf(stderr, "\033[1;91mFATAL\033[0m: %s\n", entry->message);
    }
}

void print_warnings() {
    for (int i = 0; i < error_manager.count; i++) {
        ErrorEntry* entry = & error_manager.errors[i];
        if (entry->line > 0) fprintf(stderr, "\033[1;93mWARNING\033[0m: \033[33m%d\033[0m:\033[33m%d\033[0m: %s\n", entry->line, entry->column, entry->message);
        else fprintf(stderr, "\033[1;93mWARNING\033[0m: %s\n", entry->message);
    }
}

bool has_errors() { return error_manager.count > 0; }

void free_error_manager() {
    for (int i = 0; i < error_manager.count; i++) free(error_manager.errors[i].message);
    free(error_manager.errors);
    error_manager.errors = NULL;
    error_manager.count = 0;
    error_manager.capacity = 0;
}

