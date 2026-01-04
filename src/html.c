#define _GNU_SOURCE
#include "../include/busto/html.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char* skip_whitespace(char* str) {
    while (*str && isspace(*str)) str++;
    return str;
}

static char* find_tag_end(char* tag_start) {
    char* end = tag_start;
    while (*end && *end != '>' && !isspace(*end)) end++;
    return end;
}

static char* extract_text_between_tags(char** html_ptr) {
    char* start = *html_ptr;
    char* end = start;

    while (*end && !(*end == '<' && *(end+1) != '!')) {
        end++;
    }

    if (end == start) {
        return NULL;
    }

    size_t len = end - start;
    char* text = malloc(len + 1);
    if (!text) return NULL;

    strncpy(text, start, len);
    text[len] = '\0';

    // Trim whitespace
    char* trimmed = text;
    while (*trimmed && isspace(*trimmed)) trimmed++;

    char* end_trimmed = trimmed + strlen(trimmed) - 1;
    while (end_trimmed > trimmed && isspace(*end_trimmed)) end_trimmed--;
    *(end_trimmed + 1) = '\0';

    *html_ptr = end;
    return strdup(trimmed);
}

static struct busto_html_element* parse_element(char** html_ptr) {
    if (!html_ptr || !*html_ptr) return NULL;

    char* html = *html_ptr;
    html = skip_whitespace(html);

    if (*html != '<') {
        //text content
        char* text = extract_text_between_tags(&html);
        if (!text) return NULL;

        struct busto_html_element* element = calloc(1, sizeof(struct busto_html_element));
        if (!element) {
            free(text);
            return NULL;
        }

        element->tag = strdup("#text");
        element->text = text;
        *html_ptr = html;
        return element;
    }

    html++;

    if (*html == '/') {
        //closing tag, return NULL to signal end of this element
        while (*html && *html != '>') html++;
        if (*html == '>') html++;
        *html_ptr = html;
        return NULL;
    }

    if (*html == '!') {
        while (*html && *html != '>') html++;
        if (*html == '>') html++;
        *html_ptr = html;
        return parse_element(html_ptr);
    }

    //opening tag
    char* tag_start = html;
    char* tag_end = find_tag_end(tag_start);
    size_t tag_len = tag_end - tag_start;

    char* tag_name = malloc(tag_len + 1);
    if (!tag_name) return NULL;

    strncpy(tag_name, tag_start, tag_len);
    tag_name[tag_len] = '\0';

    //convert to lowercase
    for (size_t i = 0; i < tag_len; i++) {
        tag_name[i] = tolower(tag_name[i]);
    }

    struct busto_html_element* element = calloc(1, sizeof(struct busto_html_element));
    if (!element) {
        free(tag_name);
        return NULL;
    }

    element->tag = tag_name;

    //skip to end of tag
    html = tag_end;
    while (*html && *html != '>') html++;
    if (*html == '>') html++;

    //check if it's a self-closing tag
    int self_closing = 0;
    if (*(tag_end - 1) == '/') {
        self_closing = 1;
    }

    //special case for title
    if (strcmp(tag_name, "title") == 0) {
        char* title_start = html;
        char* title_end = strstr(html, "</title>");
        if (title_end) {
            size_t title_len = title_end - title_start;
            char* title_text = malloc(title_len + 1);
            if (title_text) {
                strncpy(title_text, title_start, title_len);
                title_text[title_len] = '\0';
                // Store title in the element for now
                element->text = title_text;
            }
            html = title_end + strlen("</title>");
        }
    }

    if (!self_closing && strcmp(tag_name, "br") != 0 && strcmp(tag_name, "img") != 0) {
        //parse children
        struct busto_html_element** last_child_ptr = &element->children;

        while (*html) {
            html = skip_whitespace(html);

            if (*html == '<' && *(html + 1) == '/') {
                // Closing tag for this element
                while (*html && *html != '>') html++;
                if (*html == '>') html++;
                break;
            }

            struct busto_html_element* child = parse_element(&html);
            if (child) {
                *last_child_ptr = child;
                last_child_ptr = &child->next;
            } else {
                break;
            }
        }
    }

    *html_ptr = html;
    return element;
}

static void free_element_recursive(struct busto_html_element *element) {
    if (!element) return;

    if (element->tag) free(element->tag);
    if (element->text) free(element->text);

    struct busto_html_element *child = element->children;
    while (child) {
        struct busto_html_element *next = child->next;
        free_element_recursive(child);
        child = next;
    }

    free(element);
}

struct busto_html_document *busto_html_parse(const char *html) {
    if (!html) return NULL;

    struct busto_html_document *doc = calloc(1, sizeof(struct busto_html_document));
    if (!doc) return NULL;

    doc->raw_html = strdup(html);
    char* html_copy = strdup(html);
    char* html_ptr = html_copy;

    doc->root = parse_element(&html_ptr);

    //try to extract title
    if (doc->root) {
        // Simple title extraction - look for title element
        char* title_start = strstr(html_copy, "<title>");
        if (title_start) {
            title_start += 7; // Skip "<title>"
            char* title_end = strstr(title_start, "</title>");
            if (title_end) {
                size_t title_len = title_end - title_start;
                doc->title = malloc(title_len + 1);
                if (doc->title) {
                    strncpy(doc->title, title_start, title_len);
                    doc->title[title_len] = '\0';
                }
            }
        }
    }

    free(html_copy);
    return doc;
}

void busto_html_document_free(struct busto_html_document *doc) {
    if (!doc) return;

    if (doc->root) {
        free_element_recursive(doc->root);
    }
    if (doc->title) {
        free(doc->title);
    }
    if (doc->raw_html) {
        free(doc->raw_html);
    }

    free(doc);
}

void busto_html_render_text(struct busto_html_element *element, int depth) {
    if (!element) return;

    if (element->text && strcmp(element->tag, "#text") == 0) {
        printf("%s", element->text);
    } else if (element->tag && strcmp(element->tag, "#text") != 0) {
        for (int i = 0; i < depth; i++) {
            printf("  ");
        }
        printf("<%s>\n", element->tag);

        struct busto_html_element *child = element->children;
        while (child) {
            busto_html_render_text(child, depth + 1);
            child = child->next;
        }
    }
}

void busto_html_extract_text(struct busto_html_element *element, char *buffer, size_t buffer_size) {
    if (!element || !buffer || buffer_size == 0) return;

    if (element->text && strcmp(element->tag, "#text") == 0) {
        strncat(buffer, element->text, buffer_size - strlen(buffer) - 1);
    }

    int is_block_element = 0;
    if (element->tag && strcmp(element->tag, "#text") != 0) {
        if (strcmp(element->tag, "p") == 0 ||
            strcmp(element->tag, "div") == 0 ||
            strcmp(element->tag, "h1") == 0 ||
            strcmp(element->tag, "h2") == 0 ||
            strcmp(element->tag, "h3") == 0 ||
            strcmp(element->tag, "h4") == 0 ||
            strcmp(element->tag, "h5") == 0 ||
            strcmp(element->tag, "h6") == 0 ||
            strcmp(element->tag, "li") == 0 ||
            strcmp(element->tag, "br") == 0) {
            is_block_element = 1;
        }
    }

    struct busto_html_element *child = element->children;
    while (child) {
        busto_html_extract_text(child, buffer, buffer_size);
        child = child->next;
    }

    if (is_block_element && strlen(buffer) > 0 && buffer[strlen(buffer) - 1] != '\n') {
        strncat(buffer, "\n", buffer_size - strlen(buffer) - 1);
    }
}

static void buf_append(char *buffer, size_t buffer_size, const char *s) {
    if (!buffer || buffer_size == 0 || !s) return;
    size_t cur = strlen(buffer);
    if (cur >= buffer_size - 1) return;
    strncat(buffer, s, buffer_size - cur - 1);
}

static int is_tag(const struct busto_html_element *e, const char *name) {
    return e && e->tag && strcmp(e->tag, name) == 0;
}

static const char* style_open_marker(const char *tag) {
    if (!tag) return NULL;
    if (strcmp(tag, "h1") == 0) return "[[H1]]";
    if (strcmp(tag, "h2") == 0) return "[[H2]]";
    if (strcmp(tag, "h3") == 0) return "[[H3]]";
    if (strcmp(tag, "h4") == 0) return "[[H4]]";
    if (strcmp(tag, "h5") == 0) return "[[H5]]";
    if (strcmp(tag, "h6") == 0) return "[[H6]]";
    if (strcmp(tag, "p")  == 0) return "[[P]]";
    if (strcmp(tag, "li") == 0) return "[[LI]]";
    return NULL;
}

static const char* style_close_marker(const char *tag) {
    if (!tag) return NULL;
    if (strcmp(tag, "h1") == 0) return "[[/H1]]";
    if (strcmp(tag, "h2") == 0) return "[[/H2]]";
    if (strcmp(tag, "h3") == 0) return "[[/H3]]";
    if (strcmp(tag, "h4") == 0) return "[[/H4]]";
    if (strcmp(tag, "h5") == 0) return "[[/H5]]";
    if (strcmp(tag, "h6") == 0) return "[[/H6]]";
    if (strcmp(tag, "p")  == 0) return "[[/P]]";
    if (strcmp(tag, "li") == 0) return "[[/LI]]";
    return NULL;
}

static int is_blockish_tag(const char *tag) {
    if (!tag) return 0;
    return (strcmp(tag, "p") == 0 ||
            strcmp(tag, "div") == 0 ||
            strcmp(tag, "h1") == 0 ||
            strcmp(tag, "h2") == 0 ||
            strcmp(tag, "h3") == 0 ||
            strcmp(tag, "h4") == 0 ||
            strcmp(tag, "h5") == 0 ||
            strcmp(tag, "h6") == 0 ||
            strcmp(tag, "li") == 0 ||
            strcmp(tag, "br") == 0);
}

void busto_html_extract_rich_text(struct busto_html_element *element,
                                 char *buffer, size_t buffer_size) {
    if (!element || !buffer || buffer_size == 0) return;

    if (element->text && element->tag && strcmp(element->tag, "#text") == 0) {
        buf_append(buffer, buffer_size, element->text);
        return;
    }

    const char *open = style_open_marker(element->tag);
    const char *close = style_close_marker(element->tag);

    if (open) buf_append(buffer, buffer_size, open);

    //br is new line
    if (is_tag(element, "br")) {
        buf_append(buffer, buffer_size, "\n");
    }

    struct busto_html_element *child = element->children;
    while (child) {
        busto_html_extract_rich_text(child, buffer, buffer_size);
        child = child->next;
    }

    if (close) buf_append(buffer, buffer_size, close);

    //add newline after block-ish tags but avoid spamming
    if (is_blockish_tag(element->tag)) {
        size_t n = strlen(buffer);
        if (n > 0 && buffer[n - 1] != '\n') {
            buf_append(buffer, buffer_size, "\n");
        }
    }
}

