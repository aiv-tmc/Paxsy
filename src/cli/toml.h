#ifndef TOML_H
#define TOML_H

#include <stddef.h>
#include <stdbool.h>

/*
 * A simple key-value store for a TOML table.
 * Each entry stores the key as a string and the value as a dynamically allocated string.
 */
typedef struct TomlEntry {
    char *key;
    char *value;   /* value stored as string; for booleans, "true"/"false"; for integers, decimal string */
    struct TomlEntry *next;
} TomlEntry;

/*
 * A TOML document is a collection of sections (tables).
 * Each section is identified by its name (e.g., "project", "build.compiler") and contains entries.
 */
typedef struct TomlSection {
    char *name;
    TomlEntry *entries;
    struct TomlSection *next;
} TomlSection;

/*
 * Parse a TOML file and return a list of sections.
 * Returns NULL on error (file not found, parse error, memory failure).
 * The caller must call toml_free_sections() to free the returned structure.
 */
TomlSection *toml_parse_file(const char *path);

/*
 * Free all sections and their entries.
 */
void toml_free_sections(TomlSection *sections);

/*
 * Look up a value for a given key in a specific section.
 * Returns the value string (owned by the section) or NULL if not found.
 */
const char *toml_get_value(const TomlSection *sections, const char *section_name, const char *key);

/*
 * Look up a string array (value is of the form [ "a", "b" ]) and fill the provided array.
 * Returns the number of elements parsed, or -1 on error.
 */
int toml_get_string_array(const TomlSection *sections, const char *section_name, const char *key, char ***out_array);

/*
 * Look up an integer array.
 * Returns the number of elements parsed, or -1 on error.
 */
int toml_get_int_array(const TomlSection *sections, const char *section_name, const char *key, long long **out_array);

#endif
