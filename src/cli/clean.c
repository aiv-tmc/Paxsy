#define _POSIX_C_SOURCE 200809L
#include "commands.h"
#include "config.h"
#include "../interfaces/errhandler/errhandler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>

/*
 * Remove a directory recursively (simple implementation).
 */
static void remove_directory(const char *path) {
    char cmd[PATH_MAX + 64];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
    system(cmd);
}

/*
 * Clean build artifacts: remove build-dir, out-dir, and debug-dir.
 * Also execute pre-clean script if defined.
 */
int cmd_clean(const char *project_path) {
    char original_cwd[PATH_MAX];
    if (!getcwd(original_cwd, sizeof(original_cwd))) {
        errhandler__report_error(0, 0, "clean", "getcwd failed");
        return 1;
    }

    if (project_path) {
        if (chdir(project_path) != 0) {
            errhandler__report_error(0, 0, "clean", "Cannot change to directory: %s", project_path);
            chdir(original_cwd);
            return 1;
        }
    }

    PaxsyConfig cfg;
    config_init_defaults(&cfg);
    if (config_load_from_project(project_path, &cfg) != 0) {
        errhandler__report_error(0, 0, "clean", "Failed to load configuration");
        config_free(&cfg);
        chdir(original_cwd);
        return 1;
    }

    /* Execute pre-clean script */
    if (cfg.pre_clean && *cfg.pre_clean) {
        printf("Executing pre-clean: %s\n", cfg.pre_clean);
        if (system(cfg.pre_clean) != 0) {
            errhandler__report_error_ex(ERROR_LEVEL_WARNING, 0, 0, 1,
                                        "clean", "pre-clean script failed");
        }
    }

    /* Remove directories */
    remove_directory(cfg.build_dir);
    remove_directory(cfg.out_dir);
    remove_directory(cfg.debug_dir);

    printf("Cleaned build artifacts (from %s, %s, %s)\n", cfg.build_dir, cfg.out_dir, cfg.debug_dir);

    config_free(&cfg);
    chdir(original_cwd);
    return 0;
}
