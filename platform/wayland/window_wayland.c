#define _GNU_SOURCE
#include "../../include/busto/window.h"
#include "../../include/busto/renderer.h"
#include "../../include/busto/key_repeat.h"
#include <wayland-client.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include "xdg-shell-client-protocol.h"

struct busto_window {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct wl_surface *surface;
    struct xdg_wm_base *xdg_wm_base;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct wl_buffer *buffer;

    struct wl_seat *seat;
    struct wl_keyboard *keyboard;

    size_t shm_size;
    int pending_width;
    int pending_height;
    int needs_resize;

    cairo_surface_t *cairo_surface;
    cairo_t *cr;

    int needs_redraw;

    int width;
    int height;
    int running;

    int configured;
    void *shm_data;

    busto_key_handler_t key_handler;
    void *key_handler_data;

    struct busto_repeat_state repeat;
};

//simple keycode mapping for US keyboard
//entries not listed default to NULL, which behaves like "?" in keycode_to_string
static const char* keymap_simple[256] = {
    [1] = "Escape",
    [2] = "1",
    [3] = "2",
    [4] = "3",
    [5] = "4",
    [6] = "5",
    [7] = "6",
    [8] = "7",
    [9] = "8",
    [10] = "9",
    [11] = "0",
    [12] = "-",
    [13] = "=",
    [14] = "BackSpace",
    [15] = "Tab",
    [16] = "q",
    [17] = "w",
    [18] = "e",
    [19] = "r",
    [20] = "t",
    [21] = "y",
    [22] = "u",
    [23] = "i",
    [24] = "o",
    [25] = "p",
    [26] = "[",
    [27] = "]",
    [28] = "Return",
    [29] = "LeftCtrl",
    [30] = "a",
    [31] = "s",
    [32] = "d",
    [33] = "f",
    [34] = "g",
    [35] = "h",
    [36] = "j",
    [37] = "k",
    [38] = "l",
    [39] = ";",
    [40] = "'",
    [41] = "`",
    [42] = "LeftShift",
    [43] = "\\",
    [44] = "z",
    [45] = "x",
    [46] = "c",
    [47] = "v",
    [48] = "b",
    [49] = "n",
    [50] = "m",
    [51] = ",",
    [52] = ".",
    [53] = "/",
    [54] = "RightShift",
    [56] = "*",
    [57] = " ",
    [58] = "CapsLock",
    [59] = "F1",
    [60] = "F2",
    [61] = "F3",
    [62] = "F4",
    [63] = "F5",
    [64] = "F6",
    [65] = "F7",
    [66] = "F8",
    [67] = "F9",
    [68] = "F10",
    [69] = "NumLock",
    [70] = "ScrollLock",
    [71] = "7",
    [72] = "8",
    [73] = "9",
    [74] = "-",
    [75] = "4",
    [76] = "5",
    [77] = "6",
    [78] = "+",
    [79] = "1",
    [80] = "2",
    [81] = "3",
    [82] = "0",
    [83] = ".",
    [86] = "<",
    [87] = "F11",
    [88] = "F12",
    [102] = "Home",
    [103] = "Up",
    [105] = "Left",
    [106] = "Right",
    [107] = "End",
    [108] = "Down",
    [111] = "Down",
    [187] = "=",
    [188] = ",",
    [189] = "-",
    [190] = ".",
    [191] = "/",
    [192] = "`",
    [215] = "*",
    [219] = "[",
    [220] = "]",
    [221] = "BackSpace",
    [226] = "\\",
    [236] = "Enter",
    [237] = "Right",
};

static void handle_global(void *data, struct wl_registry *registry,
                          uint32_t name, const char *interface,
                          uint32_t version) {
    struct busto_window *window = data;

    if (strcmp(interface, "wl_compositor") == 0) {
        window->compositor =
            wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    } else if (strcmp(interface, "wl_shm") == 0) {
        window->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, "xdg_wm_base") == 0) {
        window->xdg_wm_base =
            wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
    } else if (strcmp(interface, "wl_seat") == 0) {
        window->seat =
            wl_registry_bind(registry, name, &wl_seat_interface, 1);
    }
}

static void handle_global_remove(void *data, struct wl_registry *registry,
                                 uint32_t name) {
    //handle removal if needed
}

static const struct wl_registry_listener registry_listener = {
    handle_global, handle_global_remove};

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base,
                             uint32_t serial) {
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    xdg_wm_base_ping};

static void create_buffer(struct busto_window *window);


static void xdg_surface_configure(void *data,
                                  struct xdg_surface *xdg_surface,
                                  uint32_t serial) {
    struct busto_window *window = data;
    xdg_surface_ack_configure(xdg_surface, serial);

    if (!window->configured) {
        window->configured = 1;

        //adopt pending size if set
        if (window->pending_width > 0) window->width = window->pending_width;
        if (window->pending_height > 0) window->height = window->pending_height;

        create_buffer(window);
        busto_window_redraw(window);
        return;
    }

    if (window->needs_resize) {
        window->needs_resize = 0;

        window->width = window->pending_width;
        window->height = window->pending_height;
        create_buffer(window);
    }

    busto_window_redraw(window);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    xdg_surface_configure};

static void xdg_toplevel_configure(void *data,
                                   struct xdg_toplevel *xdg_toplevel,
                                   int32_t w, int32_t h,
                                   struct wl_array *states) {
    struct busto_window *window = data;

    //some compositors send 0,0 to mean "unspecified"
    if (w > 0 && h > 0) {
        window->pending_width = w;
        window->pending_height = h;

        if (window->pending_width != window->width ||
            window->pending_height != window->height) {
            window->needs_resize = 1;
        }
    }
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    struct busto_window *window = data;
    window->running = 0;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    xdg_toplevel_configure, xdg_toplevel_close, NULL, NULL};

static void keyboard_keymap(void *data, struct wl_keyboard *keyboard,
                           uint32_t format, int fd, uint32_t size) {
    printf("Keyboard keymap received\n");
}

static void keyboard_enter(void *data, struct wl_keyboard *keyboard,
                          uint32_t serial, struct wl_surface *surface,
                          struct wl_array *keys) {
    printf("Keyboard entered surface\n");
}

static void keyboard_leave(void *data, struct wl_keyboard *keyboard,
                          uint32_t serial, struct wl_surface *surface) {
    printf("Keyboard left surface\n");
}

static int is_ctrl_down(const struct busto_window *window) {
    return window->repeat.key_down[29] || window->repeat.key_down[97]; // LeftCtrl, RightCtrl
}

static int is_shift_down(const struct busto_window *window) {
    return window->repeat.key_down[42] || window->repeat.key_down[54]; // LeftShift, RightShift
}

static const char *keycode_to_string(struct busto_window *window, int key) {
    static char buf[32];

    if (key < 0 || key >= 256) return NULL;

    if (keymap_simple[key] && strcmp(keymap_simple[key], "?") != 0) {
        const char *base = keymap_simple[key];

        //dont fuck with modifiers in output or else it gets eaten
        if (strcmp(base, "LeftShift") == 0 || strcmp(base, "RightShift") == 0 ||
            strcmp(base, "LeftCtrl") == 0  || strcmp(base, "RightCtrl") == 0 ||
            strcmp(base, "LeftAlt") == 0   || strcmp(base, "RightAlt") == 0) {
            return NULL;
        }

        int shift = is_shift_down(window);

        //for 1char keys apply shift map
        if (strlen(base) == 1) {
            char c = base[0];

            if (shift) {
                if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
                else {
                    switch (c) {
                        case '1': c = '!'; break;
                        case '2': c = '@'; break;
                        case '3': c = '#'; break;
                        case '4': c = '$'; break;
                        case '5': c = '%'; break;
                        case '6': c = '^'; break;
                        case '7': c = '&'; break;
                        case '8': c = '*'; break;
                        case '9': c = '('; break;
                        case '0': c = ')'; break;
                        case '-': c = '_'; break;
                        case '=': c = '+'; break;
                        case '[': c = '{'; break;
                        case ']': c = '}'; break;
                        case ';': c = ':'; break;
                        case '\'': c = '"'; break;
                        case '`': c = '~'; break;
                        case '\\': c = '|'; break;
                        case ',': c = '<'; break;
                        case '.': c = '>'; break;
                        case '/': c = '?'; break;
                        default: break;
                    }
                }
            }

            //for ctrl+key combos
            if (is_ctrl_down(window) && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) {
                snprintf(buf, sizeof(buf), "Ctrl+%c",
                         (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c);
                return buf;
            }

            buf[0] = c;
            buf[1] = '\0';
            return buf;
        }

        return base;
    }

    if (key == 36)  return "Return";
    if (key == 111) return "Up";
    if (key == 116) return "Down";
    if (key == 113) return "Left";
    if (key == 114) return "Right";
    if (key == 9)   return "Escape";
    if (key == 118) return "F5";

    return NULL;
}

static void keyboard_key(void *data, struct wl_keyboard *keyboard,
                        uint32_t serial, uint32_t time, uint32_t key,
                        uint32_t state) {
    struct busto_window *window = data;


    if(key >= 256) return;
    if(state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        window->repeat.key_down[key]=1;
        const char *key_str = keycode_to_string(window, key);
        //TODO: maybe remove this cause tis a lot
        printf("[key] %s key=%u '%s'\n",
           state == WL_KEYBOARD_KEY_STATE_PRESSED ? "down" : "up",
           key, key_str ? key_str : "(null)");

        if(window->key_handler){
            window->key_handler(window,key_str, window->key_handler_data);
        }

        if(window->repeat.rate >0) {
            long long t = busto_now_ms();
            window->repeat.key_next_repeat_ms[key] = t+window->repeat.delay;
        }
        else {
            window->repeat.key_next_repeat_ms[key] = 0;
        }
    }
    else if(state==WL_KEYBOARD_KEY_STATE_RELEASED) {
        window->repeat.key_down[key] =0;
        window->repeat.key_next_repeat_ms[key]=0;
    }

}

static void keyboard_modifiers(void *data, struct wl_keyboard *keyboard,
                              uint32_t serial, uint32_t mods_depressed,
                              uint32_t mods_latched, uint32_t mods_locked,
                              uint32_t group) {
    printf("Modifiers: depressed=%u, latched=%u, locked=%u, group=%u\n",
           mods_depressed, mods_latched, mods_locked, group);
}

static void keyboard_repeat_info(void *data, struct wl_keyboard *keyboard,
                                 int32_t rate, int32_t delay) {
    struct busto_window *window = data;
    window->repeat.rate = rate;
    window->repeat.delay = delay;
    printf("Keyboard repeat: rate=%d, delay=%d\n", rate, delay);
}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_keymap,
    keyboard_enter,
    keyboard_leave,
    keyboard_key,
    keyboard_modifiers,
    keyboard_repeat_info
};

static void destroy_buffer(struct busto_window *window) {
    if (window->cr) { cairo_destroy(window->cr); window->cr = NULL; }
    if (window->cairo_surface) { cairo_surface_destroy(window->cairo_surface); window->cairo_surface = NULL; }
    if (window->buffer) { wl_buffer_destroy(window->buffer); window->buffer = NULL; }

    if (window->shm_data) {
        //need to store shm_size in the window
        munmap(window->shm_data, window->shm_size);
        window->shm_data = NULL;
        window->shm_size = 0;
    }
}


static void create_buffer(struct busto_window *window) {
    //i got to free memory when we create buffer i thikn?
    destroy_buffer(window);
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, window->width);
    int size = stride * window->height;
    window->shm_size = (size_t)size;

    //create temporary file for shared memory
    char shm_name[] = "/tmp/busto-browser-XXXXXX";
    int fd = mkstemp(shm_name);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "Failed to create temporary file\n");
        exit(1);
    }
    unlink(shm_name);

    //set size
    if (ftruncate(fd, size) < 0) {
        fprintf(stderr, "Failed to truncate file\n");
        exit(1);
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(window->shm, fd, size);
    if (!pool) {
        fprintf(stderr, "Failed to create SHM pool\n");
        close(fd);
        exit(1);
    }

    window->buffer = wl_shm_pool_create_buffer(pool, 0, window->width, window->height, stride, WL_SHM_FORMAT_ARGB8888);
    if (!window->buffer) {
        fprintf(stderr, "Failed to create Wayland buffer\n");
        wl_shm_pool_destroy(pool);
        close(fd);
        exit(1);
    }


    window->shm_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (window->shm_data == MAP_FAILED) {
        fprintf(stderr, "Failed to mmap\n");
        exit(1);
    }

    window->cairo_surface = cairo_image_surface_create_for_data(
        window->shm_data, CAIRO_FORMAT_ARGB32, window->width, window->height, stride);
    window->cr = cairo_create(window->cairo_surface);

    wl_shm_pool_destroy(pool);
    close(fd);
}

struct busto_window *busto_window_create(int width, int height) {
    struct busto_window *window = calloc(1, sizeof(struct busto_window));
    if (!window) return NULL;

    window->width = width;
    window->height = height;
    window->running = 1;
    window->repeat.delay = 300;
    window->repeat.rate = 25;
    window->pending_width = width;
    window->pending_height = height;
    window->needs_resize = 0;


    window->display = wl_display_connect(NULL);
    if (!window->display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        free(window);
        return NULL;
    }


    window->registry = wl_display_get_registry(window->display);
    wl_registry_add_listener(window->registry, &registry_listener, window);
    wl_display_roundtrip(window->display);

    if (!window->compositor || !window->shm || !window->xdg_wm_base) {
        fprintf(stderr, "Missing Wayland interfaces:\n");
        fprintf(stderr, "  compositor: %s\n", window->compositor ? "OK" : "MISSING");
        fprintf(stderr, "  shm: %s\n", window->shm ? "OK" : "MISSING");
        fprintf(stderr, "  xdg_wm_base: %s\n", window->xdg_wm_base ? "OK" : "MISSING");
        fprintf(stderr, "  seat: %s\n", window->seat ? "OK" : "MISSING");
        busto_window_destroy(window);
        return NULL;
    }

    window->surface = wl_compositor_create_surface(window->compositor);
    window->xdg_surface = xdg_wm_base_get_xdg_surface(window->xdg_wm_base, window->surface);
    window->xdg_toplevel = xdg_surface_get_toplevel(window->xdg_surface);

    xdg_wm_base_add_listener(window->xdg_wm_base, &xdg_wm_base_listener, NULL);
    xdg_surface_add_listener(window->xdg_surface, &xdg_surface_listener, window);
    xdg_toplevel_add_listener(window->xdg_toplevel, &xdg_toplevel_listener, window);

    //set up keyboard if seat is available
    if (window->seat) {
        window->keyboard = wl_seat_get_keyboard(window->seat);
        if (!window->keyboard) {
            fprintf(stderr, "Failed to get keyboard interface\n");
        } else {
            wl_keyboard_add_listener(window->keyboard, &keyboard_listener, window);
        }
    } else {
        printf("No seat interface available - no keyboard input\n");
    }

    //we dont need this here anymore since its done after configuration
    //create_buffer(window);
    //without this, it becomes UB due to illegal gentoo linux laws fml
    xdg_toplevel_set_title(window->xdg_toplevel, "Busto Browser");
    wl_surface_commit(window->surface);
    wl_display_roundtrip(window->display);

    return window;
}

void busto_window_destroy(struct busto_window *window) {
    if (!window) return;

    if (window->keyboard) wl_keyboard_destroy(window->keyboard);
    if (window->seat) wl_seat_destroy(window->seat);

    destroy_buffer(window);

    if (window->xdg_toplevel) xdg_toplevel_destroy(window->xdg_toplevel);
    if (window->xdg_surface) xdg_surface_destroy(window->xdg_surface);
    if (window->surface) wl_surface_destroy(window->surface);
    if (window->xdg_wm_base) xdg_wm_base_destroy(window->xdg_wm_base);
    if (window->shm) wl_shm_destroy(window->shm);
    if (window->compositor) wl_compositor_destroy(window->compositor);
    if (window->registry) wl_registry_destroy(window->registry);
    if (window->display) wl_display_disconnect(window->display);

    free(window);
}

void busto_window_set_title(struct busto_window *window, const char *title) {
    if (window && window->xdg_toplevel && title) {
        xdg_toplevel_set_title(window->xdg_toplevel, title);
    }
}

int busto_window_is_running(struct busto_window *window) {
    return window ? window->running : 0;
}

void busto_window_dispatch(struct busto_window *window) {
    if (!window) return;
    int rc = wl_display_dispatch(window->display);
    if(rc < 0) {
        fprintf(stderr, "wl_display_dispatch failed : %s\n", strerror(errno));
        window->running = 0;
    }
}

void busto_window_poll(struct busto_window *window, int timeout_ms) {
    if (!window) return;

    wl_display_dispatch_pending(window->display);
    while (wl_display_prepare_read(window->display) != 0) {
        wl_display_dispatch_pending(window->display);
    }
    wl_display_flush(window->display);

    int fd = wl_display_get_fd(window->display);

    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN
    };

    int ret = poll(&pfd, 1, timeout_ms);
    if (ret > 0 && (pfd.revents & POLLIN)) {
        wl_display_read_events(window->display);
        wl_display_dispatch_pending(window->display);
    }
    else {
        wl_display_cancel_read(window->display);
    }
}

void busto_window_redraw(struct busto_window *window) {
    if (!window) {
        fprintf(stderr, "Invalid window in redraw\n");
        return;
    }
    if (!window->configured || !window->buffer) return;

    busto_renderer_render(window->cr, window->width, window->height);
    wl_surface_attach(window->surface, window->buffer, 0, 0);
    wl_surface_damage(window->surface, 0, 0, window->width, window->height);
    wl_surface_commit(window->surface);
}

void busto_window_set_key_handler(struct busto_window *window, busto_key_handler_t handler, void *data) {
    if (window) {
        window->key_handler = handler;
        window->key_handler_data = data;
    }
}

void busto_window_request_redraw(struct busto_window *window) {
    if (window) window->needs_redraw = 1;
}

int busto_window_needs_redraw(struct busto_window *window) {
    if (!window) return 0;
    int needs = window->needs_redraw;
    window->needs_redraw = 0;
    return needs;
}

void busto_window_update_repeats(struct busto_window *window) {
    if (!window) return;
    busto_repeat_update(
        &window->repeat,
        window->key_handler,
        window->key_handler_data,
        window,
        keycode_to_string
    );
}
