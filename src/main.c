/**
 * Simple Wayland Client using XDG Shell with visible content
 * 
 * This example demonstrates a basic Wayland client that creates a fullscreen window using xdg-shell protocol.
 * Build with: gcc -o main.out src/main.c src/xdg-shell.c src/font8x8.c -lwayland-client -lrt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <wayland-client.h>
#include "xdg-shell.h"

#define WIDTH 1000
#define HEIGHT 1000

struct client_state {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct xdg_wm_base *xdg_wm_base;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct wl_shm *shm;
    struct wl_buffer *buffer;
    bool running;
    int width, height;
};

// Create a shared memory file and return its file descriptor
static int 
create_shm_file(size_t size) {
    static int counter = 0;
    char name[256];
    
    snprintf(name, sizeof(name), "/wl_shm-%d-%d", getpid(), counter++);
    
    int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0) {
        fprintf(stderr, "shm_open failed: %s\n", strerror(errno));
        return -1;
    }
    
    // Immediately unlink the file so it's removed when the process exits
    shm_unlink(name);
    
    int ret;
    do {
        ret = ftruncate(fd, size);
    } while (ret < 0 && errno == EINTR);

    if (ret < 0) {
        fprintf(stderr, "ftruncate failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

// Create a buffer with a solid color
static struct wl_buffer *
create_buffer(struct client_state *state, int width, int height) {
    int stride = width * 4; // 4 bytes per pixel (ARGB8888)
    int size = stride * height;

    int fd = create_shm_file(size);
    if (fd < 0) {
        return NULL;
    }

    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        close(fd);
        return NULL;
    }

    // Fill with solid purple color (ARGB8888)
    uint32_t *pixels = data;
    uint32_t bg_color = 0xFFAA00FF; // Purple background color
    
    for (int i = 0; i < width * height; i++) {
        pixels[i] = bg_color;
    }
    
    printf("Created solid color buffer %dx%d\n", width, height);

    struct wl_shm_pool *pool = wl_shm_create_pool(state->shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height,
                                                      stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    munmap(data, size);

    return buffer;
}

// XDG WM Base listener
static void
xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
    printf("Received ping from Wayland compositor\n");
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

// Registry listener callbacks
static void
registry_handle_global(void *data, struct wl_registry *registry,
                      uint32_t id, const char *interface, uint32_t version)
{
    struct client_state *state = data;

    printf("Got interface: %s (version %d)\n", interface, version);

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->compositor = wl_registry_bind(
            registry, id, &wl_compositor_interface, 1);
        printf("Found compositor\n");
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        state->xdg_wm_base = wl_registry_bind(
            registry, id, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(state->xdg_wm_base, 
                               &xdg_wm_base_listener, state);
        printf("Found xdg_wm_base\n");
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
        printf("Found wl_shm\n");
    }
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
                             uint32_t name)
{
    // This space intentionally left blank
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove
};

// XDG surface listener
static void
xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
    struct client_state *state = data;
    
    xdg_surface_ack_configure(xdg_surface, serial);
    printf("Surface configured, serial: %d\n", serial);
    
    // Create a buffer and attach it to the surface
    if (!state->buffer) {
        state->buffer = create_buffer(state, state->width, state->height);
        if (!state->buffer) {
            fprintf(stderr, "Failed to create buffer\n");
            state->running = false;
            return;
        }
        
        printf("Created buffer %dx%d\n", state->width, state->height);
    }
    
    wl_surface_attach(state->surface, state->buffer, 0, 0);
    wl_surface_damage(state->surface, 0, 0, state->width, state->height);
    wl_surface_commit(state->surface);
    printf("Attached buffer to surface\n");
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

// XDG toplevel listener
static void
xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
                      int32_t width, int32_t height,
                      struct wl_array *states)
{
    struct client_state *state = data;
    
    if (width > 0 && height > 0) {
        state->width = width;
        state->height = height;
    } else {
        state->width = WIDTH;
        state->height = HEIGHT;
    }
    
    printf("Toplevel configured: width=%d, height=%d\n", width, height);
}

static void
xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
    struct client_state *state = data;
    printf("Close requested by compositor\n");
    state->running = false;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure,
    .close = xdg_toplevel_close,
};

int
main(int argc, char **argv)
{
    struct client_state state = { 0 };
    state.running = true;
    state.width = WIDTH;
    state.height = HEIGHT;

    // Connect to the Wayland display
    state.display = wl_display_connect(NULL);
    if (!state.display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return 1;
    }
    
    printf("Connected to Wayland display\n");

    // Get the registry
    state.registry = wl_display_get_registry(state.display);
    wl_registry_add_listener(state.registry, &registry_listener, &state);

    // Wait for the server to process the registry events
    wl_display_roundtrip(state.display);
    wl_display_roundtrip(state.display); // Sometimes need an extra roundtrip

    if (!state.compositor) {
        fprintf(stderr, "Could not find compositor\n");
        return 1;
    }
    
    if (!state.xdg_wm_base) {
        fprintf(stderr, "Could not find xdg_wm_base\n");
        return 1;
    }
    
    if (!state.shm) {
        fprintf(stderr, "Could not find wl_shm\n");
        return 1;
    }

    // Create a surface
    state.surface = wl_compositor_create_surface(state.compositor);
    if (!state.surface) {
        fprintf(stderr, "Failed to create surface\n");
        return 1;
    }

    // Create an XDG surface
    state.xdg_surface = xdg_wm_base_get_xdg_surface(state.xdg_wm_base, state.surface);
    if (!state.xdg_surface) {
        fprintf(stderr, "Failed to get xdg surface\n");
        return 1;
    }
    xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, &state);

    // Create an XDG toplevel
    state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
    if (!state.xdg_toplevel) {
        fprintf(stderr, "Failed to get xdg toplevel\n");
        return 1;
    }
    xdg_toplevel_add_listener(state.xdg_toplevel, &xdg_toplevel_listener, &state);

    // Configure the toplevel window
    xdg_toplevel_set_title(state.xdg_toplevel, "Simple Wayland XDG Client");
    xdg_toplevel_set_min_size(state.xdg_toplevel, 200, 100);
    xdg_toplevel_set_fullscreen(state.xdg_toplevel, NULL); // Request fullscreen mode
    
    // Commit the surface to tell the compositor we're ready
    wl_surface_commit(state.surface);

    printf("Window created, entering main loop\n");

    // Main loop
    while (state.running && wl_display_dispatch(state.display) != -1) {
        // In a real application, you would handle events and drawing here
    }

    // Clean up
    if (state.buffer)
        wl_buffer_destroy(state.buffer);
    if (state.xdg_toplevel)
        xdg_toplevel_destroy(state.xdg_toplevel);
    if (state.xdg_surface)
        xdg_surface_destroy(state.xdg_surface);
    if (state.surface)
        wl_surface_destroy(state.surface);
    if (state.xdg_wm_base)
        xdg_wm_base_destroy(state.xdg_wm_base);
    if (state.shm)
        wl_shm_destroy(state.shm);
    if (state.compositor)
        wl_compositor_destroy(state.compositor);
    if (state.registry)
        wl_registry_destroy(state.registry);
    if (state.display)
        wl_display_disconnect(state.display);

    return 0;
}
