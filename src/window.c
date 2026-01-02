#define _GNU_SOURCE
#include "../include/busto/window.h"
#include "../include/busto/renderer.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

// Simple keycode mapping for US keyboard
static const char* keymap_simple[256] = {
    [0] = "?",
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
    [29] = "Left",
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
    [42] = "Left",
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
    [54] = "Right",
    [55] = "?",
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
    [84] = "?",
    [85] = "?",
    [86] = "<",
    [87] = "F11",
    [88] = "F12",
    [89] = "?",
    [90] = "?",
    [91] = "?",
    [92] = "?",
    [93] = "?",
    [94] = "?",
    [95] = "?",
    [96] = "?",
    [97] = "?",
    [98] = "?",
    [99] = "?",
    [100] = "?",
    [101] = "?",
    [102] = "Home",
    [103] = "Up",
    [104] = "?",
    [105] = "Left",
    [106] = "?",
    [107] = "End",
    [108] = "?",
    [109] = "?",
    [110] = "?",
    [111] = "Down",
    [112] = "?",
    [113] = "?",
    [114] = "?",
    [115] = "?",
    [116] = "?",
    [117] = "?",
    [118] = "?",
    [119] = "?",
    [120] = "?",
    [121] = "?",
    [122] = "?",
    [123] = "?",
    [124] = "?",
    [125] = "?",
    [126] = "?",
    [127] = "?",
    [128] = "?",
    [129] = "?",
    [130] = "?",
    [131] = "?",
    [132] = "?",
    [133] = "?",
    [134] = "?",
    [135] = "?",
    [136] = "?",
    [137] = "?",
    [138] = "?",
    [139] = "?",
    [140] = "?",
    [141] = "?",
    [142] = "?",
    [143] = "?",
    [144] = "?",
    [145] = "?",
    [146] = "?",
    [147] = "?",
    [148] = "?",
    [149] = "?",
    [150] = "?",
    [151] = "?",
    [152] = "?",
    [153] = "?",
    [154] = "?",
    [155] = "?",
    [156] = "?",
    [157] = "?",
    [158] = "?",
    [159] = "?",
    [160] = "?",
    [161] = "?",
    [162] = "?",
    [163] = "?",
    [164] = "?",
    [165] = "?",
    [166] = "?",
    [167] = "?",
    [168] = "?",
    [169] = "?",
    [170] = "?",
    [171] = "?",
    [172] = "?",
    [173] = "?",
    [174] = "?",
    [175] = "?",
    [176] = "?",
    [177] = "?",
    [178] = "?",
    [179] = "?",
    [180] = "?",
    [181] = "?",
    [182] = "?",
    [183] = "?",
    [184] = "?",
    [185] = "?",
    [186] = "?",
    [187] = "=",
    [188] = ",",
    [189] = "-",
    [190] = ".",
    [191] = "/",
    [192] = "`",
    [193] = "?",
    [194] = "?",
    [195] = "?",
    [196] = "?",
    [197] = "?",
    [198] = "?",
    [199] = "?",
    [200] = "?",
    [201] = "?",
    [202] = "?",
    [203] = "?",
    [204] = "?",
    [205] = "?",
    [206] = "?",
    [207] = "?",
    [208] = "?",
    [209] = "?",
    [210] = "?",
    [211] = "?",
    [212] = "?",
    [213] = "?",
    [214] = "?",
    [215] = "*",
    [216] = "?",
    [217] = "?",
    [218] = "?",
    [219] = "[",
    [220] = "]",
    [221] = "BackSpace",
    [222] = "?",
    [223] = "?",
    [224] = "?",
    [225] = "?",
    [226] = "\\",
    [227] = "?",
    [228] = "?",
    [229] = "?",
    [230] = "?",
    [231] = "?",
    [232] = "?",
    [233] = "?",
    [234] = "?",
    [235] = "?",
    [236] = "Enter",
    [237] = "Right",
    [238] = "?",
    [239] = "?",
    [240] = "?",
    [241] = "?",
    [242] = "?",
    [243] = "?",
    [244] = "?",
    [245] = "?",
    [246] = "?",
    [247] = "?",
    [248] = "?",
    [249] = "?",
    [250] = "?",
    [251] = "?",
    [252] = "?",
    [253] = "?",
    [254] = "?",
    [255] = "?",
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
    // Handle removal if needed
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


static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
                                   uint32_t serial) {
    struct busto_window *window = data;
    xdg_surface_ack_configure(xdg_surface, serial);

    //only create the buffer and draw after we configure first
    if(!window->configured) {
        window->configured = 1;
        create_buffer(window);
    }

    busto_window_redraw(window);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    xdg_surface_configure};

static void xdg_toplevel_configure(void *data,
                                   struct xdg_toplevel *xdg_toplevel, int32_t w,
                                   int32_t h, struct wl_array *states) {
    struct busto_window *window = data;
    if (w > 0)
        window->width = w;
    if (h > 0)
        window->height = h;
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

static void keyboard_key(void *data, struct wl_keyboard *keyboard,
                        uint32_t serial, uint32_t time, uint32_t key,
                        uint32_t state) {
    struct busto_window *window = data;

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED && window->key_handler) {
        const char *key_str = NULL;

        if (key < 256 && keymap_simple[key] && strcmp(keymap_simple[key], "?") != 0) {
            key_str = keymap_simple[key];
        } else if (key == 22) {
            key_str = "BackSpace";
        } else if (key == 36) {
            key_str = "Return";
        } else if (key == 111) {
            key_str = "Up";
        } else if (key == 116) {
            key_str = "Down";
        } else if (key == 113) {
            key_str = "Left";
        } else if (key == 114) {
            key_str = "Right";
        } else if (key == 108) {
            key_str = "Down";
        } else if (key == 105) {
            key_str = "Left";
        } else if (key == 106) {
            key_str = "Right";
        } else if (key == 23) {
            key_str = "Tab";
        } else if (key == 9) {
            key_str = "Escape";
        } else if (key == 118) {
            key_str = "F5";
        } else {
            printf("unknown keycode: %u\n", key);
            return;
        }

        printf("Key: %u -> '%s'\n", key, key_str);
        window->key_handler(window, key_str, window->key_handler_data);
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

static void create_buffer(struct busto_window *window) {
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, window->width);
    int size = stride * window->height;

    // Create temporary file for shared memory
    char shm_name[] = "/tmp/busto-browser-XXXXXX";
    int fd = mkstemp(shm_name);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "Failed to create temporary file\n");
        exit(1);
    }
    unlink(shm_name);

    // Set size
    if (ftruncate(fd, size) < 0) {
        fprintf(stderr, "Failed to truncate file\n");
        exit(1);
    }

    //create shared memory pool
    struct wl_shm_pool *pool = wl_shm_create_pool(window->shm, fd, size);
    if (!pool) {
        fprintf(stderr, "Failed to create SHM pool\n");
        close(fd);
        exit(1);
    }

    //assign it to window not just no where lmao
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

    // Set up keyboard if seat is available
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
    if (window->cr) cairo_destroy(window->cr);
    if (window->cairo_surface) cairo_surface_destroy(window->cairo_surface);
    if (window->buffer) wl_buffer_destroy(window->buffer);
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

    /* { */
    /*     wl_display_dispatch(window->display); */
    /* } */
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
