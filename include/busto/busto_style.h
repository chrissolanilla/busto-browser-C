#ifndef BUSTO_STYLE_H
#define BUSTO_STYLE_H

#include <stddef.h>

#define BUSTO_MAX_STYLE_RULES 64
#define BUSTO_MAX_TAG_NAME 32

enum busto_selector_type {
    BUSTO_SELECTOR_TAG,
    BUSTO_SELECTOR_CLASS
};

struct busto_color {
    double r;
    double g;
    double b;
    double a;
};

struct busto_style_rule {
    char tag[BUSTO_MAX_TAG_NAME];
    enum busto_selector_type selector_type;
    int has_color;
    struct busto_color color;

    int has_background_color;
    struct busto_color background_color;
};

struct busto_stylesheet {
    struct busto_style_rule rules[BUSTO_MAX_STYLE_RULES];
    size_t rule_count;
};

void busto_stylesheet_init(struct busto_stylesheet *stylesheet);
void busto_stylesheet_parse(struct busto_stylesheet *stylesheet, const char *css);

const struct busto_style_rule *busto_stylesheet_find_tag(
    const struct busto_stylesheet *stylesheet,
    const char *tag
);

const struct busto_style_rule *busto_stylesheet_find_class(
    const struct busto_stylesheet *stylesheet,
    const char *class_name
);

#endif
