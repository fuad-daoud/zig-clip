/* Generated by wayland-scanner 1.23.1 */

/*
 * Copyright © 2018 Simon Ser
 * Copyright © 2019 Ivan Molodetskikh
 *
 * Permission to use, copy, modify, distribute, and sell this
 * software and its documentation for any purpose is hereby granted
 * without fee, provided that the above copyright notice appear in
 * all copies and that both that copyright notice and this permission
 * notice appear in supporting documentation, and that the name of
 * the copyright holders not be used in advertising or publicity
 * pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include "wayland-util.h"

#ifndef __has_attribute
# define __has_attribute(x) 0  /* Compatibility with non-clang compilers. */
#endif

#if (__has_attribute(visibility) || defined(__GNUC__) && __GNUC__ >= 4)
#define WL_PRIVATE __attribute__ ((visibility("hidden")))
#else
#define WL_PRIVATE
#endif

extern const struct wl_interface wl_seat_interface;
extern const struct wl_interface zwlr_data_control_device_v1_interface;
extern const struct wl_interface zwlr_data_control_offer_v1_interface;
extern const struct wl_interface zwlr_data_control_source_v1_interface;

static const struct wl_interface *wlr_data_control_unstable_v1_types[] = {
	NULL,
	NULL,
	&zwlr_data_control_source_v1_interface,
	&zwlr_data_control_device_v1_interface,
	&wl_seat_interface,
	&zwlr_data_control_source_v1_interface,
	&zwlr_data_control_source_v1_interface,
	&zwlr_data_control_offer_v1_interface,
	&zwlr_data_control_offer_v1_interface,
	&zwlr_data_control_offer_v1_interface,
};

static const struct wl_message zwlr_data_control_manager_v1_requests[] = {
	{ "create_data_source", "n", wlr_data_control_unstable_v1_types + 2 },
	{ "get_data_device", "no", wlr_data_control_unstable_v1_types + 3 },
	{ "destroy", "", wlr_data_control_unstable_v1_types + 0 },
};

WL_PRIVATE const struct wl_interface zwlr_data_control_manager_v1_interface = {
	"zwlr_data_control_manager_v1", 2,
	3, zwlr_data_control_manager_v1_requests,
	0, NULL,
};

static const struct wl_message zwlr_data_control_device_v1_requests[] = {
	{ "set_selection", "?o", wlr_data_control_unstable_v1_types + 5 },
	{ "destroy", "", wlr_data_control_unstable_v1_types + 0 },
	{ "set_primary_selection", "2?o", wlr_data_control_unstable_v1_types + 6 },
};

static const struct wl_message zwlr_data_control_device_v1_events[] = {
	{ "data_offer", "n", wlr_data_control_unstable_v1_types + 7 },
	{ "selection", "?o", wlr_data_control_unstable_v1_types + 8 },
	{ "finished", "", wlr_data_control_unstable_v1_types + 0 },
	{ "primary_selection", "2?o", wlr_data_control_unstable_v1_types + 9 },
};

WL_PRIVATE const struct wl_interface zwlr_data_control_device_v1_interface = {
	"zwlr_data_control_device_v1", 2,
	3, zwlr_data_control_device_v1_requests,
	4, zwlr_data_control_device_v1_events,
};

static const struct wl_message zwlr_data_control_source_v1_requests[] = {
	{ "offer", "s", wlr_data_control_unstable_v1_types + 0 },
	{ "destroy", "", wlr_data_control_unstable_v1_types + 0 },
};

static const struct wl_message zwlr_data_control_source_v1_events[] = {
	{ "send", "sh", wlr_data_control_unstable_v1_types + 0 },
	{ "cancelled", "", wlr_data_control_unstable_v1_types + 0 },
};

WL_PRIVATE const struct wl_interface zwlr_data_control_source_v1_interface = {
	"zwlr_data_control_source_v1", 1,
	2, zwlr_data_control_source_v1_requests,
	2, zwlr_data_control_source_v1_events,
};

static const struct wl_message zwlr_data_control_offer_v1_requests[] = {
	{ "receive", "sh", wlr_data_control_unstable_v1_types + 0 },
	{ "destroy", "", wlr_data_control_unstable_v1_types + 0 },
};

static const struct wl_message zwlr_data_control_offer_v1_events[] = {
	{ "offer", "s", wlr_data_control_unstable_v1_types + 0 },
};

WL_PRIVATE const struct wl_interface zwlr_data_control_offer_v1_interface = {
	"zwlr_data_control_offer_v1", 1,
	2, zwlr_data_control_offer_v1_requests,
	1, zwlr_data_control_offer_v1_events,
};

