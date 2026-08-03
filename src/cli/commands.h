#ifndef COMMANDS_H
#define COMMANDS_H

#include "config.h"

/* Build modes */
#define BUILD_MODE_EXE       0
#define BUILD_MODE_STATIC    1
#define BUILD_MODE_SHARED    2

/* Build flags (used by build and check) */
#define F_WEXTRA      (1U << 0)
#define F_WERROR      (1U << 1)
#define F_WIGNOR      (1U << 2)
#define F_CHECK_ONLY  (1U << 3)   /* Skip assembly and linking */

/* Command handlers */
int cmd_init(const char *project_name);
int cmd_build(const char *project_path, int argc, char **argv);
int cmd_check(const char *project_path, int argc, char **argv);
int cmd_test(const char *project_path);
int cmd_run(const char *project_path);
int cmd_state(const char *project_path);
int cmd_shared(const char *project_path);
int cmd_clean(const char *project_path);

/* Core build function that takes a fully configured PaxsyConfig */
int build_with_config(const PaxsyConfig *cfg, unsigned int flags);

/* Wrapper for building with mode (static/shared) – ADD THIS LINE */
int build_project(const char *project_path, int mode, unsigned int flags);

/* Helper functions */
void print_usage(void);
void print_version(void);
void handle_license(void);

#endif
