#ifndef BUSTO_WINDOW_H
#define BUSTO_WINDOW_H

struct busto_window;

typedef void (*busto_key_handler_t)(struct busto_window *window, const char *key, void *user_data);
typedef void (*busto_paste_handler_t)(struct busto_window *window, const char *text, void *user_data);

struct busto_window *busto_window_create(int width, int height);
void busto_window_destroy(struct busto_window *window);
void busto_window_set_title(struct busto_window *window, const char *title);
int busto_window_is_running(struct busto_window *window);
void busto_window_set_key_handler(struct busto_window *window, busto_key_handler_t handler, void *user_data);
void busto_window_set_paste_handler(struct busto_window *window, busto_paste_handler_t handler, void *user_data);
void busto_window_request_paste(struct busto_window *window);

void busto_window_poll(struct busto_window *window, int timeout_ms);
void busto_window_dispatch(struct busto_window *window);
void busto_window_update_repeats(struct busto_window *window);

void busto_window_request_redraw(struct busto_window *window);
int busto_window_needs_redraw(struct busto_window *window);
void busto_window_redraw(struct busto_window *window);

#endif
