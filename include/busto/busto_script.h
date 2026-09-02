#ifndef BUSTO_SCRIPT_H
#define BUSTO_SCRIPT_H

struct busto_api {
    void (*set_content)(const char *text);
    void (*set_title)(const char *title);
    void (*navigate)(const char *url);
    void (*request_redraw)(void);
};

typedef void (*busto_script_main_fn)(struct busto_api *api);

#endif
