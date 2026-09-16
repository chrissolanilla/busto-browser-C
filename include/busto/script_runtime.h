#ifndef BUSTO_SCRIPT_RUNTIME_H
#define BUSTO_SCRIPT_RUNTIME_H

#include "busto_script.h"

int busto_script_compile_and_run(
    const char *source,
    struct busto_api *api
);

int busto_script_compile_and_start(const char *source);

void busto_script_unload(void);

void busto_script_pump_browser_messages(struct busto_api *api);
void busto_script_update(double deltaT, int width, int height);
void busto_script_key_press(const char *key);

#endif
