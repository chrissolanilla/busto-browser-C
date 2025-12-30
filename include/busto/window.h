#ifndef BUSTO_WINDOW_H
#define BUSTO_WINDOW_H

#include <cairo/cairo.h>
#include <wayland-client.h>
#include "../../build/protocol/xdg-shell-client-protocol.h"

struct busto_window;

typedef void (*busto_key_handler_t)(struct busto_window *window, const char *key, void *user_data);

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

    cairo_surface_t *cairo_surface;
    cairo_t *cr;

    int width;
    int height;
    int running;

    //for it to work in gentoo
    int configured;
    void *shm_data;

    busto_key_handler_t key_handler;
    void *key_handler_data;
};

struct busto_window *busto_window_create(int width, int height);
void busto_window_destroy(struct busto_window *window);
void busto_window_set_title(struct busto_window *window, const char *title);
int busto_window_is_running(struct busto_window *window);
void busto_window_dispatch(struct busto_window *window);
void busto_window_redraw(struct busto_window *window);
void busto_window_set_key_handler(struct busto_window *window, busto_key_handler_t handler, void *data);

#endif
