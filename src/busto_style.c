#include "../include/busto/busto_style.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char *skip_space(char *s)
{
    while (*s && isspace((unsigned char)*s)) {
        s++;
    }

    return s;
}

static void trim_in_place(char *s)
{
    char *end;

    s = skip_space(s);

    end = s + strlen(s);

    while (end > s && isspace((unsigned char)*(end - 1))) {
        end--;
    }

    *end = '\0';
}

static int parse_named_color(const char *value, struct busto_color *out) {
    if (strcmp(value, "red") == 0) {
        *out = (struct busto_color){1.0, 0.0, 0.0, 1.0};
        return 1;
    }

    if (strcmp(value, "green") == 0) {
        *out = (struct busto_color){0.0, 1.0, 0.0, 1.0};
        return 1;
    }

    if (strcmp(value, "blue") == 0) {
        *out = (struct busto_color){0.0, 0.0, 1.0, 1.0};
        return 1;
    }

    if (strcmp(value, "white") == 0) {
        *out = (struct busto_color){1.0, 1.0, 1.0, 1.0};
        return 1;
    }

    if (strcmp(value, "black") == 0) {
        *out = (struct busto_color){0.0, 0.0, 0.0, 1.0};
        return 1;
    }

    if (strcmp(value, "yellow") == 0) {
        *out = (struct busto_color){1.0, 1.0, 0.0, 1.0};
        return 1;
    }

    return 0;
}

static int parse_hex_color(const char *value, struct busto_color *out) {
    unsigned int r;
    unsigned int g;
    unsigned int b;

    if (value[0] != '#') {
        return 0;
    }

    if (strlen(value) != 7) {
        return 0;
    }

    if (sscanf(value + 1, "%02x%02x%02x", &r, &g, &b) != 3) {
        return 0;
    }

    out->r = r / 255.0;
    out->g = g / 255.0;
    out->b = b / 255.0;
    out->a = 1.0;

    return 1;
}

static int parse_color_value(const char *value, struct busto_color *out) {
    return parse_named_color(value, out) || parse_hex_color(value, out);
}

void busto_stylesheet_init(struct busto_stylesheet *stylesheet) {
    if (!stylesheet) {
        return;
    }

    memset(stylesheet, 0, sizeof(*stylesheet));
}

const struct busto_style_rule *busto_stylesheet_find_tag(
    const struct busto_stylesheet *stylesheet,
    const char *tag
) {
    size_t i;

    if (!stylesheet || !tag) {
        return NULL;
    }

    for (i = 0; i < stylesheet->rule_count; i++) {
        if (strcmp(stylesheet->rules[i].tag, tag) == 0) {
            return &stylesheet->rules[i];
        }
    }

    return NULL;
}

void busto_stylesheet_parse(struct busto_stylesheet *stylesheet, const char *css) {
    char *copy;
    char *cursor;

    if (!stylesheet) {
        return;
    }

    busto_stylesheet_init(stylesheet);

    if (!css) {
        return;
    }

    copy = strdup(css);

    if (!copy) {
        return;
    }

    cursor = copy;

    while (*cursor && stylesheet->rule_count < BUSTO_MAX_STYLE_RULES) {
        char *selector_start;
        char *selector_end;
        char *block_start;
        char *block_end;
        char *property;
        char *value;
        char *semicolon;
        struct busto_style_rule *rule;

        selector_start = skip_space(cursor);
        selector_end = strchr(selector_start, '{');

        if (!selector_end) {
            break;
        }

        *selector_end = '\0';
        trim_in_place(selector_start);

        block_start = selector_end + 1;
        block_end = strchr(block_start, '}');

        if (!block_end) {
            break;
        }

        *block_end = '\0';

        rule = &stylesheet->rules[stylesheet->rule_count];

        snprintf(rule->tag, sizeof(rule->tag), "%s", selector_start);

        property = strstr(block_start, "color");

        if (property) {
            value = strchr(property, ':');

            if (value) {
                value++;
                value = skip_space(value);

                semicolon = strchr(value, ';');

                if (semicolon) {
                    *semicolon = '\0';
                }

                trim_in_place(value);

                if (parse_color_value(value, &rule->color)) {
                    rule->has_color = 1;
                }
            }
        }

        if (rule->tag[0] != '\0') {
            stylesheet->rule_count++;
        }

        cursor = block_end + 1;
    }

    free(copy);
}
