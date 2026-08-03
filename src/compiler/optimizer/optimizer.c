#include "optimizer.h"
#include "optimizer_private.h"
#include "passes.h"
#include "../../interfaces/errhandler/errhandler.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/*
 * Main optimisation entry point.
 * Runs all enabled optimisation passes in a loop until no changes occur
 * or the maximum number of iterations (10) is reached.
 * Returns 0 on success, non-zero on error.
 */
int optimizer(ASTNode* ast, struct Scope* global_scope) {
    (void)global_scope;
    if (!ast) {
        errhandler__report_error(0, 0, "optimizer", "received NULL AST");
        return 1;
    }

    if (opt_level <= 0) {
        return 0;
    }

    Program* program = (Program*)ast;
    int pass_count;
    const OptimizationPass* passes = get_passes(&pass_count);
    if (!passes || pass_count == 0) {
        return 0;
    }

    const int MAX_ITERATIONS = 10;
    bool any_change = true;
    int iteration = 0;

    while (any_change && iteration < MAX_ITERATIONS) {
        any_change = false;
        iteration++;

        for (int i = 0; i < pass_count; ++i) {
            clock_t start = 0;
            if (opt_debug) {
                start = clock();
            }

            bool changed = passes[i](program);
            if (changed) {
                any_change = true;
                if (opt_debug) {
                    clock_t end = clock();
                    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
                    fprintf(stderr, "Pass %d changed AST, time: %f s\n", i, elapsed);
                }
            }
        }
    }

    if (opt_debug && iteration > 1) {
        fprintf(stderr, "Optimization converged after %d iterations\n", iteration);
    }

    return 0;
}

/*
 * Set the optimisation level.
 */
void optimizer_set_level(int level) {
    opt_level = level;
}

/*
 * Enable or disable debug output.
 */
void optimizer_set_debug(bool debug) {
    opt_debug = debug;
}
