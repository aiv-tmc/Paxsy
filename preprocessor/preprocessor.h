#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include <stddef.h>

/* Preprocesses input source code, handling directives and macros */
char* preprocess(const char* input, const char* filename, int* error);

#endif

