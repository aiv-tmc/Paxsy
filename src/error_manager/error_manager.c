#include "error_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// Structure for storing error/warning information
typedef struct {
    char* message;      // Error/warning message
    int line;           // Line number where occurred
    int column;         // Column number where occurred
    bool is_warning;    // Flag indicating warning
} ErrorEntry;

// Error manager state
static struct {
    ErrorEntry* entries;     // Array of error/warning entries
    int count;               // Total entries count
    int capacity;            // Current storage capacity
    int error_count;         // Number of errors
    int warning_count;       // Number of warnings
} error_manager = {0};

// Reports an error
void report_error(int line, int column, const char* format, ...) {
    // Allocate initial capacity if needed
    if (error_manager.capacity == 0) {
        error_manager.capacity = 10;
        error_manager.entries = malloc(error_manager.capacity * sizeof(ErrorEntry));
    }

    // Expand storage if full
    if (error_manager.count >= error_manager.capacity) {
        error_manager.capacity *= 2;
        error_manager.entries = realloc(error_manager.entries, 
                                      error_manager.capacity * sizeof(ErrorEntry));
    }

    // Format message
    va_list args;
    va_start(args, format);
    int len = vsnprintf(NULL, 0, format, args);
    va_end(args);
    
    char* buffer = malloc(len + 1);
    va_start(args, format);
    vsnprintf(buffer, len + 1, format, args);
    va_end(args);

    // Store error entry
    ErrorEntry* entry = &error_manager.entries[error_manager.count++];
    entry->line = line;
    entry->column = column;
    entry->message = buffer;
    entry->is_warning = false;
    error_manager.error_count++;
}

// Reports a warning
void report_warning(int line, int column, const char* format, ...) {
    // Allocate initial capacity if needed
    if (error_manager.capacity == 0) {
        error_manager.capacity = 10;
        error_manager.entries = malloc(error_manager.capacity * sizeof(ErrorEntry));
    }

    // Expand storage if full
    if (error_manager.count >= error_manager.capacity) {
        error_manager.capacity *= 2;
        error_manager.entries = realloc(error_manager.entries, 
                                      error_manager.capacity * sizeof(ErrorEntry));
    }

    // Format message
    va_list args;
    va_start(args, format);
    int len = vsnprintf(NULL, 0, format, args);
    va_end(args);
    
    char* buffer = malloc(len + 1);
    va_start(args, format);
    vsnprintf(buffer, len + 1, format, args);
    va_end(args);

    // Store warning entry
    ErrorEntry* entry = &error_manager.entries[error_manager.count++];
    entry->line = line;
    entry->column = column;
    entry->message = buffer;
    entry->is_warning = true;
    error_manager.warning_count++;
}

// Prints all accumulated errors
void print_errors() {
    for (int i = 0; i < error_manager.count; i++) {
        ErrorEntry* entry = &error_manager.entries[i];
        if (!entry->is_warning) {
            if (entry->line > 0) {
                fprintf(stderr, "\033[1;91mERROR\033[0m: \033[31m%d\033[0m:\033[31m%d\033[0m: %s\n", 
                        entry->line, entry->column, entry->message);
            } else {
                fprintf(stderr, "\033[1;91mFATAL\033[0m: %s\n", entry->message);
            }
        }
    }
}

// Prints all accumulated warnings
void print_warnings() {
    for (int i = 0; i < error_manager.count; i++) {
        ErrorEntry* entry = &error_manager.entries[i];
        if (entry->is_warning) {
            if (entry->line > 0) {
                fprintf(stderr, "\033[1;93mWARNING\033[0m: \033[33m%d\033[0m:\033[33m%d\033[0m: %s\n", 
                        entry->line, entry->column, entry->message);
            } else {
                fprintf(stderr, "\033[1;93mWARNING\033[0m: %s\n", entry->message);
            }
        }
    }
}

// Checks if any errors exist
bool has_errors() { 
    return error_manager.error_count > 0; 
}

// Checks if any warnings exist
bool has_warnings() { 
    return error_manager.warning_count > 0; 
}

// Frees all error manager resources
void free_error_manager() {
    for (int i = 0; i < error_manager.count; i++) {
        free(error_manager.entries[i].message);
    }
    free(error_manager.entries);
    error_manager.entries = NULL;
    error_manager.count = 0;
    error_manager.capacity = 0;
    error_manager.error_count = 0;
    error_manager.warning_count = 0;
}

