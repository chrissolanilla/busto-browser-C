#ifndef BUSTO_KEY_REPEAT_H
#define BUSTO_KEY_REPEAT_H

#include "window.h"

struct busto_repeat_state {
    int rate;
    int delay;
    unsigned char key_down[256];
    long long key_next_repeat_ms[256];
};

typedef const char *(*busto_keycode_map_fn)(struct busto_window *window, int keycode);

long long busto_now_ms(void);
void busto_repeat_update(
    struct busto_repeat_state *state,
    busto_key_handler_t handler,
    void *handler_data,
    struct busto_window *window,
    busto_keycode_map_fn map_key
);

#endif