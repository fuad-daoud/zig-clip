/**
 * Wayland Client with Clipboard Monitoring
 * 
 * This example creates a Wayland window and listens for clipboard (selection) events.
 * Build with: gcc -o clipboard-monitor clipboard-monitor.c xdg-shell.c -lwayland-client -lrt
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
    
    // Clipboard-related objects
    struct wl_seat *seat;
    struct wl_data_device_manager *data_device_manager;
    struct wl_data_device *data_device;
    struct wl_data_offer *current_offer;
    
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

// Clipboard data offer listeners
static void
data_offer_offer(void *data, struct wl_data_offer *offer, const char *mime_type)
{
    printf("Data offer with MIME type: %s\n", mime_type);
}

static const struct wl_data_offer_listener data_offer_listener = {
    .offer = data_offer_offer
};

// Handle clipboard text content
static void
receive_clipboard_data(struct client_state *state, const char *mime_type)
{
    if (!state->current_offer) {
        return;
    }
    
    // Create pipes for reading data
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return;
    }
    
    // Send the request to read the data
    wl_data_offer_receive(state->current_offer, mime_type, pipefd[1]);
    close(pipefd[1]); // Close write end immediately after request
    
    // Dispatch events to ensure the data is sent through the pipe
    wl_display_flush(state->display);
    
    // Read the data from the pipe
    char buffer[4096];
    ssize_t bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1);
    close(pipefd[0]);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("*** Clipboard content ***\n%s\n********************\n", buffer);
    } else if (bytes_read == 0) {
        printf("Empty clipboard data\n");
    } else {
        perror("read");
    }
}

// Data device listeners
static void
data_device_data_offer(void *data, struct wl_data_device *data_device,
                      struct wl_data_offer *offer)
{
    struct client_state *state = data;
    
    printf("New data offer received\n");
    
    // Set up listener for the data offer
    wl_data_offer_add_listener(offer, &data_offer_listener, state);
}

static void
data_device_selection(void *data, struct wl_data_device *data_device,
                     struct wl_data_offer *offer)
{
    struct client_state *state = data;
    
    printf("Selection changed\n");
    
    // Update current offer
    state->current_offer = offer;
    
    if (offer) {
        // Try to receive text data
        receive_clipboard_data(state, "text/plain;charset=utf-8");
    }
}

static const struct wl_data_device_listener data_device_listener = {
    .data_offer = data_device_data_offer,
    .enter = NULL,
    .leave = NULL,
    .motion = NULL,
    .drop = NULL,
    .selection = data_device_selection
};

// Seat listener
static void
seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities)
{
    // Not used for clipboard monitoring
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = NULL
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
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        state->seat = wl_registry_bind(registry, id, &wl_seat_interface, 1);
        wl_seat_add_listener(state->seat, &seat_listener, state);
        printf("Found seat\n");
    } else if (strcmp(interface, wl_data_device_manager_interface.name) == 0) {
        state->data_device_manager = wl_registry_bind(
            registry, id, &wl_data_device_manager_interface, 1);
        printf("Found data_device_manager\n");
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

    // Check for required interfaces
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
    
    // Set up the data device (for clipboard monitoring)
    if (state.seat && state.data_device_manager) {
        state.data_device = wl_data_device_manager_get_data_device(
            state.data_device_manager, state.seat);
        wl_data_device_add_listener(state.data_device, &data_device_listener, &state);
        printf("Set up data device for clipboard monitoring\n");
    } else {
        fprintf(stderr, "Clipboard monitoring not available\n");
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
    xdg_toplevel_set_title(state.xdg_toplevel, "Wayland Clipboard Monitor");
    xdg_toplevel_set_min_size(state.xdg_toplevel, 200, 100);
    // xdg_toplevel_set_fullscreen(state.xdg_toplevel, NULL); // Optional fullscreen mode
    
    // Commit the surface to tell the compositor we're ready
    wl_surface_commit(state.surface);

    printf("Window created, entering main loop\n");
    printf("Monitoring clipboard events. Copy text in another application to see it appear here.\n");

    // Main loop
    while (state.running && wl_display_dispatch(state.display) != -1) {
        // Event handling is done in the dispatch function above
    }

    // Clean up
    if (state.data_device)
        wl_data_device_destroy(state.data_device);
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
    if (state.data_device_manager)
        wl_data_device_manager_destroy(state.data_device_manager);
    if (state.seat)
        wl_seat_destroy(state.seat);
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
