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

int cmd_run(const char *project_path) {
    char original_cwd[PATH_MAX];
    if (!getcwd(original_cwd, sizeof(original_cwd))) {
        errhandler__report_error(0, 0, "run", "getcwd failed");
        return 1;
    }

    if (project_path) {
        if (chdir(project_path) != 0) {
            errhandler__report_error(0, 0, "run", "Cannot change to directory: %s", project_path);
            chdir(original_cwd);
            return 1;
        }
    }

    PaxsyConfig cfg;
    config_init_defaults(&cfg);
    if (config_load_from_project(project_path, &cfg) != 0) {
        errhandler__report_error(0, 0, "run", "Failed to load configuration");
        config_free(&cfg);
        chdir(original_cwd);
        return 1;
    }
    config_apply_env(&cfg);
    config_detect_arch(&cfg);   // not strictly needed but keep for completeness

    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) {
        errhandler__report_error(0, 0, "run", "getcwd failed");
        config_free(&cfg);
        chdir(original_cwd);
        return 1;
    }
    char *proj_name = strrchr(cwd, '/');
    if (proj_name) proj_name++;
    else proj_name = cwd;

    char exe_path[PATH_MAX * 2];
    config_build_exe_path(&cfg, proj_name, exe_path, sizeof(exe_path));

    if (access(exe_path, X_OK) != 0) {
        errhandler__report_error(0, 0, "run", "Executable not found or not executable: %s", exe_path);
        config_free(&cfg);
        chdir(original_cwd);
        return 1;
    }

    char *args[] = { exe_path, NULL };
    execv(exe_path, args);
    errhandler__report_error(0, 0, "run", "execv failed: %s", strerror(errno));
    config_free(&cfg);
    chdir(original_cwd);
    return 1;
}
