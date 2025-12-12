#ifndef BUSTO_RENDERER_H
#define BUSTO_RENDERER_H

#include <cairo/cairo.h>

void busto_renderer_render(cairo_t *cr, int width, int height);
void busto_renderer_set_url(const char *url);
void busto_renderer_set_content(const char *content);
void busto_renderer_set_input_active(int active);
void busto_renderer_scroll(int delta);
void busto_renderer_free(void);

#endif