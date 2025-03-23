const std = @import("std");

const c = @cImport(@cInclude("wayland-client.h"));

const WaylandState = struct {
    display: ?*c.wl_display = null,
    registry: ?*c.wl_registry = null,
    seat: ?*c.wl_seat = null,
    data_device_manager: ?*c.wl_data_device_manager = null,
    data_device: ?*c.wl_data_device = null,
};
var state = WaylandState{};

fn registry_handle_global_remove(_: ?*anyopaque, _: ?*c.wl_registry, _: u32) callconv(.C) void {}

fn registry_handle_global(_: ?*anyopaque, _: ?*c.wl_registry, name: u32, interface: [*c]const u8, version: u32) callconv(.C) void {
    const interface_str = std.mem.span(interface);
    std.debug.print("Interface: {s}, version: {}\n", .{ interface_str, version });

    if (std.mem.eql(u8, interface_str, "wl_seat")) {
        state.seat = @ptrCast(c.wl_registry_bind(state.registry, name, @ptrCast(&c.wl_seat_interface), 1));
        std.debug.print("Bound to wl_seat\n", .{});
    } else if (std.mem.eql(u8, interface_str, "wl_data_device_manager")) {
        state.data_device_manager = @ptrCast(c.wl_registry_bind(state.registry, name, @ptrCast(&c.wl_data_device_manager_interface), 1));
        std.debug.print("Bound to wl_data_device_manager\n", .{});
    }
}
const registry_listener: c.wl_registry_listener = .{
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

const data_device_listener = c.wl_data_device_listener{
    .data_offer = data_device_data_offer,
    .enter = data_device_enter,
    .leave = data_device_leave,
    .motion = data_device_motion,
    .drop = data_device_drop,
    .selection = data_device_selection,
};
fn offer_handle_offer(data: ?*anyopaque, offer: ?*c.wl_data_offer, mime_type: [*c]const u8) callconv(.C) void {
    _ = data;
    _ = offer;
    std.debug.print("Offered mime type: {s}\n", .{mime_type});
}

fn data_device_data_offer(data: ?*anyopaque, data_device: ?*c.wl_data_device, data_offer: ?*c.wl_data_offer) callconv(.C) void {
    _ = data;
    _ = data_device;
    std.debug.print("Data offer received\n", .{});

    // Add a data offer listener to see available mime types
    const offer_listener = c.wl_data_offer_listener{
        .offer = offer_handle_offer,
        // You'll need to define this function
    };

    _ = c.wl_data_offer_add_listener(data_offer, &offer_listener, null);
}

fn data_device_enter(_: ?*anyopaque, _: ?*c.wl_data_device, _: u32, _: ?*c.wl_surface, _: i32, _: i32, _: ?*c.wl_data_offer) callconv(.C) void {}

fn data_device_leave(_: ?*anyopaque, _: ?*c.wl_data_device) callconv(.C) void {}

fn data_device_motion(_: ?*anyopaque, _: ?*c.wl_data_device, _: u32, _: i32, _: i32) callconv(.C) void {}

fn data_device_drop(_: ?*anyopaque, _: ?*c.wl_data_device) callconv(.C) void {}

fn data_device_selection(data: ?*anyopaque, data_device: ?*c.wl_data_device, data_offer: ?*c.wl_data_offer) callconv(.C) void {
    _ = data;
    _ = data_device;

    std.debug.print("Selection callback triggered\n", .{});

    if (data_offer == null) {
        std.debug.print("Selection cleared (null data_offer)\n", .{});
        return;
    }

    std.debug.print("New clipboard selection\n", .{});

    // Request text/plain MIME type
    c.wl_data_offer_receive(data_offer, "text/plain", 1);

    std.debug.print("Requested clipboard content (text/plain)\n", .{});
}
fn test_clipboard_callbacks() void {
    std.debug.print("Testing clipboard callbacks...\n", .{});

    // Check that our data device is valid
    if (state.data_device == null) {
        std.debug.print("Error: data_device is null\n", .{});
        return;
    }

    // Check that our callbacks are properly setup
    std.debug.print("Data device listener is attached\n", .{});

    // Force a display sync to make sure everything is processed
    _ = c.wl_display_roundtrip(state.display);
    std.debug.print("Display roundtrip completed\n", .{});
}

pub fn main() !void {
    state.display = c.wl_display_connect(null);
    if (state.display == null) {
        std.debug.print("Failed to connect to Wayland display\n", .{});
        return error.ConnectionFailed;
    }
    defer c.wl_display_disconnect(state.display);
    errdefer c.wl_display_disconnect(state.display);

    std.debug.print("Successfully connected to Wayland display\n", .{});

    state.registry = c.wl_display_get_registry(state.display);

    _ = c.wl_registry_add_listener(state.registry, &registry_listener, null);
    _ = c.wl_display_roundtrip(state.display);

    // Check if we got the required interfaces
    if (state.data_device_manager == null or state.seat == null) {
        std.debug.print("Failed to bind to required interfaces\n", .{});
        return error.BindingFailed;
    }

    state.data_device = c.wl_data_device_manager_get_data_device(state.data_device_manager, state.seat);
    if (state.data_device == null) {
        std.debug.print("Failed to create data device\n", .{});
        c.wl_display_disconnect(state.display);
        return error.DataDeviceFailed;
    }
    // Add data device listener
    _ = c.wl_data_device_add_listener(state.data_device, &data_device_listener, null);

    std.debug.print("Clipboard monitor set up. Waiting for events...\n", .{});
    std.debug.print("Press Ctrl+C to exit\n", .{});

    test_clipboard_callbacks();

    var event_count: usize = 0;
    var last_event_time: i64 = 0;
    var status: c_int = 0;
    std.debug.print("Entering event loop. Waiting for clipboard events, dispatch status: {}\n", .{status});
    // Event loop
    while (true) {
        // Continue processing events
        status = c.wl_display_dispatch(state.display);
        std.debug.print("display dispatch status: {}\n", .{status});
        const processed = c.wl_display_dispatch_pending(state.display);
        // Make sure all requests are sent
        _ = c.wl_display_flush(state.display);
        if (processed > 0) {
            event_count += @intCast(processed);
            std.debug.print("Processed {} events (total: {})\n", .{ processed, event_count });
            last_event_time = std.time.milliTimestamp();
        }
        // Every few seconds, print a status update
        const current_time = std.time.milliTimestamp();
        if (current_time - last_event_time > 5000) { // 5 seconds
            std.debug.print("No events for 5 seconds. Total events so far: {}\n", .{event_count});
            last_event_time = current_time;
        }
        // Small sleep to avoid burning CPU
        std.time.sleep(10 * std.time.ns_per_ms);
    }

    std.debug.print("display dispatch status: {}\n", .{status});

    std.debug.print("Finished\n", .{});
    // Cleanup
    if (state.seat != null) c.wl_seat_destroy(state.seat);
    if (state.data_device_manager != null) c.wl_data_device_manager_destroy(state.data_device_manager);
    if (state.registry != null) c.wl_registry_destroy(state.registry);
    c.wl_display_disconnect(state.display);
}
