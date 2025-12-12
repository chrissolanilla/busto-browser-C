#ifndef BUSTO_UTILS_H
#define BUSTO_UTILS_H

#include <stddef.h>

char *busto_strdup(const char *s);
void *busto_malloc(size_t size);
void busto_free(void *ptr);

#endif