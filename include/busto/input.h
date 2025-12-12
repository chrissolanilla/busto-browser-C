#ifndef BUSTO_INPUT_H
#define BUSTO_INPUT_H

#define MAX_URL_LENGTH 1024

struct busto_input {
    char url[MAX_URL_LENGTH];
    int active;
    int cursor_pos;
    int url_changed;
};

struct busto_input *busto_input_create(void);
void busto_input_destroy(struct busto_input *input);
void busto_input_set_url(struct busto_input *input, const char *url);
const char *busto_input_get_url(struct busto_input *input);
void busto_input_handle_key(struct busto_input *input, const char *key);
void busto_input_activate(struct busto_input *input);
void busto_input_deactivate(struct busto_input *input);
int busto_input_is_active(struct busto_input *input);
void busto_input_backspace(struct busto_input *input);
void busto_input_add_char(struct busto_input *input, char c);

#endif