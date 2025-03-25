/**
 * Wayland Clipboard Monitor using wlr-data-control protocol
 * 
 * This monitors clipboard events using the wlr-data-control protocol, which works 
 * in wlroots-based compositors like Sway, Wayfire, etc.
 * 
 * Build with:
 * 1. First generate protocol headers:
 *    wayland-scanner private-code < /path/to/wlr-data-control-unstable-v1.xml > wlr-data-control-protocol.c
 *    wayland-scanner client-header < /path/to/wlr-data-control-unstable-v1.xml > wlr-data-control-unstable-v1.h
 * 
 * 2. Then compile:
 *    gcc -o wlr-clipboard-monitor wlr-clipboard-monitor.c wlr-data-control-protocol.c -lwayland-client
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <wayland-client.h>

// Include the wlr-data-control protocol
#include "wlr-data-control-protocol.h"

struct client_state {
    struct wl_display *display;
    struct wl_registry *registry;
    
    // wlr-data-control specific objects
    struct zwlr_data_control_manager_v1 *data_control_manager;
    struct zwlr_data_control_device_v1 *data_control_device;
    struct zwlr_data_control_offer_v1 *current_offer;
    struct wl_seat *seat;
    
    bool running;
};

// Global state for signal handling
static struct client_state *global_state = NULL;

// Signal handler for clean exit
static void
handle_signal(int signum) {
    printf("\nReceived signal %d, exiting...\n", signum);
    if (global_state) {
        global_state->running = false;
        wl_display_flush(global_state->display);
    }
}

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
    zwlr_data_control_offer_v1_receive(state->current_offer, mime_type, pipefd[1]);
    close(pipefd[1]); // Close write end immediately after request
    
    // Dispatch events to ensure the data is sent through the pipe
    wl_display_flush(state->display);
    wl_display_dispatch(state->display);
    
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

// Data offer event handlers
static void
data_offer_offer(void *data, struct zwlr_data_control_offer_v1 *offer, const char *mime_type)
{
    printf("Data offer with MIME type: %s\n", mime_type);
}

static const struct zwlr_data_control_offer_v1_listener data_offer_listener = {
    .offer = data_offer_offer
};

// Data device event handlers
static void
data_device_data_offer(void *data, struct zwlr_data_control_device_v1 *device,
                     struct zwlr_data_control_offer_v1 *offer)
{
    printf("New data offer received\n");
    
    zwlr_data_control_offer_v1_add_listener(offer, &data_offer_listener, data);
}

static void
data_device_selection(void *data, struct zwlr_data_control_device_v1 *device,
                    struct zwlr_data_control_offer_v1 *offer)
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

static void
data_device_finished(void *data, struct zwlr_data_control_device_v1 *device)
{
    printf("Data device finished\n");
}

static const struct zwlr_data_control_device_v1_listener data_device_listener = {
    .data_offer = data_device_data_offer,
    .selection = data_device_selection,
    .primary_selection = NULL,  // We're only interested in clipboard, not primary selection
    .finished = data_device_finished
};

// Seat listener - not strictly needed but keeping for completeness
static void
seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities)
{
    // Not used for clipboard monitoring
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = NULL
};

// Registry handler
static void
registry_handle_global(void *data, struct wl_registry *registry,
                     uint32_t id, const char *interface, uint32_t version)
{
    struct client_state *state = data;

    printf("Got interface: %s (version %d)\n", interface, version);

    if (strcmp(interface, wl_seat_interface.name) == 0) {
        state->seat = wl_registry_bind(registry, id, &wl_seat_interface, 1);
        wl_seat_add_listener(state->seat, &seat_listener, state);
        printf("Found seat\n");
    } else if (strcmp(interface, zwlr_data_control_manager_v1_interface.name) == 0) {
        state->data_control_manager = wl_registry_bind(
            registry, id, &zwlr_data_control_manager_v1_interface, 1);
        printf("Found wlr_data_control_manager - this compositor supports wlr-data-control!\n");
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

int
main(int argc, char **argv)
{
    struct client_state state = { 0 };
    state.running = true;
    global_state = &state;
    
    // Set up signal handlers for clean exit
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

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
    
    // Set up the data control device (for clipboard monitoring)
    if (state.seat && state.data_control_manager) {
        state.data_control_device = zwlr_data_control_manager_v1_get_data_device(
            state.data_control_manager, state.seat);
        
        zwlr_data_control_device_v1_add_listener(state.data_control_device, 
                                               &data_device_listener, &state);
        
        printf("Set up wlr-data-control for clipboard monitoring\n");
    } else {
        if (!state.data_control_manager) {
            fprintf(stderr, "wlr-data-control protocol not supported by this compositor.\n");
            fprintf(stderr, "This will only work with wlroots-based compositors like Sway or Wayfire.\n");
        }
        if (!state.seat) {
            fprintf(stderr, "No seat found - can't monitor clipboard\n");
        }
        fprintf(stderr, "Clipboard monitoring not available\n");
        return 1;
    }

    printf("Monitoring clipboard events. Copy text in another application to see it appear here.\n");
    printf("Press Ctrl+C to exit.\n");

    // Main loop
    while (state.running) {
        if (wl_display_dispatch(state.display) == -1) {
            fprintf(stderr, "Error in dispatch: %s\n", strerror(errno));
            break;
        }
    }

    // Clean up
    if (state.data_control_device)
        zwlr_data_control_device_v1_destroy(state.data_control_device);
    if (state.data_control_manager)
        zwlr_data_control_manager_v1_destroy(state.data_control_manager);
    if (state.seat)
        wl_seat_destroy(state.seat);
    if (state.registry)
        wl_registry_destroy(state.registry);
    if (state.display)
        wl_display_disconnect(state.display);

    return 0;
}
