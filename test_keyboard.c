#define _GNU_SOURCE
#include "include/busto/window.h"
#include "include/busto/renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char key_log[10000] = "";

static void handle_key(struct busto_window *window, const char *key, void *user_data) {
    if (!key) return;
    
    printf("WINDOW KEY: '%s'\n", key);
    
    // Log the key
    char temp[100];
    snprintf(temp, sizeof(temp), "Key: '%s'\n", key);
    strcat(key_log, temp);
    
    // Update display with the log
    busto_renderer_set_content(key_log);
    
    // Redraw
    busto_window_redraw(window);
    
    // Quit on 'q'
    if (strcmp(key, "q") == 0) {
        printf("Quitting...\n");
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    
    printf("Keyboard Test - Click the window and type keys!\n");
    printf("Press 'q' to quit\n");
    
    // Create window
    struct busto_window *window = busto_window_create(800, 600);
    if (!window) {
        fprintf(stderr, "Failed to create window\n");
        return 1;
    }
    
    // Set up key handler
    busto_window_set_key_handler(window, handle_key, NULL);
    
    // Set window title
    busto_window_set_title(window, "Keyboard Test - Type Keys!");
    
    // Initialize renderer
    busto_renderer_set_content("Click this window and type keys...\nPress 'q' to quit\n\nKey log will appear here:");
    
    // Initial draw
    busto_window_redraw(window);
    
    // Main loop
    while (busto_window_is_running(window)) {
        busto_window_dispatch(window);
    }
    
    busto_window_destroy(window);
    busto_renderer_free();
    
    return 0;
}