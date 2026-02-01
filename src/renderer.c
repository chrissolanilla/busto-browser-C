#define _GNU_SOURCE
#include "../include/busto/renderer.h"
#include <cairo/cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    char *url;
    char *content;
    int url_input_active;
    int scroll_y;
    size_t url_cursor_pos;
} renderer_state = {0};

static double font_for_marker(const char *line, const char **out_text_start) {
    *out_text_start = line;

    if (strncmp(line, "[[H1]]", 6) == 0) { *out_text_start = line + 6; return 28.0; }
    if (strncmp(line, "[[H2]]", 6) == 0) { *out_text_start = line + 6; return 22.0; }
    if (strncmp(line, "[[H3]]", 6) == 0) { *out_text_start = line + 6; return 18.0; }
    if (strncmp(line, "[[H4]]", 6) == 0) { *out_text_start = line + 6; return 16.0; }
    if (strncmp(line, "[[H5]]", 6) == 0) { *out_text_start = line + 6; return 15.0; }
    if (strncmp(line, "[[H6]]", 6) == 0) { *out_text_start = line + 6; return 14.0; }
    if (strncmp(line, "[[LI]]", 6) == 0) { *out_text_start = line + 6; return 14.0; }
    if (strncmp(line, "[[P]]", 5)  == 0) { *out_text_start = line + 5; return 14.0; }

    return 14.0;
}

static void strip_close_markers(char *s) {
    //remove any trailing close markers
    const char *markers[] = {"[[/H1]]","[[/H2]]","[[/H3]]","[[/H4]]","[[/H5]]","[[/H6]]","[[/P]]","[[/LI]]",NULL};
    for (int i = 0; markers[i]; i++) {
        char *p = strstr(s, markers[i]);
        if (p) *p = '\0';
    }
}


void busto_renderer_render(cairo_t *cr, int width, int height) {
    //clear surface with white background
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_paint(cr);

    //draw URL bar background
    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    cairo_rectangle(cr, 10, 10, width - 20, 40);
    cairo_fill(cr);

    //draw URL bar border
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_rectangle(cr, 10, 10, width - 20, 40);
    cairo_stroke(cr);

    //draw URL text
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    //cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_select_font_face(cr, "ComicShannsMono Nerd Font", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

    cairo_set_font_size(cr, 16.0);
    cairo_move_to(cr, 15, 35);

    const char *url_display = renderer_state.url ? renderer_state.url : "about:blank";
    cairo_show_text(cr, url_display);

    //draw cursor if input is active
    if (renderer_state.url_input_active) {
        const char *url_text = url_display;
        size_t url_len = strlen(url_text);
        size_t cursor_pos = renderer_state.url_cursor_pos;
        if(cursor_pos > url_len){
            cursor_pos = url_len;
        }
        char url_prefix[1024];
        if(cursor_pos >= sizeof(url_prefix)){
            cursor_pos = sizeof(url_prefix) -1;
        }
        memcpy(url_prefix, url_text, cursor_pos);
        url_prefix[cursor_pos] = '\0';

        cairo_text_extents_t extents;
        cairo_text_extents(cr, url_prefix, &extents);
        double x = 15 + extents.x_advance + 2;
        cairo_move_to(cr, x, 35);
        cairo_line_to(cr, x, 25);
        /* cairo_move_to(cr, 15 + extents.width + 2, 35); */
        /* cairo_line_to(cr, 15 + extents.width + 2, 25); */
        cairo_stroke(cr);
    }

    //draw content area background
    //TODO: make it reactive or fuck it one theme.
    //191, 149, 249
    //43, 46, 59
    //cairo_set_source_rgb(cr, 191.0/256.0, 149.0/256.0, 249.0/256.0);
    cairo_set_source_rgb(cr, 43.0/256.0, 46.0/256.0, 59.0/256.0);
    cairo_rectangle(cr, 10, 60, width - 20, height - 70);
    cairo_fill(cr);

    //draw content
    if (renderer_state.content) {
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        //cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_select_font_face(cr, "ComicShannsMono Nerd Font", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

        cairo_set_font_size(cr, 14.0);

        char *content_copy = strdup(renderer_state.content);
        char *line = strtok(content_copy, "\n");
        int y = 85 - renderer_state.scroll_y;
        int max_width = width - 40;

        while (line && y < height - 20) {
            if (strlen(line) > 0) {
                const char *text_start = NULL;
                double font_size = font_for_marker(line, &text_start);

                char temp[1024];
                snprintf(temp, sizeof(temp), "%s", text_start ? text_start : "");

                strip_close_markers(temp);

                if (strncmp(line, "[[LI]]", 6) == 0) {
                    char with_bullet[1024];
                    snprintf(with_bullet, sizeof(with_bullet), "• %s", temp);
                    snprintf(temp, sizeof(temp), "%s", with_bullet);
                }

                cairo_set_font_size(cr, font_size);

                int line_step = (font_size >= 22.0) ? 32 : (font_size >= 18.0 ? 26 : 20);

                cairo_text_extents_t extents;
                cairo_text_extents(cr, temp, &extents);

                if (extents.width > max_width) {
                    char *pos = temp;
                    while (*pos && y < height - 20) {
                        char temp_line[512];
                        int char_count = 0;

                        while (*pos && char_count < (int)sizeof(temp_line) - 1) {
                            temp_line[char_count++] = *pos++;
                            temp_line[char_count] = '\0';

                            cairo_text_extents(cr, temp_line, &extents);
                            if (extents.width > max_width) {
                                if (char_count > 1) {
                                    pos--;
                                    char_count--;
                                    temp_line[char_count] = '\0';
                                }
                                break;
                            }
                        }

                        cairo_move_to(cr, 20, y);
                        cairo_show_text(cr, temp_line);
                        y += line_step;
                    }
                } else {
                    cairo_move_to(cr, 20, y);
                    cairo_show_text(cr, temp);
                    y += line_step;
                }
            } else {
                //blank line
                y += 20;
            }

            line = strtok(NULL, "\n");
        }


        free(content_copy);
    } else {
        //defualt controls
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        //cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_select_font_face(cr, "ComicShannsMono Nerd Font", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

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

void busto_renderer_set_cursor_pos(size_t pos) {
    renderer_state.url_cursor_pos = pos;
}

