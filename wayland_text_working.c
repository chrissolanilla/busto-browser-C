#define _GNU_SOURCE
#include "xdg-shell-client-protocol.h"
#include <cairo/cairo.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

struct wl_display *display;
struct wl_registry *registry;
struct wl_compositor *compositor;
struct wl_shm *shm;
struct wl_surface *surface;
struct xdg_wm_base *xdg_wm_base;
struct xdg_surface *xdg_surface;
struct xdg_toplevel *xdg_toplevel;
struct wl_buffer *buffer;

cairo_surface_t *cairo_surface;
cairo_t *cr;

int width = 1920;
int height = 1080;
int running = 1;
void *shm_data;

void render_text() {
    // Clear surface with white background
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_paint(cr);

    // Set up text rendering
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0); // Black text

    // Select a font
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 24.0);

    // Draw text
    cairo_move_to(cr, 50, 100);
    cairo_show_text(cr, "Hello, Wayland! AAAA");

    cairo_move_to(cr, 50, 50);
    cairo_set_font_size(cr, 18.0);
    cairo_show_text(cr, "This is text rendered in C");

    cairo_move_to(cr, 50, 140);
    cairo_show_text(cr, "using Wayland protocols");
}

static void handle_global(void *data, struct wl_registry *registry,
                          uint32_t name, const char *interface,
                          uint32_t version) {
    if (strcmp(interface, "wl_compositor") == 0) {
        compositor =
            wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    } else if (strcmp(interface, "wl_shm") == 0) {
        shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, "xdg_wm_base") == 0) {
        xdg_wm_base =
            wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
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

static void redraw();

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
                                  uint32_t serial) {
    xdg_surface_ack_configure(xdg_surface, serial);
    redraw();
}

static const struct xdg_surface_listener xdg_surface_listener = {
    xdg_surface_configure};

static void xdg_toplevel_configure(void *data,
                                   struct xdg_toplevel *xdg_toplevel, int32_t w,
                                   int32_t h, struct wl_array *states) {
    if (w > 0)
        width = w;
    if (h > 0)
        height = h;
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    running = 0;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    xdg_toplevel_configure, xdg_toplevel_close};

static void create_buffer() {
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
    int size = stride * height;

    // Create temporary file for shared memory
    char shm_name[] = "/tmp/wayland-text-app-XXXXXX";
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

    // Create shared memory pool
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride,
                                       0); // ARGB32 format

    // Map memory
    shm_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_data == MAP_FAILED) {
        fprintf(stderr, "Failed to mmap\n");
        exit(1);
    }

    // Create Cairo surface
    cairo_surface = cairo_image_surface_create_for_data(
        shm_data, CAIRO_FORMAT_ARGB32, width, height, stride);
    cr = cairo_create(cairo_surface);

    wl_shm_pool_destroy(pool);
    close(fd);
}

static void redraw() {
    render_text();
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage(surface, 0, 0, width, height);
    wl_surface_commit(surface);
}

int main(int argc, char *argv[]) {
    display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return 1;
    }

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (!compositor || !shm || !xdg_wm_base) {
        fprintf(stderr, "Missing Wayland interfaces\n");
        fprintf(stderr, "Compositor: %s\n",
                compositor ? "available" : "missing");
        fprintf(stderr, "SHM: %s\n", shm ? "available" : "missing");
        fprintf(stderr, "XDG WM Base: %s\n",
                xdg_wm_base ? "available" : "missing");
        return 1;
    }

    surface = wl_compositor_create_surface(compositor);
    xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
    xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);

    xdg_wm_base_add_listener(xdg_wm_base, &xdg_wm_base_listener, NULL);
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
    xdg_toplevel_add_listener(xdg_toplevel, &xdg_toplevel_listener, NULL);

    xdg_toplevel_set_title(xdg_toplevel, "Wayland Text Renderer");

    create_buffer();
    redraw();

    printf("Window created. Press Ctrl+C to exit.\n");

    // Main loop
    while (running && wl_display_dispatch(display) != -1) {
        // Event handling
    }

    // Cleanup
    cairo_destroy(cr);
    cairo_surface_destroy(cairo_surface);
    wl_buffer_destroy(buffer);
    xdg_toplevel_destroy(xdg_toplevel);
    xdg_surface_destroy(xdg_surface);
    wl_surface_destroy(surface);
    xdg_wm_base_destroy(xdg_wm_base);
    wl_shm_destroy(shm);
    wl_compositor_destroy(compositor);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);

    return 0;
}
