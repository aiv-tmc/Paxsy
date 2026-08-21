#include "toml.h"
#include "../interfaces/errhandler/errhandler.h"
#include "../interfaces/utils/memory.h"
#include "../interfaces/utils/str.h"
#include "../interfaces/utils/file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * Strip trailing whitespace and carriage returns from a line (in‑place).
 * Returns the modified string pointer.
 */
static char *strip_trailing_whitespace(char *line) {
    size_t len = strlen(line);
    while (len > 0 && (line[len-1] == ' ' || line[len-1] == '\t' || line[len-1] == '\r' || line[len-1] == '\n')) {
        line[len-1] = '\0';
        len--;
    }
    return line;
}

/*
 * Trim leading and trailing whitespace from a string (in‑place).
 * Returns a pointer to the trimmed part (may be inside the original).
 */
static char *trim_whitespace(char *str) {
    while (*str == ' ' || *str == '\t') str++;
    char *end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t')) end--;
    end[1] = '\0';
    return str;
}

/*
 * Parse a double‑quoted string literal, handling simple escape sequences.
 * Returns a newly allocated string, or NULL on error.
 * The endptr is set to the character after the closing quote.
 */
static char *parse_string(const char *start, const char **endptr) {
    if (*start != '"') return NULL;
    start++;
    const char *p = start;
    while (*p && !(*p == '"' && *(p-1) != '\\')) p++;
    if (*p != '"') return NULL;
    size_t len = p - start;
    char *result = (char*)memory_allocate_zero(len + 1);
    if (!result) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (start[i] == '\\' && i+1 < len) {
            switch (start[i+1]) {
                case 'n': result[j++] = '\n'; break;
                case 't': result[j++] = '\t'; break;
                case 'r': result[j++] = '\r'; break;
                case '\\': result[j++] = '\\'; break;
                case '"': result[j++] = '"'; break;
                default: result[j++] = start[i+1]; break;
            }
            i++;
        } else {
            result[j++] = start[i];
        }
    }
    result[j] = '\0';
    *endptr = p + 1;
    return result;
}

/*
 * Parse a bare (unquoted) string until whitespace or comment.
 * Returns a newly allocated string, or NULL on failure.
 */
static char *parse_bare_string(const char *start, const char **endptr) {
    const char *p = start;
    while (*p && !isspace((unsigned char)*p) && *p != '#') p++;
    size_t len = p - start;
    char *result = (char*)memory_allocate_zero(len + 1);
    if (!result) return NULL;
    memcpy(result, start, len);
    result[len] = '\0';
    *endptr = p;
    return result;
}

/*
 * Parse a decimal integer from a string.
 * Returns 0 if the string is not a valid integer.
 */
static long long parse_integer(const char *str) {
    char *end;
    long long val = strtoll(str, &end, 10);
    if (*end != '\0') return 0;
    return val;
}

/*
 * Parse a value from a string. Handles booleans, quoted strings, integers,
 * and bare strings. For arrays, only the raw content is stored (without brackets).
 * Returns a newly allocated string representation, or NULL on error.
 */
static char *parse_value(const char *value_str, const char **endptr) {
    value_str = trim_whitespace((char*)value_str);
    if (!*value_str) return NULL;

    if (strcmp(value_str, "true") == 0 || strcmp(value_str, "false") == 0) {
        *endptr = value_str + strlen(value_str);
        return u__strduplic(value_str);
    }

    if (*value_str == '"') {
        const char *end;
        char *result = parse_string(value_str, &end);
        if (result) *endptr = end;
        return result;
    }

    const char *p = value_str;
    if (*p == '-' || *p == '+') p++;
    while (*p && isdigit((unsigned char)*p)) p++;
    if (*p == '\0' || isspace((unsigned char)*p) || *p == '#') {
        *endptr = p;
        return u__strduplic(value_str);
    }

    return parse_bare_string(value_str, endptr);
}

/*
 * Parse a line of the form "key = value" or "key = [ ... ]".
 * Returns 1 on success, 0 on error or if line is empty/comment.
 * On success, *out_key and *out_value are set to newly allocated strings.
 */
static int parse_key_value(const char *line, char **out_key, char **out_value) {
    char *line_copy = u__strduplic(line);
    if (!line_copy) return 0;
    char *p = line_copy;

    while (*p == ' ' || *p == '\t') p++;
    if (*p == '#' || *p == '\0') {
        memory_free_safe((void**)&line_copy);
        return 0;
    }

    char *eq = strchr(p, '=');
    if (!eq) {
        memory_free_safe((void**)&line_copy);
        return 0;
    }
    *eq = '\0';
    char *key = trim_whitespace(p);
    if (!*key) {
        memory_free_safe((void**)&line_copy);
        return 0;
    }

    char *value_start = eq + 1;
    value_start = trim_whitespace(value_start);
    char *comment = strchr(value_start, '#');
    if (comment) *comment = '\0';
    value_start = trim_whitespace(value_start);

    if (!*value_start) {
        memory_free_safe((void**)&line_copy);
        return 0;
    }

    const char *endptr;
    char *value = NULL;
    if (*value_start == '[') {
        char *close = strrchr(value_start, ']');
        if (!close) {
            memory_free_safe((void**)&line_copy);
            return 0;
        }
        *close = '\0';
        value = u__strduplic(value_start + 1);
        if (!value) {
            memory_free_safe((void**)&line_copy);
            return 0;
        }
    } else {
        value = parse_value(value_start, &endptr);
        if (!value) {
            memory_free_safe((void**)&line_copy);
            return 0;
        }
    }

    *out_key = u__strduplic(key);
    *out_value = value;
    memory_free_safe((void**)&line_copy);
    if (!*out_key || !*out_value) {
        memory_free_safe((void**)out_key);
        memory_free_safe((void**)out_value);
        return 0;
    }
    return 1;
}

/*
 * Add an entry to a section (insert at head).
 */
static void add_entry(TomlSection *section, const char *key, const char *value) {
    TomlEntry *entry = (TomlEntry*)memory_allocate_zero(sizeof(TomlEntry));
    if (!entry) return;
    entry->key = u__strduplic(key);
    entry->value = u__strduplic(value);
    entry->next = section->entries;
    section->entries = entry;
}

/*
 * Find a section by name, or create it if it does not exist.
 * Returns the section pointer, or NULL on allocation failure.
 */
static TomlSection *find_or_create_section(TomlSection **head, const char *name) {
    TomlSection *s = *head;
    while (s) {
        if (strcmp(s->name, name) == 0) return s;
        s = s->next;
    }
    TomlSection *new_s = (TomlSection*)memory_allocate_zero(sizeof(TomlSection));
    if (!new_s) return NULL;
    new_s->name = u__strduplic(name);
    new_s->next = *head;
    *head = new_s;
    return new_s;
}

/*
 * Parse a TOML file and return a list of sections.
 * The entire file is read at once for performance.
 * Returns NULL on error (file not found, parse error, memory failure).
 * The caller must call toml_free_sections() to free the returned structure.
 */
TomlSection *toml_parse_file(const char *path) {
    char *content = read_entire_file(path);
    if (!content) {
        errhandler__report_error(0, 0, "toml", "Cannot open config file: %s", path);
        return NULL;
    }

    TomlSection *sections = NULL;
    TomlSection *current_section = NULL;
    char *line_start = content;
    int line_num = 0;

    while (*line_start) {
        char *line_end = strchr(line_start, '\n');
        if (!line_end) line_end = line_start + strlen(line_start);
        char saved = *line_end;
        *line_end = '\0';
        line_num++;

        char *clean = strip_trailing_whitespace(line_start);
        char *trimmed = trim_whitespace(clean);

        if (*trimmed != '\0' && *trimmed != '#') {
            if (*trimmed == '[') {
                char *end = strchr(trimmed, ']');
                if (!end) {
                    errhandler__report_error(0, line_num, "toml", "Unclosed section header");
                } else {
                    *end = '\0';
                    char *name = trim_whitespace(trimmed + 1);
                    if (!*name) {
                        errhandler__report_error(0, line_num, "toml", "Empty section name");
                    } else {
                        current_section = find_or_create_section(&sections, name);
                        if (!current_section) {
                            errhandler__report_error(0, line_num, "toml", "Memory allocation failed for section");
                            free(content);
                            toml_free_sections(sections);
                            return NULL;
                        }
                    }
                }
            } else {
                if (!current_section) {
                    errhandler__report_error(0, line_num, "toml", "Key-value outside any section");
                } else {
                    char *key, *value;
                    if (parse_key_value(trimmed, &key, &value)) {
                        add_entry(current_section, key, value);
                        memory_free_safe((void**)&key);
                        memory_free_safe((void**)&value);
                    } else {
                        errhandler__report_error(0, line_num, "toml", "Failed to parse key-value pair");
                    }
                }
            }
        }

        if (saved == '\0') break;
        *line_end = saved;
        line_start = line_end + 1;
    }

    free(content);
    return sections;
}

/*
 * Free all sections and their entries.
 */
void toml_free_sections(TomlSection *sections) {
    while (sections) {
        TomlSection *next_sec = sections->next;
        TomlEntry *entry = sections->entries;
        while (entry) {
            TomlEntry *next_entry = entry->next;
            memory_free_safe((void**)&entry->key);
            memory_free_safe((void**)&entry->value);
            memory_free_safe((void**)&entry);
            entry = next_entry;
        }
        memory_free_safe((void**)&sections->name);
        memory_free_safe((void**)&sections);
        sections = next_sec;
    }
}

/*
 * Look up a value for a given key in a specific section.
 * Returns the value string (owned by the section) or NULL if not found.
 */
const char *toml_get_value(const TomlSection *sections, const char *section_name, const char *key) {
    while (sections) {
        if (strcmp(sections->name, section_name) == 0) {
            TomlEntry *entry = sections->entries;
            while (entry) {
                if (strcmp(entry->key, key) == 0) {
                    return entry->value;
                }
                entry = entry->next;
            }
        }
        sections = sections->next;
    }
    return NULL;
}

/*
 * Parse a comma‑separated list of strings (from an array value).
 * Fills out_array with newly allocated strings.
 * Returns the number of items, or -1 on error.
 */
int toml_get_string_array(const TomlSection *sections, const char *section_name, const char *key, char ***out_array) {
    const char *raw = toml_get_value(sections, section_name, key);
    if (!raw) return 0;

    char *copy = u__strduplic(raw);
    if (!copy) return -1;
    int count = 0;
    char *p = copy;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ',') { p++; continue; }
        if (*p == '\0') break;
        char *item_start = p;
        while (*p && *p != ',') p++;
        char *item_end = p;
        while (item_end > item_start && (*(item_end-1) == ' ' || *(item_end-1) == '\t')) item_end--;
        if (item_end > item_start) {
            size_t len = item_end - item_start;
            char *item = (char*)memory_allocate_zero(len + 1);
            if (!item) {
                memory_free_safe((void**)&copy);
                return -1;
            }
            memcpy(item, item_start, len);
            item[len] = '\0';
            char **new_arr = (char**)memory_reallocate_zero(*out_array, count * sizeof(char*), (count + 1) * sizeof(char*));
            if (!new_arr) {
                memory_free_safe((void**)&item);
                memory_free_safe((void**)&copy);
                return -1;
            }
            *out_array = new_arr;
            (*out_array)[count] = item;
            count++;
        }
        if (*p == ',') p++;
    }
    memory_free_safe((void**)&copy);
    return count;
}

/*
 * Parse a comma‑separated list of integers (from an array value).
 * Fills out_array with long long values.
 * Returns the number of items, or -1 on error.
 */
int toml_get_int_array(const TomlSection *sections, const char *section_name, const char *key, long long **out_array) {
    const char *raw = toml_get_value(sections, section_name, key);
    if (!raw) return 0;

    char *copy = u__strduplic(raw);
    if (!copy) return -1;
    int count = 0;
    char *p = copy;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ',') { p++; continue; }
        if (*p == '\0') break;
        char *item_start = p;
        while (*p && *p != ',') p++;
        char *item_end = p;
        while (item_end > item_start && (*(item_end-1) == ' ' || *(item_end-1) == '\t')) item_end--;
        if (item_end > item_start) {
            size_t len = item_end - item_start;
            char *item_str = (char*)memory_allocate_zero(len + 1);
            if (!item_str) {
                memory_free_safe((void**)&copy);
                return -1;
            }
            memcpy(item_str, item_start, len);
            item_str[len] = '\0';
            long long val = parse_integer(item_str);
            memory_free_safe((void**)&item_str);
            long long *new_arr = (long long*)memory_reallocate_zero(*out_array, count * sizeof(long long), (count + 1) * sizeof(long long));
            if (!new_arr) {
                memory_free_safe((void**)&copy);
                return -1;
            }
            *out_array = new_arr;
            (*out_array)[count] = val;
            count++;
        }
        if (*p == ',') p++;
    }
    memory_free_safe((void**)&copy);
    return count;
}
