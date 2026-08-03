#ifndef LITERALS_H
#define LITERALS_H

#include "../lexer/lexer.h"
#include <stdint.h>
#include <stdbool.h>

/*
 * Parses a numeric literal (integer or floating point) according to the language rules.
 * Supports binary (0b), octal (0o), decimal, and hexadecimal (0x) prefixes,
 * underscores as separators, and exponent notation for floating point.
 * Recognises 'U' suffix for unsigned integers and 'f' suffix for floats.
 *
 * Returns a token of type TOKEN_NUMBER_LITERAL, or TOKEN_ERRORCODE on failure.
 * The token's 'value' is the raw literal text including suffix.
 */
Token literal__parse_number(Lexer *lexer);

/*
 * Parses a character literal enclosed in single quotes. Escape sequences are processed.
 * The literal must contain exactly one character after escape expansion.
 * Returns a token of type TOKEN_CHAR_LITERAL, or TOKEN_ERRORCODE on failure.
 */
Token literal__parse_char(Lexer *lexer);

/*
 * Parses a string literal enclosed in double quotes. Escape sequences and embedded
 * newlines are supported. If the string contains an unescaped '{' followed by an
 * expression (until '}'), the token type becomes TOKEN_INTERPOLATED_STRING.
 * Returns a token of type TOKEN_STRING_LITERAL or TOKEN_INTERPOLATED_STRING,
 * or TOKEN_ERRORCODE on failure.
 */
Token literal__parse_string(Lexer *lexer);

/*
 * Parses a sequence of adjacent string and character literals separated only by
 * whitespace, concatenating them into a single string token.
 * If any component is interpolated, the resulting token is also interpolated.
 * Returns a token of type TOKEN_STRING_LITERAL or TOKEN_INTERPOLATED_STRING,
 * or TOKEN_ERRORCODE on failure.
 */
Token literal__parse_concatenated(Lexer *lexer);

#endif
