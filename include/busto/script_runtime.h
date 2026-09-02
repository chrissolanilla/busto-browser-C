#ifndef BUSTO_SCRIPT_RUNTIME_H
#define BUSTO_SCRIPT_RUNTIME_H

#include "busto_script.h"

int busto_script_compile_and_run(
    const char *source,
    struct busto_api *api
);

void busto_script_unload(void);

#endif
