#ifndef ERROR_MANAGER_H
#define ERROR_MANAGER_H

#include <stdbool.h>

/* Function to report an error at a specific line and column with formatted message */
void report_error(int line, int column, const char* format, ...);

/* Function to report a warning at a specific line and column with formatted message */
void report_warning(int line, int column, const char* format, ...);

/* Print all accumulated error messages */
void print_errors(void);

/* Print all accumulated warning messages */
void print_warnings(void);

/* Check if any errors have been reported */
bool has_errors(void);

/* Check if any warnings have been reported */
bool has_warnings(void);

/* Free all allocated memory used by the error manager */
void free_error_manager(void);

#endif

