#include "../include/busto/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *busto_strdup(const char *s) {
    if (!s) return NULL;
    
    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    if (!copy) return NULL;
    
    strcpy(copy, s);
    return copy;
}

void *busto_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Failed to allocate %zu bytes\n", size);
        return NULL;
    }
    return ptr;
}

void busto_free(void *ptr) {
    if (ptr) {
        free(ptr);
    }
}