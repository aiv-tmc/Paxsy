#include "commands.h"
#include "../interfaces/errhandler/errhandler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#ifndef STATE
#define STATE "Blackberry"
#endif
#ifndef VERSION
#define VERSION "0.7.1b"
#endif
#ifndef DATE
#define DATE __DATE__
#endif

/*
 * Print the general usage message.
 */
void print_usage(void) {
    printf("\033[92m████████████████\033[0m\n");
    printf("\033[92m      ██████████\033[90m░░\033[0m\tpaxsy \033[91m%s \033[90m%s - %s\033[0m\n", STATE, VERSION, DATE);
    printf("\033[92m  ████  ████████\033[90m░░\033[0m\n");
    printf("\033[92m  ████  ████████\033[90m░░\033[0m\tRead the documentation here:\n");
    printf("\033[92m      ██████████\033[90m░░\033[0m\t\t\033[90mhttps://github.com/aiv-tmc/paxsy/wiki\033[0m\n");
    printf("\033[92m  ██████████████\033[90m░░\033[0m\n");
    printf("\033[92m  ██████████████\033[90m░░\033[0m\tOur official website:\n");
    printf("\033[92m████████████████\033[90m░░\033[0m\t\t\033[90mhttps://paxsy.org\033[0m\n");
    printf("\033[90m  ░░░░░░░░░░░░░░░░\033[0m\n");
    printf("\n");
    printf("Usage: paxsy <command> [options]\n"
           "Commands:\n"
           "    init <project>          Create a new project directory with src/\n"
           "    build [path] [options]  Build the project (default: current directory)\n"
           "    check [path] [options]  Check the project for errors without building\n"
           "    test [path]             Run tests (if any)\n"
           "    run [path]              Run the compiled executable\n"
           "    state [path]            Build a static library archive (.a)\n"
           "    shared [path]           Build a shared object (.so)\n"
           "\n"
           "Build/check options:\n"
           "    --Wextra                Enable extended warnings\n"
           "    --Werror                Treat warnings as errors\n"
           "    --ignore-warnings       Suppress all warnings\n"
           "\n"
           "Other:\n"
           "    -l, --license           Show license information\n"
           "    -h, --help              Display this help\n");
}

/*
 * Display the license using nano (or fallback to cat).
 */
void handle_license(void) {
    const char *license_path = "/usr/share/doc/paxsy/LICENSE";
    FILE *f = fopen(license_path, "r");
    if (!f) {
        errhandler__report_error_ex(ERROR_LEVEL_WARNING, 0, 0, 1,
                                    "license", "License not found at %s", license_path);
        errhandler__print_errors();
        errhandler__print_warnings();
        return;
    }
    fclose(f);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "nano -v \"%s\" 2>/dev/null", license_path);
    if (system(cmd) != 0) {
        FILE *f2 = fopen(license_path, "r");
        if (f2) {
            char buf[1024];
            while (fgets(buf, sizeof(buf), f2)) {
                printf("%s", buf);
            }
            fclose(f2);
        } else {
            errhandler__report_error_ex(ERROR_LEVEL_WARNING, 0, 0, 1,
                                        "license", "Failed to read license file: %s", license_path);
            errhandler__print_errors();
            errhandler__print_warnings();
        }
    }
}
