#define _GNU_SOURCE
#include "../include/busto/renderer.h"
#include <cairo/cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple renderer state
static struct {
    char *url;
    char *content;
    int url_input_active;
    int scroll_y;
} renderer_state = {0};

void busto_renderer_render(cairo_t *cr, int width, int height) {
    // Clear surface with white background
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_paint(cr);

    // Draw URL bar background
    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    cairo_rectangle(cr, 10, 10, width - 20, 40);
    cairo_fill(cr);

    // Draw URL bar border
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_rectangle(cr, 10, 10, width - 20, 40);
    cairo_stroke(cr);

    // Draw URL text
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 16.0);
    cairo_move_to(cr, 15, 35);

    const char *url_display = renderer_state.url ? renderer_state.url : "about:blank";
    cairo_show_text(cr, url_display);

    // Draw cursor if input is active
    if (renderer_state.url_input_active) {
        cairo_text_extents_t extents;
        cairo_text_extents(cr, url_display, &extents);
        cairo_move_to(cr, 15 + extents.width + 2, 35);
        cairo_line_to(cr, 15 + extents.width + 2, 25);
        cairo_stroke(cr);
    }

    // Draw content area background
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_rectangle(cr, 10, 60, width - 20, height - 70);
    cairo_fill(cr);

    // Draw content
    if (renderer_state.content) {
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 14.0);

        // Simple text rendering
        char *content_copy = strdup(renderer_state.content);
        char *line = strtok(content_copy, "\n");
        int y = 85 - renderer_state.scroll_y;

        while (line && y < height - 20) {
            if (strlen(line) > 0) {
                cairo_move_to(cr, 20, y);
                cairo_show_text(cr, line);
            }
            y += 20;
            line = strtok(NULL, "\n");
        }

        free(content_copy);
    } else {
        // Show default content
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 18.0);
        cairo_move_to(cr, 20, 85);
        cairo_show_text(cr, "Enter a URL to get started");
    }
}

void busto_renderer_set_url(const char *url) {
    if (renderer_state.url) {
        free(renderer_state.url);
    }
    renderer_state.url = url ? strdup(url) : NULL;
}

void busto_renderer_set_content(const char *content) {
    if (renderer_state.content) {
        free(renderer_state.content);
    }
    renderer_state.content = content ? strdup(content) : NULL;
}

void busto_renderer_set_input_active(int active) {
    renderer_state.url_input_active = active;
}

void busto_renderer_scroll(int delta) {
    renderer_state.scroll_y += delta;
    if (renderer_state.scroll_y < 0) {
        renderer_state.scroll_y = 0;
    }
}

void busto_renderer_free(void) {
    if (renderer_state.url) {
        free(renderer_state.url);
        renderer_state.url = NULL;
    }
    if (renderer_state.content) {
        free(renderer_state.content);
        renderer_state.content = NULL;
    }
}
