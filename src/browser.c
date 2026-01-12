#define _GNU_SOURCE
#include "../include/busto/window.h"
#include "../include/busto/renderer.h"
#include "../include/busto/http.h"
#include "../include/busto/html.h"
#include "../include/busto/input.h"
#include "../include/busto/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <poll.h>
#include <unistd.h>

static struct busto_input *g_input = NULL;
static struct busto_window *g_window = NULL;
static char *g_current_url = NULL;
static pthread_t g_fetch_thread;
static int g_fetching = 0;

static void refresh_display(void) {
    //printf("Refreshing display...\n");
    //busto_window_redraw(g_window);
    busto_window_request_redraw(g_window);
}

static void* fetch_url_thread(void *arg) {
    char *url = (char*)arg;

    printf("Fetching URL: %s\n", url);

    char *content = busto_http_get(url);
    if (content) {
        //extract text
        struct busto_html_document *doc = busto_html_parse(content);
        if (doc) {
            char text_buffer[8192] = "";
            if (doc->root) {
                //busto_html_extract_text(doc->root, text_buffer, sizeof(text_buffer));
                busto_html_extract_rich_text(doc->root, text_buffer, sizeof(text_buffer));
            }

            busto_renderer_set_content(text_buffer[0] ? text_buffer : content);

            if (doc->title) {
                char title[256];
                snprintf(title, sizeof(title), "Busto Browser - %s", doc->title);
                busto_window_set_title(g_window, title);
            }

            busto_html_document_free(doc);
        } else {
            //show raw if fails
            busto_renderer_set_content(content);
        }

        busto_http_cleanup(content);
    } else {
        busto_renderer_set_content("Failed to load page");
    }

    g_fetching = 0;
    busto_renderer_set_input_active(0);
	//auto unfocus after load
    busto_input_deactivate(g_input);

    //refresh fter load
    refresh_display();

    free(url);
    return NULL;
}

static void load_url(const char *url) {
    if (!url || g_fetching) return;

    if (g_current_url) {
        free(g_current_url);
    }
    g_current_url = strdup(url);

    //update inputa nd render
    busto_input_set_url(g_input, url);
    busto_renderer_set_url(url);
    busto_renderer_set_content("Loading...");

    //prob shows loading here
    refresh_display();
    g_fetching = 1;
    pthread_create(&g_fetch_thread, NULL, fetch_url_thread, strdup(url));
    pthread_detach(g_fetch_thread);
}

static void reload_current_page(void) {
    if (g_current_url && !g_fetching) {
        printf("Reloading current page: %s\n", g_current_url);
        load_url(g_current_url);
    }
}

static void handle_key(struct busto_window *window, const char *key, void *user_data) {
    if (!key) return;

    printf("Key received: '%s'\n", key);

    if (busto_input_is_active(g_input)) {
        busto_input_handle_key(g_input, key);

        busto_renderer_set_url(busto_input_get_url(g_input));
        busto_renderer_set_input_active(1);

        if (strcmp(key, "Return") == 0) {
            const char *url = busto_input_get_url(g_input);
            if (url && strlen(url) > 0) {
                load_url(url);
            }
        } else if (strcmp(key, "Escape") == 0) {
            printf("Unfocusing URL bar\n");
            busto_input_deactivate(g_input);
            busto_renderer_set_input_active(0);
        }
    } else {
        //global key handling when not in input mode
		//TODO: have vim navigation like ctrl+d and u for scrolling, selecting text and all
        if (strcmp(key, "l") == 0) {
            // Activate URL bar
            printf("Activating URL bar\n");
            busto_input_activate(g_input);
            busto_renderer_set_input_active(1);
			//show cursor
            refresh_display();
        } else if (strcmp(key, "q") == 0) {
            printf("Quitting...\n");
            busto_window_destroy(window);
            exit(0);
        } else if (strcmp(key, "Up") == 0) {
            busto_renderer_scroll(-50);
            refresh_display();
        } else if (strcmp(key, "Down") == 0) {
            busto_renderer_scroll(50);
            refresh_display();
        } else if (strcmp(key, "r") == 0 || strcmp(key, "F5") == 0) {
            reload_current_page();
        } else if (strcmp(key, "?") == 0) {
            busto_renderer_set_content(
                "BUSTO BROWSER HELP\n\n"
                "CONTROLS:\n"
                "  l           - Focus URL bar\n"
                "  q           - Quit browser\n"
                "  r / F5      - Reload current page\n"
                "  Up/Down     - Scroll content\n"
                "  ?           - Show this help\n"
                "\n"
                "URL BAR (when focused):\n"
                "  Type URL    - Enter web address\n"
                "  Enter       - Load the URL\n"
                "  Escape      - Unfocus URL bar\n"
                "  Backspace   - Delete character\n"
                "  Left/Right  - Move cursor\n"
                "  Home/End    - Jump to start/end\n"
                "\n"
                "TIPS:\n"
                "  - Page auto-refreshes after loading\n"
                "  - Press Escape to leave URL bar\n"
                "  - Use 'r' to reload current page"
            );
            refresh_display();
        }
    }

    //always refresh after key press
    refresh_display();
}

int main() {
    printf("Starting Busto Browser...\n");
    printf("Window keyboard input is active!\n");
    printf("Press '?' in the window for help.\n\n");

    g_window = busto_window_create(1920, 1080);
    if (!g_window) {
        fprintf(stderr, "Failed to create window\n");
        return 1;
    }

    g_input = busto_input_create();
    if (!g_input) {
        fprintf(stderr, "Failed to create input handler\n");
        busto_window_destroy(g_window);
        return 1;
    }

    busto_window_set_key_handler(g_window, handle_key, NULL);

    busto_window_set_title(g_window, "Busto Browser - Press '?' for help");

	//TODO: set default url
    busto_renderer_set_url("about:blank");
    busto_renderer_set_content(
        "Welcome to Busto Browser!\n\n"
        "Type in the window:\n"
        "  ? - Show help\n"
        "  l - Focus URL bar\n"
        "  r - Reload page\n"
        "  q - Quit\n"
        "  Up/Down - Scroll\n\n"
        "Click this window and type keys!\n\n"
        "NEW FEATURES:\n"
        "• Auto-refresh after loading pages\n"
        "• Press Escape to unfocus URL bar\n"
        "• Press 'r' or F5 to reload current page"
    );

	//do i need this
    refresh_display();


    //main loop
    while(busto_window_is_running(g_window)) {
        //framerate tick
        busto_window_update_repeats(g_window);
        //if something happens, redraw also
        if(g_window->needs_redraw){
            g_window->needs_redraw = 0;
            busto_window_redraw(g_window);
        }
        //process any queued events
        wl_display_dispatch_pending(g_window->display);
        while(wl_display_prepare_read(g_window->display) !=0) {
            wl_display_dispatch_pending(g_window->display);
        }
        wl_display_flush(g_window->display);

        //wait a bit or until wayland got some info
        int fd = wl_display_get_fd(g_window->display);

        struct pollfd pfd = {
            .fd = fd,
            .events = POLLIN
        };
        //16 ms , but could use 8 for snappeir
        int ret = poll(&pfd, 1, 16);
        if(ret >0 && (pfd.revents & POLLIN)) {
            wl_display_read_events(g_window->display);
            wl_display_dispatch_pending(g_window->display);
        }
        else {
            wl_display_cancel_read(g_window->display);
        }

    }

    //get rid of threads
    if (g_fetching) {
        pthread_join(g_fetch_thread, NULL);
    }

    busto_input_destroy(g_input);
    busto_window_destroy(g_window);
    busto_renderer_free();

    if (g_current_url) {
        free(g_current_url);
    }

    return 0;
}
