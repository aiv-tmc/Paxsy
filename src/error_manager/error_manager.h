#ifndef ERROR_MANAGER_H
#define ERROR_MANAGER_H

#include <stdbool.h>

void report_error(int line, int column, const char* format, ...);
void print_errors();
bool has_errors();
void free_error_manager();

#endif

