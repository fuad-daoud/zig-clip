const std = @import("std");

const c = @cImport(@cInclude("wayland-client.h"));

fn registry_handle_global_remove(data: ?*anyopaque, registry: ?*c.wl_registry, name: u32) callconv(.C) void {
    _ = data;
    _ = registry;
    _ = name;
    // This space deliberately left blank
}

fn registry_handle_global(data: ?*anyopaque, registry: ?*c.wl_registry, name: u32, interface: [*c]const u8, version: u32) callconv(.C) void {
    _ = data;
    _ = registry;
    std.debug.print("interface: '{s}', version: {}, name: {}\n", .{ interface, version, name });
}

pub fn main() !void {
    // Connect to Wayland display
    const display = c.wl_display_connect(null);
    if (display == null) {
        std.debug.print("Failed to connect to Wayland display\n", .{});
        return error.ConnectionFailed;
    }

    std.debug.print("Successfully connected to Wayland display\n", .{});

    // Disconnect from Wayland display
    defer c.wl_display_disconnect(display);

    const registry = c.wl_display_get_registry(display);

    const registry_listener: c.wl_registry_listener = .{
        .global = registry_handle_global,
        .global_remove = registry_handle_global_remove,
    };
    _ = c.wl_registry_add_listener(registry, &registry_listener, null);
    _ = c.wl_display_roundtrip(display);

    std.debug.print("Finished\n", .{});
}
