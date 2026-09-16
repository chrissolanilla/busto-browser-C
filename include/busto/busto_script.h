#ifndef BUSTO_SCRIPT_H
#define BUSTO_SCRIPT_H

struct busto_graphics_api {
    void (*clear)(void);
    void (*set_fill)(double r, double g, double b, double a);
    void (*fill_rect)(double x, double y, double w, double h);
};


struct busto_api {
    void (*set_content)(const char *text);
    void (*set_title)(const char *title);
    void (*navigate)(const char *url);
    void (*request_redraw)(void);
    int (*get_width)(void);
    int (*get_height)(void);
    struct busto_graphics_api graphics;
};

typedef void (*busto_script_main_fn)(struct busto_api *api);
typedef void (*busto_script_start_fn)(struct busto_api *api);
typedef void (*busto_script_update_fn)(struct busto_api *api, double deltaT);
typedef void (*busto_script_on_key_press_fn)(struct busto_api *api, const char *key);
typedef void (*busto_script_stop_fn)(struct busto_api *api);

#endif
