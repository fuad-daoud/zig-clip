const std = @import("std");

const c = @cImport(@cInclude("wayland-client.h"));

pub fn main() !void {
    // Connect to Wayland display
    const display = c.wl_display_connect(null);
    if (display == null) {
        std.debug.print("Failed to connect to Wayland display\n", .{});
        return error.ConnectionFailed;
    }

    std.debug.print("Successfully connected to Wayland display\n", .{});

    // Disconnect from Wayland display
    c.wl_display_disconnect(display);
}
