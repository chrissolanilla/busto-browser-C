#define _GNU_SOURCE
#include "../include/busto/window.h"
#include "../include/busto/renderer.h"
#include "../include/busto/http.h"
#include "../include/busto/html.h"
#include "../include/busto/input.h"
#include "../include/busto/utils.h"
#include "../include/busto/busto_script.h"
#include "../include/busto/script_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#if defined(__linux__) || defined(__APPLE__)
#define BUSTO_ENABLE_SCRIPT 1
#endif

static struct busto_input *g_input = NULL;
static struct busto_window *g_window = NULL;
static char *g_current_url = NULL;
static pthread_t g_fetch_thread;
static int g_fetching = 0;
static int g_fetch_thread_active = 0;
static char lastKey[64] = {0};

static void load_url(const char *url);

/* enum busto_content_mode { */
/*     BUSTO_CONTENT_RICH, */
/*     BUSTO_CONTENT_PLAIN */
/* }; */

/* void busto_renderer_set_content_mode(enum busto_content_mode mode) { */
/*     renderer_state.content_mode = mode; */
/* } */

struct fetch_result {
    char *content;
    char *title;
    char *script_source;
    char *style_source;
    enum busto_content_mode mode;
    int ready;
};

static struct fetch_result g_pending_result = {0};

static pthread_mutex_t g_fetch_mutex = PTHREAD_MUTEX_INITIALIZER;

static void refresh_display(void) {
    //printf("Refreshing display...\n");
    //busto_window_redraw(g_window);
    busto_window_request_redraw(g_window);
}

#ifdef BUSTO_ENABLE_SCRIPT
static void script_set_content(const char *text)
{
    busto_renderer_set_content_mode(BUSTO_CONTENT_PLAIN);
    busto_renderer_set_content(text ? text : "");
    refresh_display();
}

static void script_set_title(const char *title)
{
    busto_window_set_title(g_window, title ? title : "Busto Browser");
}

static void script_navigate(const char *url)
{
    load_url(url);
}

static void script_request_redraw(void)
{
    refresh_display();
}

static void script_graphics_clear(void)
{
    busto_renderer_graphics_clear();
    refresh_display();
}

static void script_graphics_set_fill(double r, double g, double b, double a)
{
    busto_renderer_graphics_set_fill(r, g, b, a);
}

static void script_graphics_fill_rect(double x, double y, double w, double h)
{
    busto_renderer_graphics_fill_rect(x, y, w, h);
    refresh_display();
}

static int script_get_width(void) {
    return busto_window_get_width(g_window);
}

static int script_get_height(void) {
    return busto_window_get_height(g_window);
}

static struct busto_api g_busto_api = {
    .set_content = script_set_content,
    .set_title = script_set_title,
    .navigate = script_navigate,
    .request_redraw = script_request_redraw,
    .graphics = {
        .clear = script_graphics_clear,
        .set_fill = script_graphics_set_fill,
        .fill_rect = script_graphics_fill_rect,
    },
    .get_width = script_get_width,
    .get_height = script_get_height,
};
#endif

static void sync_urlbar_to_renderer(void) {
    busto_renderer_set_url(busto_input_get_url(g_input));
    busto_renderer_set_cursor_pos(g_input->cursor_pos);
    busto_renderer_set_input_active(busto_input_is_active(g_input));
}

static char *read_local_file(const char *url)
{
    if (!url || url[0] != '~') {
        return NULL;
    }
    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "HOME is not set\n");
        return NULL;
    }
    //lib.c can not really read ~ paths so make them /home
    size_t path_len = strlen(home) + strlen(url);
    char *path = malloc(path_len + 1);
    if (!path) {
        return NULL;
    }
    snprintf(path, path_len + 1, "%s%s", home, url + 1);
    printf("Opening local file: %s\n", path);
    FILE *file = fopen(path, "rb");
    free(path);
    if (!file) {
        perror("fopen");
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);
    char *buffer = malloc((size_t)file_size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    size_t bytes_read = fread(buffer, 1, (size_t)file_size, file);
    buffer[bytes_read] = '\0';
    fclose(file);
    return buffer;
}

static char *find_busto_style_src(const char *html) {
    const char *start = strstr(html, "<link rel=\"stylesheet\"");
    if(!start) {
        return NULL;
    }

    const char *href = strstr(start, "href=\"");
    if(!href) {
        return  NULL;
    }

    href+= strlen("href=\"");

    const char *end = strchr(href,'"');
    if(!end) {
        return NULL;
    }

    size_t len = (size_t)(end-href);

    char *result = malloc(len + 1);
    if(!result) {
        return NULL;
    }

    memcpy(result, href, len);
    result[len] = '\0';
    return result;
}

static char *find_busto_script_src(const char *html)
{
    const char *start =
        strstr(html, "<busto-script");

    if (!start) {
        return NULL;
    }

    const char *src = strstr(start, "src=\"");

    if (!src) {
        return NULL;
    }

    src += strlen("src=\"");

    const char *end = strchr(src, '"');

    if (!end) {
        return NULL;
    }

    size_t len = (size_t)(end - src);

    char *result = malloc(len + 1);

    if (!result) {
        return NULL;
    }

    memcpy(result, src, len);
    result[len] = '\0';

    return result;
}

char *load_file_url(char *file_url, char *result_file_source) {
    if (
        strncmp(file_url, "http://", 7) == 0 ||
        strncmp(file_url, "https://", 8) == 0
    ) {
        result_file_source = busto_http_get(file_url);
        if (!result_file_source) {
            fprintf(
                stderr,
                "Failed to load busto script: %s\n",
                file_url
            );
        }
    }
    else {
        fprintf(
            stderr,
            "Unsupported file URL: %s\n",
            file_url
        );
    }

    free(file_url);
    return result_file_source;
}

static void *fetch_url_thread(void *arg)
{
    char *url = arg;

    char *result_content = NULL;
    char *result_title = NULL;
    char *result_script_source = NULL;
    char *result_style_source = NULL;
    enum busto_content_mode result_mode = BUSTO_CONTENT_PLAIN;

    printf("Fetching URL: %s\n", url);

    if (url[0] == '~') {
        result_content = read_local_file(url);
        result_mode = BUSTO_CONTENT_PLAIN;

        if (!result_content) {
            result_content = strdup("Failed to open local file");
        }
    }
    else {
        char *content = busto_http_get(url);
        if (content) {
            //find bustoscript
            char *script_url = find_busto_script_src(content);
            //find style.css
            char *style_url = find_busto_style_src(content);

            if(style_url) {
                result_style_source = load_file_url(style_url, result_style_source);
            }
            if (script_url) {
                result_script_source = load_file_url(script_url, result_script_source);
            }

            struct busto_html_document *doc = busto_html_parse(content);

            if (doc) {
                size_t text_cap = 64 * 1024 * 1024;
                char *text_buffer = calloc(1, text_cap);

                if (text_buffer && doc->root) {
                    struct busto_text_buffer tb = {
                        .data = text_buffer,
                        .len = 0,
                        .cap = text_cap
                    };

                    busto_html_extract_rich_text_fast(doc->root, &tb);
                }

                if (text_buffer && text_buffer[0]) {
                    result_content = strdup(text_buffer);
                    result_mode = BUSTO_CONTENT_RICH;
                }
                else {
                    result_content = strdup(content);
                    result_mode = BUSTO_CONTENT_PLAIN;
                }

                if (doc->title) {
                    result_title = strdup(doc->title);
                }

                free(text_buffer);
                busto_html_document_free(doc);
            }
            else {
                result_content = strdup(content);
                result_mode = BUSTO_CONTENT_PLAIN;
            }

            busto_http_cleanup(content);
        }
        else {
            result_content = strdup("Failed to load page");
            result_mode = BUSTO_CONTENT_PLAIN;
        }
    }

    //prevent datarace with the ONLY shared-state interaction from the worker.
    pthread_mutex_lock(&g_fetch_mutex);

    free(g_pending_result.content);
    free(g_pending_result.title);
    free(g_pending_result.script_source);

    g_pending_result.content = result_content;
    g_pending_result.title = result_title;
    g_pending_result.script_source = result_script_source;
    g_pending_result.style_source = result_style_source;
    g_pending_result.mode = result_mode;
    g_pending_result.ready = 1;

    pthread_mutex_unlock(&g_fetch_mutex);

    free(url);

    return NULL;
}

static void load_url(const char *url) {
    if (!url || g_fetching) {
        return;
    }

    char *new_current_url = strdup(url);
    if (!new_current_url) {
        perror("strdup");
        return;
    }

    if (g_current_url) {
        free(g_current_url);
    }
    /* g_current_url = strdup(url); */
    g_current_url = new_current_url;

    //update inputa nd render
    /* busto_input_set_url(g_input, url); */
    busto_input_set_url(g_input, new_current_url);
    sync_urlbar_to_renderer();
    busto_renderer_set_url(new_current_url);
    busto_renderer_graphics_clear();
    busto_renderer_set_content("Loading...");

    //prob shows loading here
    refresh_display();
    g_fetching = 1;
    char *thread_url = strdup(new_current_url);

    if (!thread_url) {
        perror("strdup");
        g_fetching = 0;
        return;
    }

    if (pthread_create(&g_fetch_thread, NULL, fetch_url_thread, thread_url) == 0) {
        g_fetch_thread_active = 1;
    }
    else {
        perror("pthread_create");
        free(thread_url);
        g_fetching = 0;
    }
}

static void reload_current_page(void) {
    if (g_current_url && !g_fetching) {
        printf("Reloading current page: %s\n", g_current_url);
        load_url(g_current_url);
    }
}

static void handle_key(struct busto_window *window, const char *key, void *user_data) {
    if (!key) {
        return;
    }

    (void)user_data;
    printf("Key received: '%s'\n", key);
    printf("LastKey received: '%s'\n", lastKey);

    if (busto_input_is_active(g_input)) {
        if (strcmp(key, "Ctrl+V") == 0) {
            busto_window_request_paste(window);
            return;
        }

        busto_input_handle_key(g_input, key);
        sync_urlbar_to_renderer();
        busto_renderer_set_cursor_pos(g_input->cursor_pos);

        busto_renderer_set_url(busto_input_get_url(g_input));
        busto_renderer_set_input_active(1);

        if (strcmp(key, "Return") == 0) {
            const char *url = busto_input_get_url(g_input);
            if (url && strlen(url) > 0) {
                load_url(url);
            }
        }

		else if (strcmp(key, "Escape") == 0) {
            printf("Unfocusing URL bar\n");
            busto_input_deactivate(g_input);
            busto_renderer_set_input_active(0);
        }

        printf("url='%s' len=%zu cursor=%d\n",
           busto_input_get_url(g_input),
           strlen(busto_input_get_url(g_input)),
           g_input->cursor_pos);

        //this dosent render the cursor over white space... cursor dosent move when going left or right.
        /* refresh_display(); */
    }
	else {
        //global key handling when not in input mode
		//TODO: have vim navigation like ctrl+d and u for scrolling, selecting text and all
        if (strcmp(key, "Ctrl+L") == 0) {
            //make url bar active
            printf("Activating URL bar\n");
            busto_input_activate(g_input);
            sync_urlbar_to_renderer();
            busto_renderer_set_cursor_pos(g_input->cursor_pos);
            busto_renderer_set_input_active(1);
			//show cursor
            refresh_display();
        }

		else if (strcmp(key, "q") == 0 && strcmp(lastKey, ":") == 0) {
            printf("Quitting...\n");
            busto_window_destroy(window);
            exit(0);
        }

		else if (strcmp(key, "Up") == 0 || strcmp(key, "k") == 0) {
            busto_renderer_scroll(-50);
            refresh_display();
        }

		else if (strcmp(key, "Down") == 0 || strcmp(key, "j") ==0 ) {
            busto_renderer_scroll(50);
            refresh_display();
        }

		else if (strcmp(key, "r") == 0 || strcmp(key, "F5") == 0) {
            reload_current_page();
        }

		else if (strcmp(key, "?") == 0) {
            busto_renderer_set_content(
                "BUSTO BROWSER HELP\n\n"
                "CONTROLS:\n"
                "  l           - Focus URL bar\n"
                "  q           - Quit browser\n"
                "  r / F5      - Reload current page\n"
                "  j/k Up/Down - Scroll content\n"
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
    snprintf(lastKey, sizeof(lastKey), "%s", key);
}

static void handle_paste(struct busto_window *window, const char *text, void *user_data) {
    (void)window;
    (void)user_data;

    if (!busto_input_is_active(g_input)) {
        return;
    }

    busto_input_insert_text(g_input, text);
    sync_urlbar_to_renderer();
    busto_renderer_set_cursor_pos(g_input->cursor_pos);
    busto_renderer_set_url(busto_input_get_url(g_input));
    busto_renderer_set_input_active(1);
    refresh_display();
}

static void process_fetch_result(void)
{
    char *content = NULL;
    char *title = NULL;
    char *script_source = NULL;
    char *style_source = NULL;
    enum busto_content_mode mode;

    pthread_mutex_lock(&g_fetch_mutex);

    if (!g_pending_result.ready) {
        pthread_mutex_unlock(&g_fetch_mutex);
        return;
    }

    //move ownership out of pending result.
    content = g_pending_result.content;
    title = g_pending_result.title;
    script_source = g_pending_result.script_source;
    style_source = g_pending_result.style_source;
    mode = g_pending_result.mode;

    g_pending_result.content = NULL;
    g_pending_result.title = NULL;
    g_pending_result.script_source = NULL;
    g_pending_result.style_source = NULL;
    g_pending_result.ready = 0;

    pthread_mutex_unlock(&g_fetch_mutex);

    if (g_fetch_thread_active) {
        if (pthread_join(g_fetch_thread, NULL) != 0) {
            perror("pthread_join");
        }
        g_fetch_thread_active = 0;
    }

    g_fetching = 0;

    //this all runs on the main thread
    busto_renderer_set_content_mode(mode);
    busto_renderer_set_content(content);

    printf("Page loaded, title='%s'\n", title ? title : "(none)");

    if (title) {
        char window_title[256];

        snprintf(
            window_title,
            sizeof(window_title),
            "Busto Browser - %s",
            title
        );

        busto_window_set_title(g_window, window_title);
    }

    busto_input_deactivate(g_input);
    busto_renderer_set_input_active(0);
    sync_urlbar_to_renderer();

    refresh_display();

#ifdef BUSTO_ENABLE_SCRIPT
    busto_script_unload();

    if (script_source) {
        busto_script_compile_and_run(script_source, &g_busto_api);
    }
#endif

    free(content);
    free(title);
    free(script_source);
    free(style_source);
}

int main() {
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
    busto_window_set_paste_handler(g_window, handle_paste, NULL);

    busto_window_set_title(g_window, "Busto Browser - Press '?' for help");

	//TODO: set default url
    busto_renderer_set_url("about:blank");
    busto_renderer_set_content(
        "Busto Browser!\n\n"
        "  ? - Show help\n"
        "  l - Focus URL bar\n"
        "  r - Reload page\n"
        "  :q - Quit\n"
        "  j/k Up/Down - Scroll\n\n"
    );

	//do i need this
    refresh_display();


    //main loop
    while (busto_window_is_running(g_window)) {
        process_fetch_result();
        //framerate tick
        busto_window_update_repeats(g_window);
        //wait for events or timeout
        busto_window_poll(g_window, 16);
        //if something happens, redraw also
        if (busto_window_needs_redraw(g_window)) {
            busto_window_redraw(g_window);
        }
    }

    //get rid of threads
    if (g_fetch_thread_active) {
        pthread_join(g_fetch_thread, NULL);
        g_fetch_thread_active = 0;
    }

#ifdef BUSTO_ENABLE_SCRIPT
    busto_script_unload();
#endif

    busto_input_destroy(g_input);
    busto_window_destroy(g_window);
    busto_renderer_free();

    if (g_current_url) {
        free(g_current_url);
    }

    return 0;
}
