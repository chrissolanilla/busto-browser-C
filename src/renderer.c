#define _GNU_SOURCE
#include "../include/busto/renderer.h"
#include "../include/busto/busto_style.h"
#include <cairo/cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    char *url;
    char *content;
    int url_input_active;
    enum busto_content_mode content_mode;
    int scroll_y;
    size_t url_cursor_pos;

    struct busto_draw_command *draw_commands;
    struct busto_stylesheet stylesheet;
    size_t draw_command_count;
    size_t draw_command_cap;

    double fill_r;
    double fill_g;
    double fill_b;
    double fill_a;
} renderer_state = {
    .fill_a = 1.0
};


void busto_renderer_set_stylesheet(const struct busto_stylesheet *stylesheet) {
    if (stylesheet) {
        renderer_state.stylesheet = *stylesheet;
    }
    else {
        busto_stylesheet_init(&renderer_state.stylesheet);
    }
}

void busto_renderer_set_content_mode(enum busto_content_mode mode)
{
    renderer_state.content_mode = mode;
}

static double font_for_marker(const char *line,
        const char **out_text_start,
        const char **out_tag,
        char *out_class,
        size_t out_class_size) {

    *out_text_start = line;
    *out_tag = NULL;
    if (out_class && out_class_size > 0) {
        out_class[0] = '\0';
    }

    struct marker_style {
        const char *marker;
        const char *tag;
        double font_size;
    } markers[] = {
        {"H1", "h1", 28.0},
        {"H2", "h2", 22.0},
        {"H3", "h3", 18.0},
        {"H4", "h4", 16.0},
        {"H5", "h5", 15.0},
        {"H6", "h6", 14.0},
        {"LI", "li", 14.0},
        {"P",  "p",  14.0},
    };

    for (size_t i = 0; i < sizeof(markers) / sizeof(markers[0]); i++) {
        size_t marker_len = strlen(markers[i].marker);
        const char *after_marker;
        const char *close;

        if (strncmp(line, "[[", 2) != 0 ||
            strncmp(line + 2, markers[i].marker, marker_len) != 0) {
            continue;
        }

        after_marker = line + 2 + marker_len;

        if (strncmp(after_marker, "]]", 2) == 0) {
            *out_text_start = after_marker + 2;
            *out_tag = markers[i].tag;
            return markers[i].font_size;
        }

        if (*after_marker == ':') {
            close = strstr(after_marker, "]]");

            if (close) {
                size_t class_len = (size_t)(close - (after_marker + 1));

                if (out_class && out_class_size > 0) {
                    if (class_len >= out_class_size) {
                        class_len = out_class_size - 1;
                    }

                    memcpy(out_class, after_marker + 1, class_len);
                    out_class[class_len] = '\0';
                }

                *out_text_start = close + 2;
                *out_tag = markers[i].tag;
                return markers[i].font_size;
            }
        }
    }

    return 14.0;
}

static const struct busto_style_rule *style_rule_for_text(
    const char *tag,
    const char *class_name
) {
    const struct busto_style_rule *rule = NULL;

    if (class_name && class_name[0]) {
        rule = busto_stylesheet_find_class(&renderer_state.stylesheet, class_name);
    }

    if (!rule && tag) {
        rule = busto_stylesheet_find_tag(&renderer_state.stylesheet, tag);
    }

    return rule;
}

static void apply_text_background(
    cairo_t *cr,
    const char *tag,
    const char *class_name,
    double x,
    double y,
    double width,
    double height
) {
    const struct busto_style_rule *rule = style_rule_for_text(tag, class_name);

    if (!rule || !rule->has_background_color) {
        return;
    }

    cairo_set_source_rgba(
        cr,
        rule->background_color.r,
        rule->background_color.g,
        rule->background_color.b,
        rule->background_color.a
    );

    cairo_rectangle(cr, x, y, width, height);
    cairo_fill(cr);
}

static void apply_body_background(cairo_t *cr) {
    const struct busto_style_rule *rule;

    rule = busto_stylesheet_find_tag(&renderer_state.stylesheet, "body");

    if (rule && rule->has_background_color) {
        cairo_set_source_rgba(
            cr,
            rule->background_color.r,
            rule->background_color.g,
            rule->background_color.b,
            rule->background_color.a
        );
    }
    else {
        cairo_set_source_rgb(cr, 43.0 / 256.0, 46.0 / 256.0, 59.0 / 256.0);
    }
}

static void apply_text_color(cairo_t *cr, const char *tag, const char *class_name) {
    const struct busto_style_rule *rule = style_rule_for_text(tag, class_name);

    if (rule && rule->has_color) {
        cairo_set_source_rgba(
            cr,
            rule->color.r,
            rule->color.g,
            rule->color.b,
            rule->color.a
        );
    }
    else {
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    }
}

static void strip_close_markers(char *s) {
    //remove any trailing close markers
    const char *markers[] = {"[[/H1]]","[[/H2]]","[[/H3]]","[[/H4]]","[[/H5]]","[[/H6]]","[[/P]]","[[/LI]]",NULL};
    for (int i = 0; markers[i]; i++) {
        char *p = strstr(s, markers[i]);
        if (p) *p = '\0';
    }
}

static char *expand_tabs(const char *line, size_t tab_width) {
    if (!line || tab_width == 0)
        return NULL;

    size_t len = strlen(line);

    //worst case every char is a tab and needs tab space
    char *result = malloc(len * tab_width + 1);
    if (!result)
        return NULL;

    size_t out = 0;
    size_t column = 0;

    for (size_t i = 0; line[i]; i++) {

        if (line[i] == '\t') {
            size_t spaces =
                tab_width - (column % tab_width);

            for (size_t j = 0; j < spaces; j++) {
                result[out++] = ' ';
                column++;
            }
        }
        else {
            result[out++] = line[i];
            column++;
        }
    }

    result[out] = '\0';
    return result;
}

static void render_plain_content(
    cairo_t *cr,
    const char *content,
    int width,
    int height
) {
    //not using width i gess
    (void)width;
    if (!content)
        return;

    char *copy = strdup(content);
    if (!copy)
        return;

    char *remaining = copy;
    char *line;

    int y = 85 - renderer_state.scroll_y;

    cairo_set_font_size(cr, 14.0);

    while ((line = strsep(&remaining, "\n")) != NULL) {

        if (y >= 60 && y < height - 20) {
            char *expanded = expand_tabs(line, 4);

            if (expanded) {
                cairo_move_to(cr, 20, y);
                cairo_show_text(cr, expanded);
                free(expanded);
            }
        }

        /*
         * ALWAYS advance, including empty lines.
         */
        y += 20;

        if (y >= height)
            break;
    }

    free(copy);
}

static void render_rich_content(
    cairo_t *cr,
    const char *content,
    int width,
    int height
) {
    if (!content)
        return;

    char *content_copy = strdup(content);
    if (!content_copy)
        return;

    char *line = strtok(content_copy, "\n");

    int y = 85 - renderer_state.scroll_y;
    int max_width = width - 40;

    while (line && y < height - 20) {

        if (strlen(line) > 0) {
            const char *text_start = NULL;
            const char *tag = NULL;
            char class_name[64];

            double font_size =
                font_for_marker(line, &text_start, &tag, class_name, sizeof(class_name));

            char temp[1024];

            snprintf(
                temp,
                sizeof(temp),
                "%s",
                text_start ? text_start : ""
            );

            strip_close_markers(temp);

            if (tag && strcmp(tag, "li") == 0) {
                char with_bullet[1024];

                snprintf(
                    with_bullet,
                    sizeof(with_bullet),
                    "• %.1018s",
                    temp
                );

                snprintf(
                    temp,
                    sizeof(temp),
                    "%s",
                    with_bullet
                );
            }

            cairo_set_font_size(cr, font_size);

            int line_step =
                (font_size >= 22.0)
                    ? 32
                    : (font_size >= 18.0 ? 26 : 20);

            cairo_text_extents_t extents;
            cairo_text_extents(cr, temp, &extents);

            if (extents.width > max_width) {
                char *pos = temp;

                while (*pos && y < height - 20) {
                    char temp_line[512];
                    int char_count = 0;

                    while (
                        *pos &&
                        char_count < (int)sizeof(temp_line) - 1
                    ) {
                        temp_line[char_count++] = *pos++;
                        temp_line[char_count] = '\0';

                        cairo_text_extents(
                            cr,
                            temp_line,
                            &extents
                        );

                        if (extents.width > max_width) {
                            if (char_count > 1) {
                                pos--;
                                char_count--;
                                temp_line[char_count] = '\0';
                            }

                            break;
                        }
                    }

                    double text_x = 20;
                    double rect_y = y - font_size;
                    //right now it starts slightly above the text
                    double rect_h = line_step+5;
                    double rect_w = max_width;

                    apply_text_background(cr, tag, class_name, text_x, rect_y, rect_w, rect_h);
                    cairo_move_to(cr, 20, y);
                    apply_text_color(cr, tag, class_name);
                    cairo_show_text(cr, temp_line);

                    y += line_step;
                }
            }
            else {
                double text_x = 20;
                double rect_y = y - font_size;
                //for non wrapped stuff, background starts slightly above the text
                double rect_h = line_step+5;
                double rect_w = max_width;

                apply_text_background(cr, tag, class_name, text_x, rect_y, rect_w, rect_h);

                cairo_move_to(cr, 20, y);
                apply_text_color(cr, tag, class_name);
                cairo_show_text(cr, temp);

                y += line_step;
            }
        }
        else {
            y += 20;
        }

        line = strtok(NULL, "\n");
    }

    free(content_copy);
}

static void render_graphics(cairo_t *cr) {
    for (size_t i = 0; i < renderer_state.draw_command_count; i++) {
        struct busto_draw_command *cmd = &renderer_state.draw_commands[i];

        if (cmd->type == BUSTO_DRAW_FILL_RECT) {
            cairo_set_source_rgba(cr, cmd->r, cmd->g, cmd->b, cmd->a);
            cairo_rectangle(cr, cmd->x, cmd->y, cmd->w, cmd->h);
            cairo_fill(cr);
        }
    }
}

void busto_renderer_render(cairo_t *cr, int width, int height) {
    //clear surface with white background
    //also the window bg
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
    //
    //
    /* cairo_set_source_rgb(cr, 43.0/256.0, 46.0/256.0, 59.0/256.0); */
    //instead of above we can fill in the style bgs now.
    apply_body_background(cr);
    cairo_rectangle(cr, 10, 60, width - 20, height - 70);
    cairo_fill(cr);

    //draw content
    if (renderer_state.content) {
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        //cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_select_font_face(cr, "ComicShannsMono Nerd Font", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

        cairo_set_font_size(cr, 14.0);

        if (renderer_state.content_mode == BUSTO_CONTENT_PLAIN) {
            render_plain_content(
                cr,
                renderer_state.content,
                width,
                height
            );
        } else {
            render_rich_content(
                cr,
                renderer_state.content,
                width,
                height
            );
        }

    } else {
        //defualt controls
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        //cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_select_font_face(cr, "ComicShannsMono Nerd Font", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

        cairo_set_font_size(cr, 18.0);
        cairo_move_to(cr, 20, 85);
        cairo_show_text(cr, "Enter a URL to get started");
    }

    render_graphics(cr);
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

    free(renderer_state.draw_commands);
    renderer_state.draw_commands = NULL;
    renderer_state.draw_command_count = 0;
    renderer_state.draw_command_cap = 0;
}

void busto_renderer_set_cursor_pos(size_t pos) {
    renderer_state.url_cursor_pos = pos;
}

void busto_renderer_graphics_clear(void) {
    renderer_state.draw_command_count = 0;
}

void busto_renderer_graphics_set_fill(double r, double g, double b, double a) {
    renderer_state.fill_r = r;
    renderer_state.fill_g = g;
    renderer_state.fill_b = b;
    renderer_state.fill_a = a;
}

void busto_renderer_graphics_fill_rect(double x, double y, double w, double h) {
    if (w <= 0 || h <= 0) {
        return;
    }

    if (renderer_state.draw_command_count == renderer_state.draw_command_cap) {
        size_t new_cap = renderer_state.draw_command_cap == 0
            ? 64
            : renderer_state.draw_command_cap * 2;

        struct busto_draw_command *new_commands = realloc(
            renderer_state.draw_commands,
            new_cap * sizeof(*new_commands)
        );

        if (!new_commands) {
            return;
        }

        renderer_state.draw_commands = new_commands;
        renderer_state.draw_command_cap = new_cap;
    }

    struct busto_draw_command *cmd =
        &renderer_state.draw_commands[renderer_state.draw_command_count++];

    cmd->type = BUSTO_DRAW_FILL_RECT;
    cmd->x = x;
    cmd->y = y;
    cmd->w = w;
    cmd->h = h;
    cmd->r = renderer_state.fill_r;
    cmd->g = renderer_state.fill_g;
    cmd->b = renderer_state.fill_b;
    cmd->a = renderer_state.fill_a;
}
