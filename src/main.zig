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

fn data_device_data_offer(_: ?*anyopaque, _: ?*c.wl_data_device, _: ?*c.wl_data_offer) callconv(.C) void {
    std.debug.print("Data offer received\n", .{});
}

fn data_device_enter(_: ?*anyopaque, _: ?*c.wl_data_device, _: u32, _: ?*c.wl_surface, _: i32, _: i32, _: ?*c.wl_data_offer) callconv(.C) void {}

fn data_device_leave(_: ?*anyopaque, _: ?*c.wl_data_device) callconv(.C) void {}

fn data_device_motion(_: ?*anyopaque, _: ?*c.wl_data_device, _: u32, _: i32, _: i32) callconv(.C) void {}

fn data_device_drop(_: ?*anyopaque, _: ?*c.wl_data_device) callconv(.C) void {}

fn data_device_selection(_: ?*anyopaque, _: ?*c.wl_data_device, data_offer: ?*c.wl_data_offer) callconv(.C) void {
    if (data_offer == null) {
        std.debug.print("Selection cleared\n", .{});
        return;
    }

    std.debug.print("New clipboard selection\n", .{});

    // Request text/plain MIME type
    c.wl_data_offer_receive(data_offer, "text/plain", 1);

    // TODO: In a real implementation, you would need to handle reading from the pipe
    // This requires setting up non-blocking I/O or a separate thread
    std.debug.print("Requested clipboard content (text/plain)\n", .{});
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

    // Event loop
    while (c.wl_display_dispatch(state.display) != -1) {
        // Continue processing events
    }

    std.debug.print("Finished\n", .{});
}
