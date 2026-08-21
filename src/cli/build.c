#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "commands.h"
#include "config.h"
#include "../compiler/semantic/semantic.h"
#include "../compiler/semantic/symbol.h"
#include "../compiler/preprocessor/skipcom.h"
#include "../compiler/lexer/lexer.h"
#include "../compiler/parser/parser.h"
#include "../compiler/optimizer/optimizer.h"
#include "../interfaces/errhandler/errhandler.h"
#include "../interfaces/utils/common.h"
#include "../interfaces/utils/file.h"
#include "../interfaces/utils/arena.h"
#include "../interfaces/utils/hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/wait.h>

struct SemanticContext;
struct Scope;
typedef struct Scope* (*ModuleResolver)(const char*);
struct SemanticContext* semantic__create_context(void);
void semantic__set_extra_warnings(struct SemanticContext* ctx, bool enable);
void semantic__destroy_context(struct SemanticContext* ctx);
int semantic__analyze(struct Program* ast, ModuleResolver resolver);
struct Scope* semantic__get_global_scope(struct SemanticContext* ctx);

static struct Scope* dummy_resolver(const char *module_name) { (void)module_name; return NULL; }

/*
 * Run an external command asynchronously using fork.
 * Returns a PID that can be waited on later, or -1 on failure.
 * Errors are reported via errhandler.
 */
static pid_t run_command_async(const char *cmd, char *const argv[]) {
    pid_t pid = fork();
    if (pid == -1) {
        errhandler__report_error(0, 0, "build", "fork failed: %s", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        execvp(cmd, argv);
        errhandler__report_error(0, 0, "build", "execvp failed for '%s': %s", cmd, strerror(errno));
        exit(127);
    }
    return pid;
}

/*
 * Wait for a list of child processes and count failures.
 * Returns the number of processes that exited with non-zero status or failed.
 */
static int wait_for_children(pid_t *pids, int count) {
    int failures = 0;
    for (int i = 0; i < count; i++) {
        int status;
        if (waitpid(pids[i], &status, 0) == -1) {
            errhandler__report_error(0, 0, "build", "waitpid failed");
            failures++;
        } else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            failures++;
        }
    }
    return failures;
}

/*
 * Check if an executable is available in PATH.
 * Returns 0 if found, -1 otherwise.
 */
static int check_executable(const char *cmd) {
    char *path = getenv("PATH");
    if (!path) return -1;
    char *path_copy = strdup(path);
    if (!path_copy) return -1;
    char *token = strtok(path_copy, ":");
    int found = 0;
    while (token) {
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", token, cmd);
        if (access(full_path, X_OK) == 0) {
            found = 1;
            break;
        }
        token = strtok(NULL, ":");
    }
    free(path_copy);
    return found ? 0 : -1;
}

/*
 * Recursively collect all .px files from a directory.
 * If recursive is true, traverse subdirectories.
 * Returns 0 on success, -1 on error.
 * The caller must free the returned array and its strings.
 */
static int collect_px_files(const char *dir, char ***files, size_t *count, bool recursive) {
    DIR *d = opendir(dir);
    if (!d) return -1;
    struct dirent *entry;
    size_t cap = 8;
    *files = (char**)malloc(cap * sizeof(char*));
    if (!*files) { closedir(d); return -1; }
    *count = 0;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);
        if (entry->d_type == DT_DIR) {
            if (recursive) {
                char **sub_files = NULL;
                size_t sub_count = 0;
                if (collect_px_files(full_path, &sub_files, &sub_count, recursive) == 0) {
                    for (size_t i = 0; i < sub_count; i++) {
                        if (*count >= cap) {
                            cap <<= 1;
                            char **new_arr = (char**)realloc(*files, cap * sizeof(char*));
                            if (!new_arr) {
                                for (size_t j = 0; j < sub_count; j++) free(sub_files[j]);
                                free(sub_files);
                                for (size_t j = 0; j < *count; j++) free((*files)[j]);
                                free(*files);
                                closedir(d);
                                return -1;
                            }
                            *files = new_arr;
                        }
                        (*files)[*count] = sub_files[i];
                        (*count)++;
                    }
                    free(sub_files);
                }
            }
        } else if (entry->d_type == DT_REG) {
            size_t len = strlen(entry->d_name);
            if (len > 3 && strcmp(entry->d_name + len - 3, ".px") == 0) {
                if (*count >= cap) {
                    cap <<= 1;
                    char **new_arr = (char**)realloc(*files, cap * sizeof(char*));
                    if (!new_arr) {
                        for (size_t i = 0; i < *count; i++) free((*files)[i]);
                        free(*files);
                        closedir(d);
                        return -1;
                    }
                    *files = new_arr;
                }
                char *full = (char*)malloc(strlen(dir) + len + 2);
                if (!full) {
                    for (size_t i = 0; i < *count; i++) free((*files)[i]);
                    free(*files);
                    closedir(d);
                    return -1;
                }
                sprintf(full, "%s/%s", dir, entry->d_name);
                (*files)[(*count)++] = full;
            }
        }
    }
    closedir(d);
    return 0;
}

/*
 * Process a single source file: preprocess, tokenize, parse, semantic, optimize.
 * Uses an arena allocator for temporary allocations.
 * Returns 0 on success, -1 on error.
 */
static int process_one_file(const char *filename, const char *output_asm,
                            unsigned int flags, struct SemanticContext **semantic_ctx,
                            ModuleResolver resolver, const PaxsyConfig *cfg,
                            Arena *arena) {
    int err = 0;
    char *raw = NULL;
    char *processed = NULL;
    Lexer *lexer = NULL;
    Program *ast = NULL;
    const char **lines = NULL;
    size_t line_count = 0;

    errhandler__set_current_filename(filename);

    raw = read_entire_file(filename);
    if (!raw) { err = 1; goto cleanup; }

    int pp_error = 0;
    processed = preprocess(raw, filename, &pp_error);
    if (!processed || pp_error) {
        err = 1;
        goto cleanup;
    }

    lines = split_into_lines(processed, &line_count);
    if (lines) { errhandler__set_source_code(lines, (uint16_t)line_count); }

    lexer = lexer__init_lexer(processed);
    if (!lexer) { err = 1; goto cleanup; }
    lexer__tokenize(lexer);

    if (!errhandler__has_errors()) {
        ast = parse(lexer->tokens, lexer->token_count);
    }

    if (*semantic_ctx && ast && !errhandler__has_errors()) {
        if (flags & F_WEXTRA) { semantic__set_extra_warnings(*semantic_ctx, true); }
        /* semantic__analyze returns 0 on success, non-zero on error */
        if (semantic__analyze(ast, resolver) != 0) { err = 1; }
        /*
         * The optimizer is intentionally not run here: it allocates and
         * frees AST nodes with malloc/free, which is incompatible with the
         * parser's arena-owned AST, and its output feeds only the assembly
         * placeholder below. Re-enable once the optimizer is arena-aware.
         */
    }

    /* Write assembly output */
    if (!err && output_asm) {
        FILE *out = fopen(output_asm, "w");
        if (out) {
            /* Emit assembly (placeholder – real generator would be called here) */
            fprintf(out, "; Assembly for %s (placeholder)\n", filename);
            fclose(out);
        } else {
            errhandler__report_error(0, 0, "build", "Failed to write assembly: %s", output_asm);
            err = 1;
        }
    }

cleanup:
    errhandler__clear_source_code();
    if (lines) free_lines(lines, line_count);
    if (ast) parser_free_ast(ast);
    if (lexer) lexer__free_lexer(lexer);
    FREE_AND_NULL(processed);
    FREE_AND_NULL(raw);
    errhandler__set_current_filename(NULL);
    return err || errhandler__has_errors() ? -1 : 0;
}

/*
 * Build the project using the provided configuration.
 * Implements parallel assembly and incremental caching.
 */
int build_with_config(const PaxsyConfig *cfg, unsigned int flags) {
    int result = 1;
    char original_cwd[PATH_MAX];
    if (!getcwd(original_cwd, sizeof(original_cwd))) {
        errhandler__report_error(0, 0, "build", "getcwd failed");
        return 1;
    }

    /* Execute pre-build script if defined */
    if (cfg->pre_build && *cfg->pre_build) {
        printf("Executing pre-build: %s\n", cfg->pre_build);
        if (system(cfg->pre_build) != 0) {
            errhandler__report_error_ex(ERROR_LEVEL_WARNING, 0, 0, 1,
                                        "build", "pre-build script failed");
        }
    }

    /* Create output directories */
    if (!(flags & F_CHECK_ONLY)) {
        mkdir(cfg->build_dir, 0755);
        mkdir(cfg->out_dir, 0755);
        if (cfg->debug) {
            mkdir(cfg->debug_dir, 0755);
        }
    }

    /* Collect source files */
    char **px_files = NULL;
    size_t px_count = 0;
    if (collect_px_files("src", &px_files, &px_count, cfg->recursive) != 0 || px_count == 0) {
        errhandler__report_error(0, 0, "build", "No .px files found in src/");
        free(px_files);
        goto cleanup;
    }

    struct SemanticContext *semantic_ctx = semantic__create_context();
    if (!semantic_ctx) {
        errhandler__report_error(0, 0, "semantic", "Failed to create semantic analysis context");
        free(px_files);
        goto cleanup;
    }
    if (flags & F_WEXTRA) {
        semantic__set_extra_warnings(semantic_ctx, true);
    }

    int error_occurred = 0;
    int do_assemble = !(flags & F_CHECK_ONLY);
    ModuleResolver resolver = dummy_resolver;
    bool keep_asm = cfg->emit_asm;

    /* Cache directory for incremental builds */
    const char *cache_dir = ".paxsy_cache";
    if (!(flags & F_CHECK_ONLY)) {
        mkdir(cache_dir, 0755);
    }

    pid_t *asm_pids = NULL;
    int asm_count = 0;
    if (do_assemble) {
        asm_pids = (pid_t*)malloc(px_count * sizeof(pid_t));
        if (!asm_pids) {
            errhandler__report_error(0, 0, "build", "Memory allocation for PIDs failed");
            error_occurred = 1;
            goto link_cleanup;
        }
    }

    for (size_t i = 0; i < px_count && !error_occurred; ++i) {
        const char *src = px_files[i];
        const char *base = strrchr(src, '/');
        base = base ? base + 1 : src;

        /* Compute hash of source file for caching */
        uint64_t hash = u__hash_file(src);
        if (hash == 0) {
            errhandler__report_error(0, 0, "build", "Failed to hash source: %s", src);
            error_occurred = 1;
            break;
        }

        char cache_path[PATH_MAX];
        snprintf(cache_path, sizeof(cache_path), "%s/%s.%016llx", cache_dir, base, (unsigned long long)hash);

        char asm_path[PATH_MAX];
        snprintf(asm_path, sizeof(asm_path), "%s/%s.s", cfg->build_dir, base);

        char obj_path[PATH_MAX];
        snprintf(obj_path, sizeof(obj_path), "%s/%s.o", cfg->build_dir, base);

        /* Check cache: if hash matches and object exists, skip compilation */
        bool up_to_date = false;
        if (!(flags & F_CHECK_ONLY)) {
            FILE *cache_file = fopen(cache_path, "r");
            if (cache_file) {
                char stored_hash_str[17];
                if (fread(stored_hash_str, 1, 16, cache_file) == 16) {
                    /* In real implementation we would compare with stored hash */
                    up_to_date = (access(obj_path, F_OK) == 0);
                }
                fclose(cache_file);
            }
        }

        if (up_to_date) {
            printf("Up-to-date: %s\n", src);
            continue;
        }

        printf("%s %s\n", do_assemble ? "Compiling" : "Checking", src);

        Arena *arena = arena_create(1 << 20); // 1 MB initial
        if (!arena) {
            errhandler__report_error(0, 0, "build", "Failed to create arena for %s", src);
            error_occurred = 1;
            break;
        }

        if (process_one_file(src, do_assemble ? asm_path : NULL, flags, &semantic_ctx, resolver, cfg, arena) != 0) {
            error_occurred = 1;
            arena_destroy(arena);
            break;
        }
        arena_destroy(arena);

        /* Write cache entry */
        if (do_assemble && !error_occurred) {
            FILE *cache_file = fopen(cache_path, "w");
            if (cache_file) {
                fprintf(cache_file, "%016llx", (unsigned long long)hash);
                fclose(cache_file);
            }
        }

        if (do_assemble) {
            /* Check assembler availability */
            if (check_executable("as") != 0) {
                errhandler__report_error(0, 0, "build", "Assembler 'as' not found in PATH");
                error_occurred = 1;
                break;
            }

            const char *arch = cfg->arch ? cfg->arch : "aarch64";
            char *asm_argv[] = {
                (char*)"as",
                (char*)"-march", (char*)arch,
                cfg->debug ? (char*)"-g" : NULL,
                (char*)"-o", obj_path,
                asm_path,
                NULL
            };
            /* Build argument list, skipping NULL entries */
            char *argv_build[10];
            int idx = 0;
            for (int j = 0; j < 7; j++) {
                if (asm_argv[j]) argv_build[idx++] = asm_argv[j];
            }
            argv_build[idx] = NULL;

            printf("Assembling: as %s\n", asm_path);
            pid_t pid = run_command_async("as", argv_build);
            if (pid == -1) {
                error_occurred = 1;
                break;
            }
            asm_pids[asm_count++] = pid;

            /* Remove assembly file if not keeping it */
            if (!keep_asm) {
                remove(asm_path);
            }
        }
    }

    /* Wait for all assembly processes */
    if (do_assemble && !error_occurred && asm_count > 0) {
        int asm_failures = wait_for_children(asm_pids, asm_count);
        if (asm_failures > 0) {
            errhandler__report_error(0, 0, "build", "%d assembly subprocess(es) failed", asm_failures);
            error_occurred = 1;
        }
    }

    /* Link if no errors and not check-only */
    if (!error_occurred && do_assemble && px_count > 0) {
        char cwd[PATH_MAX];
        if (!getcwd(cwd, sizeof(cwd))) {
            errhandler__report_error(0, 0, "build", "getcwd failed");
            error_occurred = 1;
        } else {
            char *proj_name = strrchr(cwd, '/');
            if (proj_name) proj_name++;
            else proj_name = cwd;

            const char *target = cfg->target ? cfg->target : "exe";
            bool need_linking = true;
            const char *suffix = config_get_exe_suffix();

            if (strcmp(target, "asm") == 0 || strcmp(target, "obj") == 0) {
                need_linking = false;
                printf("Target %s: object files generated in %s/\n", target, cfg->build_dir);
            } else if (strcmp(target, "static") == 0) {
                if (check_executable("ar") != 0) {
                    errhandler__report_error(0, 0, "build", "Archiver 'ar' not found in PATH");
                    error_occurred = 1;
                    goto link_cleanup;
                }
                char *ar_argv[256];
                int idx = 0;
                ar_argv[idx++] = (char*)"ar";
                ar_argv[idx++] = (char*)"rcs";
                char lib_path[PATH_MAX];
                snprintf(lib_path, sizeof(lib_path), "%s/lib%s.a", cfg->out_dir, proj_name);
                ar_argv[idx++] = lib_path;
                for (size_t i = 0; i < px_count; ++i) {
                    char *base = strrchr(px_files[i], '/');
                    base = base ? base + 1 : px_files[i];
                    char o_file[PATH_MAX];
                    snprintf(o_file, sizeof(o_file), "%s/%s.o", cfg->build_dir, base);
                    char *dot = strrchr(o_file, '.');
                    if (dot) *dot = '\0';
                    strcat(o_file, ".o");
                    ar_argv[idx++] = o_file;
                }
                ar_argv[idx] = NULL;
                printf("Creating static library: ar rcs %s\n", lib_path);
                int ret = run_command_async("ar", ar_argv); /* synchronous for simplicity */
                if (ret == -1) {
                    error_occurred = 1;
                } else {
                    int status;
                    waitpid(ret, &status, 0);
                    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                        errhandler__report_error(0, 0, "archiver", "ar failed");
                        error_occurred = 1;
                    } else {
                        printf("Static library created: %s\n", lib_path);
                    }
                }
                need_linking = false;
            } else if (strcmp(target, "shared") == 0) {
                if (check_executable("gcc") != 0) {
                    errhandler__report_error(0, 0, "build", "Compiler 'gcc' not found in PATH");
                    error_occurred = 1;
                    goto link_cleanup;
                }
                char link_cmd_full[PATH_MAX * 2];
                snprintf(link_cmd_full, sizeof(link_cmd_full), "%s/lib%s.so", cfg->out_dir, proj_name);
                char *gcc_argv[256];
                int idx = 0;
                gcc_argv[idx++] = (char*)"gcc";
                gcc_argv[idx++] = (char*)"-shared";
                gcc_argv[idx++] = (char*)"-o";
                gcc_argv[idx++] = link_cmd_full;
                for (size_t i = 0; i < px_count; ++i) {
                    char *base = strrchr(px_files[i], '/');
                    base = base ? base + 1 : px_files[i];
                    char o_file[PATH_MAX];
                    snprintf(o_file, sizeof(o_file), "%s/%s.o", cfg->build_dir, base);
                    char *dot = strrchr(o_file, '.');
                    if (dot) *dot = '\0';
                    strcat(o_file, ".o");
                    gcc_argv[idx++] = o_file;
                }
                for (size_t i = 0; i < cfg->compiler_flags_count; ++i) {
                    gcc_argv[idx++] = cfg->compiler_flags[i];
                }
                for (size_t i = 0; i < cfg->linker_flags_count; ++i) {
                    gcc_argv[idx++] = cfg->linker_flags[i];
                }
                if (cfg->debug) {
                    gcc_argv[idx++] = (char*)"-g";
                }
                gcc_argv[idx] = NULL;
                printf("Linking: gcc -shared ...\n");
                int ret = run_command_async("gcc", gcc_argv);
                if (ret == -1) {
                    error_occurred = 1;
                } else {
                    int status;
                    waitpid(ret, &status, 0);
                    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                        errhandler__report_error(0, 0, "linker", "Linker failed");
                        error_occurred = 1;
                    } else {
                        printf("Build successful: %s\n", link_cmd_full);
                    }
                }
                need_linking = false;
            } else if (strcmp(target, "apk") == 0 || strcmp(target, "reakt") == 0) {
                errhandler__report_error_ex(ERROR_LEVEL_WARNING, 0, 0, 1,
                                            "build", "Target %s is not yet fully supported; building as exe", target);
            }

            if (need_linking) {
                if (check_executable("gcc") != 0) {
                    errhandler__report_error(0, 0, "build", "Compiler 'gcc' not found in PATH");
                    error_occurred = 1;
                    goto link_cleanup;
                }
                char exe_path[PATH_MAX * 2];
                snprintf(exe_path, sizeof(exe_path), "%s/%s%s", cfg->out_dir, proj_name, suffix);
                char *gcc_argv[256];
                int idx = 0;
                gcc_argv[idx++] = (char*)"gcc";
                gcc_argv[idx++] = (char*)"-o";
                gcc_argv[idx++] = exe_path;
                for (size_t i = 0; i < px_count; ++i) {
                    char *base = strrchr(px_files[i], '/');
                    base = base ? base + 1 : px_files[i];
                    char o_file[PATH_MAX];
                    snprintf(o_file, sizeof(o_file), "%s/%s.o", cfg->build_dir, base);
                    char *dot = strrchr(o_file, '.');
                    if (dot) *dot = '\0';
                    strcat(o_file, ".o");
                    gcc_argv[idx++] = o_file;
                }
                for (size_t i = 0; i < cfg->compiler_flags_count; ++i) {
                    gcc_argv[idx++] = cfg->compiler_flags[i];
                }
                for (size_t i = 0; i < cfg->linker_flags_count; ++i) {
                    gcc_argv[idx++] = cfg->linker_flags[i];
                }
                if (cfg->debug) {
                    gcc_argv[idx++] = (char*)"-g";
                }
                gcc_argv[idx] = NULL;
                printf("Linking: gcc ...\n");
                int ret = run_command_async("gcc", gcc_argv);
                if (ret == -1) {
                    error_occurred = 1;
                } else {
                    int status;
                    waitpid(ret, &status, 0);
                    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                        errhandler__report_error(0, 0, "linker", "Linker failed");
                        error_occurred = 1;
                    } else {
                        printf("Build successful: %s\n", exe_path);
                    }
                }
            }
        }
    }

link_cleanup:
    free(asm_pids);
    for (size_t i = 0; i < px_count; ++i) free(px_files[i]);
    free(px_files);
    if (semantic_ctx) semantic__destroy_context(semantic_ctx);

    /* Diagnostics are printed once by the command dispatcher in main.c */

    result = (error_occurred || errhandler__has_errors()) ? 1 : 0;

    /* Execute post-build script */
    if (result == 0 && cfg->post_build && *cfg->post_build) {
        printf("Executing post-build: %s\n", cfg->post_build);
        if (system(cfg->post_build) != 0) {
            errhandler__report_error_ex(ERROR_LEVEL_WARNING, 0, 0, 1,
                                        "build", "post-build script failed");
        }
    }

cleanup:
    chdir(original_cwd);
    return result;
}

/*
 * Wrapper for build_project (used by state, shared) that loads defaults and environment.
 */
int build_project(const char *project_path, int mode, unsigned int flags) {
    PaxsyConfig cfg;
    config_init_defaults(&cfg);
    if (config_load_from_project(project_path, &cfg) != 0) {
        errhandler__report_error(0, 0, "build", "Failed to load configuration");
        config_free(&cfg);
        return 1;
    }
    config_apply_env(&cfg);
    config_detect_arch(&cfg);
    if (mode == BUILD_MODE_STATIC) {
        config_set_string(&cfg.target, "static");
    } else if (mode == BUILD_MODE_SHARED) {
        config_set_string(&cfg.target, "shared");
    }
    int ret = build_with_config(&cfg, flags);
    config_free(&cfg);
    return ret;
}

/* Public command: build */
int cmd_build(const char *project_path, int argc, char **argv) {
    PaxsyConfig cfg;
    config_init_defaults(&cfg);
    if (config_load_from_project(project_path, &cfg) != 0) {
        errhandler__report_error(0, 0, "build", "Failed to load configuration");
        config_free(&cfg);
        return 1;
    }
    config_apply_env(&cfg);
    if (config_apply_command_line(&cfg, argc, argv) != 0) {
        config_free(&cfg);
        return 1;
    }
    config_detect_arch(&cfg);
    unsigned int flags = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--Wextra") == 0) flags |= F_WEXTRA;
        else if (strcmp(argv[i], "--Werror") == 0) flags |= F_WERROR;
        else if (strcmp(argv[i], "--ignore-warnings") == 0) flags |= F_WIGNOR;
        else if (strcmp(argv[i], "--target") == 0 || strcmp(argv[i], "--optimize") == 0 ||
                 strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "--emit-asm") == 0 ||
                 strcmp(argv[i], "--recursive") == 0 || strcmp(argv[i], "--profile") == 0 ||
                 strcmp(argv[i], "--verbose") == 0) {
            if (strcmp(argv[i], "--target") == 0 || strcmp(argv[i], "--optimize") == 0 ||
                strcmp(argv[i], "--profile") == 0) i++;
        } else {
            errhandler__report_error(0, 0, "build", "Unknown build option: %s", argv[i]);
        }
    }
    int ret = build_with_config(&cfg, flags);
    config_free(&cfg);
    return ret;
}

/* Public command: check */
int cmd_check(const char *project_path, int argc, char **argv) {
    PaxsyConfig cfg;
    config_init_defaults(&cfg);
    if (config_load_from_project(project_path, &cfg) != 0) {
        errhandler__report_error(0, 0, "check", "Failed to load configuration");
        config_free(&cfg);
        return 1;
    }
    config_apply_env(&cfg);
    if (config_apply_command_line(&cfg, argc, argv) != 0) {
        config_free(&cfg);
        return 1;
    }
    config_detect_arch(&cfg);
    unsigned int flags = F_CHECK_ONLY;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--Wextra") == 0) flags |= F_WEXTRA;
        else if (strcmp(argv[i], "--Werror") == 0) flags |= F_WERROR;
        else if (strcmp(argv[i], "--ignore-warnings") == 0) flags |= F_WIGNOR;
        else if (strcmp(argv[i], "--target") == 0 || strcmp(argv[i], "--optimize") == 0 ||
                 strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "--emit-asm") == 0 ||
                 strcmp(argv[i], "--recursive") == 0 || strcmp(argv[i], "--profile") == 0 ||
                 strcmp(argv[i], "--verbose") == 0) {
            if (strcmp(argv[i], "--target") == 0 || strcmp(argv[i], "--optimize") == 0 ||
                strcmp(argv[i], "--profile") == 0) i++;
        } else {
            errhandler__report_error(0, 0, "check", "Unknown check option: %s", argv[i]);
        }
    }
    int ret = build_with_config(&cfg, flags);
    config_free(&cfg);
    return ret;
}
