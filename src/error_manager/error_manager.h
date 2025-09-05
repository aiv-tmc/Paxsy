#ifndef ERROR_MANAGER_H
#define ERROR_MANAGER_H

#include <stdbool.h>

void report_error(int line, int column, const char* format, ...);
void report_warning(int line, int column, const char* format, ...);

void print_errors();
void print_warnings();

bool has_errors();
bool has_warnings();

void free_error_manager();

#endif

