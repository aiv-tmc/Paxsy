#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stddef.h>

typedef struct PaxsyConfig {
    /* project */
    char *name;
    char *version;
    char *description;
    char **authors;
    size_t authors_count;

    /* build */
    char *target;            /* "exe", "elf", "asm", "obj", "static", "shared", "apk", "reakt" */
    int optimization;        /* 0,1,2; -1 for "s" (size) */
    bool debug;
    bool emit_asm;
    bool recursive;
    char *out_dir;
    char *build_dir;
    char *debug_dir;

    /* compiler */
    char **compiler_flags;
    size_t compiler_flags_count;
    char **linker_flags;
    size_t linker_flags_count;

    /* assembly */
    char *arch;              /* "aarch64", "x86_64", or empty for auto-detection */

    /* debug */
    bool builtin;
    char **breakpoints;
    size_t breakpoints_count;
    char **watchpoints;
    size_t watchpoints_count;

    /* test */
    char *test_filter;
    bool test_verbose;

    /* dependencies */
    char **dep_names;
    char **dep_versions;
    size_t deps_count;

    /* modules */
    char **module_aliases;
    char **module_paths;
    size_t modules_count;

    /* scripts */
    char *pre_build;
    char *post_build;
    char *pre_clean;
} PaxsyConfig;

/*
 * Initialise a config structure with default values.
 * Caller must call config_free() to release resources.
 */
void config_init_defaults(PaxsyConfig *cfg);

/*
 * Load configuration from a project directory.
 * Looks for <project>.toml and merges with defaults.
 * Returns 0 on success, -1 on error (errors reported via errhandler).
 */
int config_load_from_project(const char *project_path, PaxsyConfig *cfg);

/*
 * Apply overrides from command-line options.
 * The arguments are parsed from the command line (excluding the command name and path).
 * Supported: --target, --optimize, --debug, --emit-asm, --recursive, --profile, --Wextra, --Werror, --ignore-warnings.
 * Also handles --profile <name>.
 * Returns 0 on success, -1 on error.
 */
int config_apply_command_line(PaxsyConfig *cfg, int argc, char **argv);

/*
 * Apply environment variable overrides.
 * Reads PAXSY_TARGET, PAXSY_OPT_LEVEL, PAXSY_DEBUG_DIR.
 * Returns 0.
 */
void config_apply_env(PaxsyConfig *cfg);

/*
 * Apply a named profile (from [profiles] section) to the build settings.
 * Overrides only [build] fields.
 * Returns 0 if profile found and applied, -1 if not found.
 */
int config_apply_profile(PaxsyConfig *cfg, const char *profile_name);

/*
 * Free all dynamically allocated resources in the config.
 */
void config_free(PaxsyConfig *cfg);

/*
 * Set a string field in config, freeing old value if any.
 * Used internally and by build_project for mode overrides.
 */
void config_set_string(char **field, const char *value);

/*
 * Detect the host architecture if arch is not set.
 * Fills cfg->arch with a string like "aarch64" or "x86_64".
 * If detection fails, defaults to "aarch64".
 */
void config_detect_arch(PaxsyConfig *cfg);

/*
 * Return the executable name with the appropriate suffix for the platform.
 * For Windows, adds ".exe"; for others, returns the name as is.
 * The returned string is statically allocated; do not free.
 */
const char *config_get_exe_suffix(void);

/*
 * Build the full executable path: out_dir/name + suffix.
 * The result is written into a buffer of size at least PATH_MAX*2.
 */
void config_build_exe_path(const PaxsyConfig *cfg, const char *proj_name, char *out_path, size_t out_size);

#endif
