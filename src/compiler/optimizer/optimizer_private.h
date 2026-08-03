#ifndef OPTIMIZER_PRIVATE_H
#define OPTIMIZER_PRIVATE_H

#include "optimizer.h"
#include "passes.h"

/* Global optimisation level and debug flag. */
extern int opt_level;
extern bool opt_debug;

/* Type of a single optimisation pass. */
typedef bool (*OptimizationPass)(Program* ast);

/* Return the list of passes to apply, and store the count in *count. */
const OptimizationPass* get_passes(int* count);

#endif
