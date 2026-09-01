#include "../include/busto/key_repeat.h"
#include <stdio.h>
#include <time.h>

long long busto_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

void busto_repeat_update(
    struct busto_repeat_state *state,
    busto_key_handler_t handler,
    void *handler_data,
    struct busto_window *window,
    busto_keycode_map_fn map_key
) {
	long long interval_ms;
    if (!state || !handler) {
		return;
	}

    if (state->rate <= 0){
		return;
	}

    interval_ms = 1000LL / state->rate;
    if (interval_ms <= 0) {
		interval_ms =1;
	}

    long long t = busto_now_ms();
    for (int key = 0; key < 256; key++) {
        if (!state->key_down[key]) {
			continue;
		}

        long long next = state->key_next_repeat_ms[key];
        if (next == 0) {
			continue;
		}

        if (t < next) {
			continue;
		}

		//need the terenary to declar it const, but maybe could be a function
        const char *key_str = map_key ? map_key(window, key) : NULL;
        if (!key_str) {
			continue;
		}

        printf("[repeat] key=%d '%s'\n", key, key_str);

        handler(window, key_str, handler_data);

        state->key_next_repeat_ms[key] = next + interval_ms;

        if (state->key_next_repeat_ms[key] < t - 200) {
            state->key_next_repeat_ms[key] = t + interval_ms;
        }
    }
}
