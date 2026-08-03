#define _POSIX_C_SOURCE 200809L
#include "config.h"
#include "toml.h"
#include "../interfaces/errhandler/errhandler.h"
#include "../interfaces/utils/memory.h"
#include "../interfaces/utils/str.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/utsname.h>

/*
 * Set a string field in config, freeing old value if any.
 */
void config_set_string(char **field, const char *value) {
    if (*field) {
        memory_free_safe((void**)field);
    }
    if (value) {
        *field = u__strduplic(value);
    } else {
        *field = NULL;
    }
}

/*
 * Set an array of strings.
 */
static void set_string_array(char ***arr, size_t *count, const char *const *values, size_t n) {
    if (*arr) {
        for (size_t i = 0; i < *count; i++) {
            memory_free_safe((void**)&(*arr)[i]);
        }
        memory_free_safe((void**)arr);
    }
    *count = 0;
    if (n == 0 || !values) return;
    *arr = (char**)memory_allocate_zero(n * sizeof(char*));
    if (!*arr) return;
    for (size_t i = 0; i < n; i++) {
        (*arr)[i] = u__strduplic(values[i]);
        if (!(*arr)[i]) {
            for (size_t j = 0; j < i; j++) memory_free_safe((void**)&(*arr)[j]);
            memory_free_safe((void**)arr);
            *arr = NULL;
            return;
        }
    }
    *count = n;
}

/*
 * Helper: read a string from a section, with default.
 */
static const char *get_string(const TomlSection *s, const char *section, const char *key, const char *def) {
    const char *val = toml_get_value(s, section, key);
    return val ? val : def;
}

/*
 * Helper: read a boolean from a section.
 */
static bool get_bool(const TomlSection *s, const char *section, const char *key, bool def) {
    const char *val = toml_get_value(s, section, key);
    if (!val) return def;
    if (strcmp(val, "true") == 0) return true;
    if (strcmp(val, "false") == 0) return false;
    return def;
}

/*
 * Helper: read an integer.
 */
static int get_int(const TomlSection *s, const char *section, const char *key, int def) {
    const char *val = toml_get_value(s, section, key);
    if (!val) return def;
    char *end;
    long v = strtol(val, &end, 10);
    if (*end != '\0') return def;
    return (int)v;
}

void config_init_defaults(PaxsyConfig *cfg) {
    memset(cfg, 0, sizeof(PaxsyConfig));
    cfg->name = NULL; // will be set from directory name
    cfg->version = u__strduplic("0.1.0");
    cfg->description = u__strduplic("");
    cfg->authors = NULL;
    cfg->authors_count = 0;

    cfg->target = u__strduplic("exe");
    cfg->optimization = 2;
    cfg->debug = false;
    cfg->emit_asm = false;
    cfg->recursive = false;
    cfg->out_dir = u__strduplic("target");
    cfg->build_dir = u__strduplic("build");
    cfg->debug_dir = u__strduplic("debug");

    cfg->compiler_flags = NULL;
    cfg->compiler_flags_count = 0;
    cfg->linker_flags = NULL;
    cfg->linker_flags_count = 0;

    cfg->arch = NULL;          // will be auto-detected if not set
    cfg->builtin = true;
    cfg->breakpoints = NULL;
    cfg->breakpoints_count = 0;
    cfg->watchpoints = NULL;
    cfg->watchpoints_count = 0;

    cfg->test_filter = u__strduplic("");
    cfg->test_verbose = false;

    cfg->dep_names = NULL;
    cfg->dep_versions = NULL;
    cfg->deps_count = 0;

    cfg->module_aliases = NULL;
    cfg->module_paths = NULL;
    cfg->modules_count = 0;

    cfg->pre_build = NULL;
    cfg->post_build = NULL;
    cfg->pre_clean = NULL;
}

int config_load_from_project(const char *project_path, PaxsyConfig *cfg) {
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) {
        errhandler__report_error(0, 0, "config", "getcwd failed: %s", strerror(errno));
        return -1;
    }

    if (project_path) {
        if (chdir(project_path) != 0) {
            errhandler__report_error(0, 0, "config", "Cannot change to directory: %s", project_path);
            return -1;
        }
    }

    /* Determine project name from current directory */
    char dir[PATH_MAX];
    if (!getcwd(dir, sizeof(dir))) {
        errhandler__report_error(0, 0, "config", "getcwd failed: %s", strerror(errno));
        chdir(cwd);
        return -1;
    }
    char *proj_name = strrchr(dir, '/');
    if (proj_name) proj_name++;
    else proj_name = dir;
    /* If config->name is already set (from command line?), we keep it; else use dir name */
    if (!cfg->name) {
        cfg->name = u__strduplic(proj_name);
    }

    /* Build config file path: <project>.toml */
    char config_path[PATH_MAX * 2];          // larger buffer to avoid truncation warnings
    snprintf(config_path, sizeof(config_path), "%s.toml", proj_name);

    TomlSection *sections = toml_parse_file(config_path);
    if (!sections) {
        /* If file doesn't exist, that's okay; we use defaults and emit warning */
        errhandler__report_error_ex(ERROR_LEVEL_WARNING, 0, 0, 1,
                                    "config", "Config file %s not found, using defaults", config_path);
    } else {
        /* [project] */
        const char *val;
        val = get_string(sections, "project", "name", NULL);
        if (val) config_set_string(&cfg->name, val);
        val = get_string(sections, "project", "version", NULL);
        if (val) config_set_string(&cfg->version, val);
        val = get_string(sections, "project", "description", NULL);
        if (val) config_set_string(&cfg->description, val);
        /* authors: array */
        char **authors = NULL;
        int count = toml_get_string_array(sections, "project", "authors", &authors);
        if (count > 0) {
            set_string_array(&cfg->authors, &cfg->authors_count, (const char*const*)authors, count);
            for (int i = 0; i < count; i++) memory_free_safe((void**)&authors[i]);
            memory_free_safe((void**)&authors);
        }

        /* [build] */
        val = get_string(sections, "build", "target", NULL);
        if (val) config_set_string(&cfg->target, val);
        int opt = get_int(sections, "build", "optimization", cfg->optimization);
        cfg->optimization = opt;
        cfg->debug = get_bool(sections, "build", "debug", cfg->debug);
        cfg->emit_asm = get_bool(sections, "build", "emit-asm", cfg->emit_asm);
        cfg->recursive = get_bool(sections, "build", "recursive", cfg->recursive);
        val = get_string(sections, "build", "out-dir", NULL);
        if (val) config_set_string(&cfg->out_dir, val);
        val = get_string(sections, "build", "build-dir", NULL);
        if (val) config_set_string(&cfg->build_dir, val);
        val = get_string(sections, "build", "debug-dir", NULL);
        if (val) config_set_string(&cfg->debug_dir, val);

        /* [build.compiler] */
        char **flags = NULL;
        count = toml_get_string_array(sections, "build.compiler", "flags", &flags);
        if (count > 0) {
            set_string_array(&cfg->compiler_flags, &cfg->compiler_flags_count, (const char*const*)flags, count);
            for (int i = 0; i < count; i++) memory_free_safe((void**)&flags[i]);
            memory_free_safe((void**)&flags);
        }
        flags = NULL;
        count = toml_get_string_array(sections, "build.compiler", "linker-flags", &flags);
        if (count > 0) {
            set_string_array(&cfg->linker_flags, &cfg->linker_flags_count, (const char*const*)flags, count);
            for (int i = 0; i < count; i++) memory_free_safe((void**)&flags[i]);
            memory_free_safe((void**)&flags);
        }

        /* [build.assembly] */
        val = get_string(sections, "build.assembly", "arch", NULL);
        if (val) config_set_string(&cfg->arch, val);

        /* [debug] */
        cfg->builtin = get_bool(sections, "debug", "builtin", cfg->builtin);
        /* breakpoints and watchpoints arrays */
        char **bps = NULL;
        count = toml_get_string_array(sections, "debug", "breakpoints", &bps);
        if (count > 0) {
            set_string_array(&cfg->breakpoints, &cfg->breakpoints_count, (const char*const*)bps, count);
            for (int i = 0; i < count; i++) memory_free_safe((void**)&bps[i]);
            memory_free_safe((void**)&bps);
        }
        char **wps = NULL;
        count = toml_get_string_array(sections, "debug", "watchpoints", &wps);
        if (count > 0) {
            set_string_array(&cfg->watchpoints, &cfg->watchpoints_count, (const char*const*)wps, count);
            for (int i = 0; i < count; i++) memory_free_safe((void**)&wps[i]);
            memory_free_safe((void**)&wps);
        }

        /* [test] */
        val = get_string(sections, "test", "filter", NULL);
        if (val) config_set_string(&cfg->test_filter, val);
        cfg->test_verbose = get_bool(sections, "test", "verbose", cfg->test_verbose);

        /* [scripts] */
        val = get_string(sections, "scripts", "pre-build", NULL);
        if (val) config_set_string(&cfg->pre_build, val);
        val = get_string(sections, "scripts", "post-build", NULL);
        if (val) config_set_string(&cfg->post_build, val);
        val = get_string(sections, "scripts", "pre-clean", NULL);
        if (val) config_set_string(&cfg->pre_clean, val);

        toml_free_sections(sections);
    }

    /* Restore original cwd */
    chdir(cwd);
    return 0;
}

int config_apply_command_line(PaxsyConfig *cfg, int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
            config_set_string(&cfg->target, argv[++i]);
        } else if (strcmp(argv[i], "--optimize") == 0 && i + 1 < argc) {
            int opt = atoi(argv[++i]);
            cfg->optimization = opt;
        } else if (strcmp(argv[i], "--debug") == 0) {
            cfg->debug = true;
        } else if (strcmp(argv[i], "--emit-asm") == 0) {
            cfg->emit_asm = true;
        } else if (strcmp(argv[i], "--recursive") == 0) {
            cfg->recursive = true;
        } else if (strcmp(argv[i], "--profile") == 0 && i + 1 < argc) {
            const char *profile = argv[++i];
            if (config_apply_profile(cfg, profile) != 0) {
                errhandler__report_error(0, 0, "config", "Profile '%s' not found", profile);
                return -1;
            }
        } else if (strcmp(argv[i], "--Wextra") == 0) {
            /* handled elsewhere via flags */
        } else if (strcmp(argv[i], "--Werror") == 0) {
            /* handled elsewhere */
        } else if (strcmp(argv[i], "--ignore-warnings") == 0) {
            /* handled elsewhere */
        } else if (strcmp(argv[i], "--verbose") == 0) {
            /* handled elsewhere */
        } else {
            errhandler__report_error(0, 0, "config", "Unknown command-line option: %s", argv[i]);
            return -1;
        }
    }
    return 0;
}

void config_apply_env(PaxsyConfig *cfg) {
    const char *env;
    env = getenv("PAXSY_TARGET");
    if (env) config_set_string(&cfg->target, env);
    env = getenv("PAXSY_OPT_LEVEL");
    if (env) cfg->optimization = atoi(env);
    env = getenv("PAXSY_DEBUG_DIR");
    if (env) config_set_string(&cfg->debug_dir, env);
}

int config_apply_profile(PaxsyConfig *cfg, const char *profile_name) {
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) {
        errhandler__report_error(0, 0, "config", "getcwd failed");
        return -1;
    }
    /* Determine project name */
    char dir[PATH_MAX];
    if (!getcwd(dir, sizeof(dir))) {
        errhandler__report_error(0, 0, "config", "getcwd failed");
        return -1;
    }
    char *proj_name = strrchr(dir, '/');
    if (proj_name) proj_name++;
    else proj_name = dir;
    char config_path[PATH_MAX * 2];
    snprintf(config_path, sizeof(config_path), "%s.toml", proj_name);

    TomlSection *sections = toml_parse_file(config_path);
    if (!sections) {
        errhandler__report_error(0, 0, "config", "Cannot parse config file for profile");
        return -1;
    }

    char section_name[256];
    snprintf(section_name, sizeof(section_name), "profiles.%s", profile_name);
    const char *target = toml_get_value(sections, section_name, "target");
    if (target) config_set_string(&cfg->target, target);
    const char *opt = toml_get_value(sections, section_name, "optimization");
    if (opt) cfg->optimization = atoi(opt);
    const char *debug = toml_get_value(sections, section_name, "debug");
    if (debug) cfg->debug = (strcmp(debug, "true") == 0);
    const char *emit = toml_get_value(sections, section_name, "emit-asm");
    if (emit) cfg->emit_asm = (strcmp(emit, "true") == 0);
    const char *rec = toml_get_value(sections, section_name, "recursive");
    if (rec) cfg->recursive = (strcmp(rec, "true") == 0);
    const char *out = toml_get_value(sections, section_name, "out-dir");
    if (out) config_set_string(&cfg->out_dir, out);
    const char *bdir = toml_get_value(sections, section_name, "build-dir");
    if (bdir) config_set_string(&cfg->build_dir, bdir);
    const char *ddir = toml_get_value(sections, section_name, "debug-dir");
    if (ddir) config_set_string(&cfg->debug_dir, ddir);

    toml_free_sections(sections);
    return 0;
}

void config_free(PaxsyConfig *cfg) {
    config_set_string(&cfg->name, NULL);
    config_set_string(&cfg->version, NULL);
    config_set_string(&cfg->description, NULL);
    set_string_array(&cfg->authors, &cfg->authors_count, NULL, 0);

    config_set_string(&cfg->target, NULL);
    config_set_string(&cfg->out_dir, NULL);
    config_set_string(&cfg->build_dir, NULL);
    config_set_string(&cfg->debug_dir, NULL);

    set_string_array(&cfg->compiler_flags, &cfg->compiler_flags_count, NULL, 0);
    set_string_array(&cfg->linker_flags, &cfg->linker_flags_count, NULL, 0);

    config_set_string(&cfg->arch, NULL);

    set_string_array(&cfg->breakpoints, &cfg->breakpoints_count, NULL, 0);
    set_string_array(&cfg->watchpoints, &cfg->watchpoints_count, NULL, 0);

    config_set_string(&cfg->test_filter, NULL);

    set_string_array(&cfg->dep_names, &cfg->deps_count, NULL, 0);
    set_string_array(&cfg->dep_versions, &cfg->deps_count, NULL, 0);
    set_string_array(&cfg->module_aliases, &cfg->modules_count, NULL, 0);
    set_string_array(&cfg->module_paths, &cfg->modules_count, NULL, 0);

    config_set_string(&cfg->pre_build, NULL);
    config_set_string(&cfg->post_build, NULL);
    config_set_string(&cfg->pre_clean, NULL);
}

/*
 * Detect the host architecture using uname.
 * If detection fails, default to "aarch64".
 */
void config_detect_arch(PaxsyConfig *cfg) {
    if (cfg->arch && *cfg->arch) {
        return; // already set
    }
    struct utsname uname_data;
    if (uname(&uname_data) != 0) {
        config_set_string(&cfg->arch, "aarch64");
        return;
    }
    const char *machine = uname_data.machine;
    if (strcmp(machine, "x86_64") == 0 || strcmp(machine, "amd64") == 0) {
        config_set_string(&cfg->arch, "x86_64");
    } else if (strcmp(machine, "aarch64") == 0 || strcmp(machine, "arm64") == 0) {
        config_set_string(&cfg->arch, "aarch64");
    } else {
        config_set_string(&cfg->arch, "aarch64"); // fallback
    }
}

/*
 * Return the suffix for executable files based on the platform.
 * For Windows (when _WIN32 is defined) returns ".exe", otherwise "".
 */
const char *config_get_exe_suffix(void) {
#ifdef _WIN32
    return ".exe";
#else
    return "";
#endif
}

/*
 * Build the full executable path: out_dir/name + suffix.
 * The result is written into a buffer of size at least PATH_MAX*2.
 */
void config_build_exe_path(const PaxsyConfig *cfg, const char *proj_name, char *out_path, size_t out_size) {
    const char *suffix = config_get_exe_suffix();
    snprintf(out_path, out_size, "%s/%s%s", cfg->out_dir, proj_name, suffix);
}
