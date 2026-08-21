#define _POSIX_C_SOURCE 200809L

#include "commands.h"
#include "../interfaces/errhandler/errhandler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>

/*
 * Create a new project directory and initialise the source tree.
 * Also generate a default <project>.toml configuration file.
 * Returns 0 on success, non-zero on error.
 */
int cmd_init(const char *project_name) {
    int result = 0;

    /* Validate project name. */
    if (!project_name || !*project_name) {
        errhandler__report_error(0, 0, "init",
                                 "Project name required");
        result = 1;
        goto cleanup;
    }

    /* Create the project root directory. */
    if (mkdir(project_name, 0755) != 0 && errno != EEXIST) {
        errhandler__report_error(0, 0, "init",
                                 "mkdir failed for %s: %s", project_name, strerror(errno));
        result = 1;
        goto cleanup;
    }

    /* Create src/ subdirectory. */
    char src_path[PATH_MAX];
    snprintf(src_path, sizeof(src_path), "%s/src", project_name);
    if (mkdir(src_path, 0755) != 0 && errno != EEXIST) {
        errhandler__report_error(0, 0, "init",
                                 "mkdir failed for %s: %s", src_path, strerror(errno));
        result = 1;
        goto cleanup;
    }

    /* Write the main.px file. */
    char main_file[PATH_MAX];
    snprintf(main_file, sizeof(main_file), "%s/src/main.px", project_name);
    FILE *file = fopen(main_file, "w");
    if (!file) {
        errhandler__report_error(0, 0, "init",
                                 "Failed to create main.px: %s", strerror(errno));
        result = 1;
        goto cleanup;
    }
    fprintf(file,
            "use std::inout;\n"
            "\n"
            "func main(): Void {\n"
            "    std::inout::println(\"Hello, World!\");\n"
            "}\n");
    fclose(file);

    /* Write the default <project>.toml configuration file. */
    char config_file[PATH_MAX];
    snprintf(config_file, sizeof(config_file), "%s/%s.toml", project_name, project_name);
    FILE *cfg = fopen(config_file, "w");
    if (!cfg) {
        errhandler__report_error(0, 0, "init",
                                 "Failed to create %s.toml: %s", project_name, strerror(errno));
        result = 1;
        goto cleanup;
    }
    fprintf(cfg,
            "[project]\n"
            "name = %s\n"
            "version = \"0.1.0\"\n"
            "description = \"\"\n"
            "authors = []\n"
            "\n"
            "[build]\n"
            "target = \"elf\"\n"
            "optimization = 2\n"
            "debug = false\n"
            "emit-asm = false\n"
            "recursive = false\n"
            "out-dir = \"target\"\n"
            "build-dir = \"build\"\n"
            "debug-dir = \"debug\"\n"
            "\n"
            "[build.assembly]\n"
            "arch = \"aarch64\"\n"
            "\n"
            "[debug]\n"
            "builtin = true\n"
            "breakpoints = []\n"
            "watchpoints = []\n"
            "\n"
            "[modules]\n"
            "std = std",
            project_name);
    fclose(cfg);

    /* Initialize Git repository in the project directory. */
    char git_cmd[PATH_MAX + 20];
    snprintf(git_cmd, sizeof(git_cmd), "git init %s", project_name);
    if (system(git_cmd) != 0) {
        errhandler__report_error_ex(ERROR_LEVEL_WARNING, 0, 0, 1,
                                    "init", "Failed to initialize Git repository (git not found?)");
        /* Not a fatal error, continue. */
    } else {
        printf("Initialized Git repository in %s/\n", project_name);
    }

    printf("Created project '%s' with src/ and %s.toml\n", project_name, project_name);

cleanup:
    errhandler__print_errors();
    errhandler__print_warnings();
    return result;
}
