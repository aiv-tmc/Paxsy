#include "commands.h"
#include "../interfaces/errhandler/errhandler.h"

/*
 * Build a shared object (.so) from the project.
 * project_path: path to project root (NULL for current directory).
 */
int cmd_shared(const char *project_path) {
    return build_project(project_path, BUILD_MODE_SHARED, 0);
}
