#include "error_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

/* Structure to store error/warning information */
typedef struct {
    char* message;      // Formatted message string
    int line;           // Line number where issue occurred (-1 if not applicable)
    int column;         // Column number where issue occurred (-1 if not applicable)
    bool is_warning;    // Flag indicating if this is a warning (true) or error (false)
} ErrorEntry;

/* Static global structure holding all error/warning records */
static struct {
    ErrorEntry* entries;    // Dynamic array of error entries
    int count;              // Current number of entries
    int capacity;           // Current capacity of the entries array
    int error_count;        // Total number of errors
    int warning_count;      // Total number of warnings
} error_manager = { NULL, 0, 0, 0, 0 };

/* Internal function to handle both errors and warnings */
static void vreport_issue(int line, int column, bool is_warning, const char* format, va_list args)
{
    /* Initialize array if this is the first error */
    if (error_manager.capacity == 0) {
        error_manager.capacity = 10;
        error_manager.entries = malloc(error_manager.capacity * sizeof(ErrorEntry));
    }

    /* Double capacity if array is full */
    if (error_manager.count >= error_manager.capacity) {
        error_manager.capacity *= 2;
        error_manager.entries = realloc(error_manager.entries, error_manager.capacity * sizeof(ErrorEntry));
    }

    /* Determine required buffer size for formatted message */
    va_list args_len;
    va_copy(args_len, args);
    int len = vsnprintf(NULL, 0, format, args_len);
    va_end(args_len);

    /* Allocate and create formatted message string */
    char* buffer = malloc(len + 1);
    va_list args_format;
    va_copy(args_format, args);
    vsnprintf(buffer, len + 1, format, args_format);
    va_end(args_format);

    /* Store error entry in array */
    ErrorEntry* entry = &error_manager.entries[error_manager.count];
    entry->line = line;
    entry->column = column;
    entry->message = buffer;
    entry->is_warning = is_warning;

    /* Update counters */
    error_manager.count++;
    if (is_warning) {
        error_manager.warning_count++;
    } else {
        error_manager.error_count++;
    }
}

/* Public function to report an error */
void report_error(int line, int column, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vreport_issue(line, column, false, format, args);
    va_end(args);
}

/* Public function to report a warning */
void report_warning(int line, int column, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vreport_issue(line, column, true, format, args);
    va_end(args);
}

/* Print all errors with color formatting */
void print_errors(void)
{
    for (int i = 0; i < error_manager.count; i++) {
        ErrorEntry* entry = &error_manager.entries[i];
        if (!entry->is_warning) {
            if (entry->line > 0) {
                /* Print with line/column information */
                fprintf(stderr, "\033[1;91mERROR\033[0m: \033[31m%d\033[0m:\033[31m%d\033[0m: %s\n", entry->line, entry->column, entry->message);
            } else {
                /* Print without location information */
                fprintf(stderr, "\033[1;91mFATAL\033[0m: %s\n", entry->message);
            }
        }
    }
}

/* Print all warnings with color formatting */
void print_warnings(void)
{
    for (int i = 0; i < error_manager.count; i++) {
        ErrorEntry* entry = &error_manager.entries[i];
        if (entry->is_warning) {
            if (entry->line > 0) {
                /* Print with line/column information */
                fprintf(stderr, "\033[1;93mWARNING\033[0m: \033[33m%d\033[0m:\033[33m%d\033[0m: %s\n", entry->line, entry->column, entry->message);
            } else {
                /* Print without location information */
                fprintf(stderr, "\033[1;93mWARNING\033[0m: %s\n", entry->message);
            }
        }
    }
}

/* Check if any errors were reported */
bool has_errors(void)
{
    return error_manager.error_count > 0;
}

/* Check if any warnings were reported */
bool has_warnings(void)
{
    return error_manager.warning_count > 0;
}

/* Clean up all allocated memory */
void free_error_manager(void)
{
    /* Free all message strings */
    for (int i = 0; i < error_manager.count; i++) {
        free(error_manager.entries[i].message);
    }
    /* Free entries array and reset state */
    free(error_manager.entries);
    error_manager.entries = NULL;
    error_manager.count = 0;
    error_manager.capacity = 0;
    error_manager.error_count = 0;
    error_manager.warning_count = 0;
}

