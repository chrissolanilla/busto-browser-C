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

#endif
