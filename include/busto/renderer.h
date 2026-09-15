#ifndef BUSTO_RENDERER_H
#define BUSTO_RENDERER_H

#include <cairo/cairo.h>
#include <stddef.h>
#include "busto_style.h"

void busto_renderer_set_stylesheet(const struct busto_stylesheet *stylesheet);

void busto_renderer_render(cairo_t *cr, int width, int height);
void busto_renderer_set_url(const char *url);
void busto_renderer_set_content(const char *content);
void busto_renderer_set_input_active(int active);
void busto_renderer_scroll(int delta);
void busto_renderer_free(void);
void busto_renderer_set_cursor_pos(size_t pos);

enum busto_content_mode {
    BUSTO_CONTENT_RICH,
    BUSTO_CONTENT_PLAIN
};
void busto_renderer_set_content_mode(enum busto_content_mode mode);

enum busto_draw_command_type {
    BUSTO_DRAW_FILL_RECT
};

struct busto_draw_command {
    enum busto_draw_command_type type;
    double x;
    double y;
    double w;
    double h;
    double r;
    double g;
    double b;
    double a;
};

void busto_renderer_graphics_clear(void);
void busto_renderer_graphics_set_fill(double r, double g, double b, double a);
void busto_renderer_graphics_fill_rect(double x, double y, double w, double h);


#endif
