#ifndef BUSTO_HTML_H
#define BUSTO_HTML_H

#include <stddef.h>

struct busto_html_element {
    char *tag;
    char *text;
    struct busto_html_element *children;
    struct busto_html_element *next;
};

struct busto_html_document {
    struct busto_html_element *root;
    char *title;
    char *raw_html;
};

struct busto_html_document *busto_html_parse(const char *html);
void busto_html_document_free(struct busto_html_document *doc);
void busto_html_render_text(struct busto_html_element *element, int depth);
void busto_html_extract_text(struct busto_html_element *element, char *buffer, size_t buffer_size);
void busto_html_extract_rich_text(struct busto_html_element *element,
                                 char *buffer, size_t buffer_size);


#endif
