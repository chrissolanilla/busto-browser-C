#ifndef BUSTO_HTTP_H
#define BUSTO_HTTP_H

char *busto_http_get(const char *url);
void busto_http_cleanup(char *response);

#endif