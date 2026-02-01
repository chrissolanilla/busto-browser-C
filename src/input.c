#define _GNU_SOURCE
#include "../include/busto/input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct busto_input *busto_input_create(void) {
    struct busto_input *input = calloc(1, sizeof(struct busto_input));
    if (!input) return NULL;

    strcpy(input->url, "about:blank");
    input->active = 0;
    input->cursor_pos = strlen(input->url);
    input->url_changed = 1;

    return input;
}

void busto_input_destroy(struct busto_input *input) {
    if (input) {
        free(input);
    }
}

void busto_input_set_url(struct busto_input *input, const char *url) {
    if (!input || !url) return;

    strncpy(input->url, url, MAX_URL_LENGTH - 1);
    input->url[MAX_URL_LENGTH - 1] = '\0';
    input->cursor_pos = strlen(input->url);
    input->url_changed = 1;
}

const char *busto_input_get_url(struct busto_input *input) {
    return input ? input->url : NULL;
}

void busto_input_activate(struct busto_input *input) {
    if (!input) {
        return;
    }
    input->active = 1;
    //preserve last pos
    size_t len = strlen(input->url);
    if (input->cursor_pos > len){
        input->cursor_pos = len;
    }
}

void busto_input_deactivate(struct busto_input *input) {
    if (input) {
        input->active = 0;
    }
}

int busto_input_is_active(struct busto_input *input) {
    return input ? input->active : 0;
}

void busto_input_backspace(struct busto_input *input) {
    if (!input || !input->active || input->cursor_pos == 0) return;

    memmove(&input->url[input->cursor_pos - 1],
            &input->url[input->cursor_pos],
            strlen(input->url) - input->cursor_pos + 1);
    input->cursor_pos--;
    input->url_changed = 1;
}

void busto_input_add_char(struct busto_input *input, char c) {
    if (!input || !input->active || input->cursor_pos >= MAX_URL_LENGTH - 1) return;

    memmove(&input->url[input->cursor_pos + 1],
            &input->url[input->cursor_pos],
            strlen(input->url) - input->cursor_pos + 1);
    input->url[input->cursor_pos] = c;
    input->cursor_pos++;
    input->url_changed = 1;
}

void busto_input_handle_key(struct busto_input *input, const char *key) {
    if (!input || !input->active || !key) return;

    if (strcmp(key, "BackSpace") == 0) {
        busto_input_backspace(input);
    } else if (strcmp(key, "Return") == 0) {
        busto_input_deactivate(input);
    } else if (strcmp(key, "Left") == 0) {
        if (input->cursor_pos > 0) input->cursor_pos--;
    } else if (strcmp(key, "Right") == 0) {
        if (input->cursor_pos < strlen(input->url)) input->cursor_pos++;
    } else if (strcmp(key, "Home") == 0) {
        input->cursor_pos = 0;
    } else if (strcmp(key, "End") == 0) {
        input->cursor_pos = strlen(input->url);
    } else if (strlen(key) == 1) {
        busto_input_add_char(input, key[0]);
    }
}
