#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <glib.h>
#include <config.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <glade/glade.h>
#include <GL/gl.h>

#include "main.h"
#include "range.h"


gboolean range_check_valid(const gchar *addr_str, const gchar *bits_str)
{
	errno = 0;
	struct in_addr addr;
	gint bits = -1;

	bits = strtol(bits_str, NULL, 10);

	// make sure strtol went ok
	if (bits == 0 && strcmp("0", bits_str) != 0) {
		return FALSE;
	}

	// make sure they're not blank
	if (strlen(addr_str) == 0 || strlen(bits_str) == 0) {
		return FALSE;
	}

	// make sure its a valid addresses
	if (inet_aton(addr_str, &addr) == 0) {
		return FALSE;
	}

	// make sure network bits are valid
	if (bits < 0 || bits > 32) {
		return FALSE;
	}

	return TRUE;
}


void range_text_changed(GtkEditable *editable, gpointer range_data)
{
	range_widget_t *range = (range_widget_t *) range_data;
	GdkColor color;
 
	// get the text from the editable widget
	gchar *addr = gtk_editable_get_chars(GTK_EDITABLE(range->ip), 0, -1);
	gchar *bits = gtk_editable_get_chars(GTK_EDITABLE(range->bits), 0, -1);

	// if its an empty string, set bg to white
	if (strlen(addr) == 0 && strlen(bits) == 0) {
		gdk_color_parse("white", &color);
	} else {
		if (range_check_valid(addr, bits)) {
			// if valid, set bg to green
			gdk_color_parse("#afffaf", &color);
		} else {
			// else, set bg to red
			gdk_color_parse("#ffafaf", &color);
		}
	}

	// change the bg color
	gtk_widget_modify_base(GTK_WIDGET(range->ip), GTK_STATE_NORMAL, &color);
	gtk_widget_modify_base(GTK_WIDGET(range->bits), GTK_STATE_NORMAL, &color);

	g_free(addr);
	g_free(bits);
}


void range_reverse_quadtree(bound_t *bound, struct in_addr *low_addr, struct in_addr *high_addr)
{
	GLfloat adjustment;
	guint k, loops;
	GLfloat bound_x, bound_y;
	guint address, tmp;

	address = 0;
	bound_x = bound->max_x_coord;
	bound_y = bound->max_y_coord;
	loops = 32 / 2;
	adjustment = COORD_SIZE;
	for (k = 0; k < loops; ++k) {
		adjustment /= 2.0;
		tmp = 0;

		if (bound_x - adjustment > 0) {
			tmp += 2;
			bound_x -= adjustment;
		}

		if (bound_y - adjustment > 0) {
			tmp += 1;
			bound_y -= adjustment;
		}

		if (tmp == 0) continue;

		address |= ((address >> ((((k/4)+1)*8) - 2*((k%4)+1))) | tmp) << ((((k/4)+1)*8) - 2*((k%4)+1));
	}

	high_addr->s_addr = address;

	address = 0;
	bound_x = bound->min_x_coord;
	bound_y = bound->min_y_coord;
	loops = 32 / 2;
	adjustment = COORD_SIZE;
	for (k = 0; k < loops; ++k) {
		adjustment /= 2.0;
		tmp = 0;

		if (bound_x - adjustment >= 0) {
			tmp += 2;
			bound_x -= adjustment;
		}

		if (bound_y - adjustment >= 0) {
			tmp += 1;
			bound_y -= adjustment;
		}

		if (tmp == 0) continue;

		address |= ((address >> ((((k/4)+1)*8) - 2*((k%4)+1))) | tmp) << ((((k/4)+1)*8) - 2*((k%4)+1));
	}

	low_addr->s_addr = address;
}


void range_calculate_quadtree(ip_range_t *range)
{
	guint i, loops;
	GLfloat adjustment;

	range->x_coord = 0.0;
	range->y_coord = 0.0;

	guint bits = range->network_bits;

	// if bits are odd, subtract one from the bits
	if (bits % 2 == 1) {
		bits -= 1;
	}

	// calculate width of the range
	guint num_of_quad_divisions = bits / 2;
	range->width = (COORD_SIZE / (pow(2.0, num_of_quad_divisions)));

	// calculate the coords of the min address of the range
	loops = bits / 2;
	adjustment = COORD_SIZE;
	for (i = 0; i < loops; ++i) {
		adjustment /= 2.0;

		// for a COORD_SIZE x COORD_SIZE grid
		switch (SHIFT(range->addr, ((((i/4)+1)*8) - 2*((i%4)+1)), 0x3)) {
			// 00
			case 0:
				// do nothing
			break;

			// 01
			case 1:
				range->y_coord += adjustment;
			break;

			// 10
			case 2:
				range->x_coord += adjustment;
			break;

			// 11
			case 3:
				range->x_coord += adjustment;
				range->y_coord += adjustment;
			break;
		}
	}
}


void range_square_bound(bound_t *bound)
{
	g_debug("our 'pre-squared' bounding box:  min_x_coord:%f  max_x_coord:%f  min_y_coord:%f  max_y_coord:%f", bound->min_x_coord, bound->max_x_coord, bound->min_y_coord, bound->max_y_coord);

	// now we need to expand our bounding box to make it square
	guint need_to_expand;
	guint width = bound->max_x_coord - bound->min_x_coord;
	guint height = bound->max_y_coord - bound->min_y_coord;
	if (width > height) {
		// width is greater than height, need to expand down/up
		need_to_expand = width - height;

		guint space_to_expand_down = COORD_SIZE - bound->max_y_coord;

		if (need_to_expand > space_to_expand_down) {
			bound->max_y_coord += space_to_expand_down;
			need_to_expand -= space_to_expand_down;

			guint space_to_expand_up = bound->min_y_coord;

			if (need_to_expand > space_to_expand_up) {
				g_warning("error with expanding our bounding square up...this should never happen, yell at jon");
			} else {
				bound->min_y_coord -= need_to_expand;
				need_to_expand = 0;
			}
		} else {
			bound->max_y_coord += need_to_expand;
			need_to_expand = 0;
		}
	} else {
		// height is greater than width, need to expand right/left
		need_to_expand = height - width;

		guint space_to_expand_right = COORD_SIZE - bound->max_x_coord;

		if (need_to_expand > space_to_expand_right) {
			bound->max_x_coord += space_to_expand_right;
			need_to_expand -= space_to_expand_right;

			guint space_to_expand_left = bound->min_x_coord;

			if (need_to_expand > space_to_expand_left) {
				g_warning("error with expanding our bounding square left...this should never happen, yell at jon");
			} else {
				bound->min_x_coord -= need_to_expand;
				need_to_expand = 0;
			}
		} else {
			bound->max_x_coord += need_to_expand;
			need_to_expand = 0;
		}
	}

	g_debug("our 'squared' bounding box:  min_x_coord:%f  max_x_coord:%f  min_y_coord:%f  max_y_coord:%f", bound->min_x_coord, bound->max_x_coord, bound->min_y_coord, bound->max_y_coord);
}


guint range_count_valid(ip_range_t *ranges, guint num_widgets, GQueue *queue, bound_t *bound)
{
	guint i;
	guint num_ranges;
	ip_range_t *range;
	struct in_addr addr;
	const gchar *addr_str, *bits_str;

	num_ranges = 0;

	// count our new ranges
	for (i = 0; i < num_widgets; ++i) {
		range_widget_t *widget = g_queue_peek_nth(queue, i);

		addr_str = gtk_entry_get_text(GTK_ENTRY(widget->ip));
		bits_str = gtk_entry_get_text(GTK_ENTRY(widget->bits));

		// make sure this range is actually enabled (checkbox state == TRUE)
		if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget->checkbox))) {
			continue;
		}

		// make sure the range values are valid
		if (!range_check_valid(addr_str, bits_str)) {
			continue;
		}

		// aton
		inet_aton(addr_str, &addr);

		range = &ranges[num_ranges];

		// don't convert to host length until the quadtree calculations have been done
		range->addr = addr.s_addr;
		range->network_bits = atoi(bits_str);

		// calculate the quadtree coords of the min and max addresses of the range
		range_calculate_quadtree(range);

		// convert to host length
		range->addr = g_ntohl(addr.s_addr);

		g_debug("our range box:  x_coord:%f  y_coord:%f  width:%f", range->x_coord, range->y_coord, range->width);

		// check if this box increases our bounding box extremes at all
		if (range->x_coord < bound->min_x_coord) {
			bound->min_x_coord = range->x_coord;
		}
		if (range->x_coord + range->width > bound->max_x_coord) {
			bound->max_x_coord = range->x_coord + range->width;
		}
		if (range->y_coord < bound->min_y_coord) {
			bound->min_y_coord = range->y_coord;
		}
		if (range->y_coord + range->width > bound->max_y_coord) {
			bound->max_y_coord = range->y_coord + range->width;
		}

		// expand our src_dst_port common bound too if we're using that data type
		if (range->x_coord < viz->src_dst_port_bound.min_x_coord) {
			viz->src_dst_port_bound.min_x_coord = range->x_coord;
		}
		if (range->x_coord + range->width > viz->src_dst_port_bound.max_x_coord) {
			viz->src_dst_port_bound.max_x_coord = range->x_coord + range->width;
		}
		if (range->y_coord < viz->src_dst_port_bound.min_y_coord) {
			viz->src_dst_port_bound.min_y_coord = range->y_coord;
		}
		if (range->y_coord + range->width > viz->src_dst_port_bound.max_y_coord) {
			viz->src_dst_port_bound.max_y_coord = range->y_coord + range->width;
		}

		// increment the number of ranges we need to send over
		num_ranges++;
	}

	return num_ranges;
}


gboolean range_add_all_from_widgets()
{
	guint i, old_num_ranges;
	frange_t *curr_range, *old_ranges;

	viz->src_bound.min_x_coord = COORD_SIZE;
	viz->src_bound.max_x_coord = 0;
	viz->src_bound.min_y_coord = COORD_SIZE;
	viz->src_bound.max_y_coord = 0;
	viz->dst_bound.min_x_coord = COORD_SIZE;
	viz->dst_bound.max_x_coord = 0;
	viz->dst_bound.min_y_coord = COORD_SIZE;
	viz->dst_bound.max_y_coord = 0;
	viz->src_dst_port_bound.min_x_coord = COORD_SIZE;
	viz->src_dst_port_bound.max_x_coord = 0;
	viz->src_dst_port_bound.min_y_coord = COORD_SIZE;
	viz->src_dst_port_bound.max_y_coord = 0;

	// record our old values to figure out whether there is a change in the ranges
	old_num_ranges = viz->num_src_ranges + viz->num_dst_ranges;
	old_ranges = g_new0(frange_t, viz->num_src_ranges + viz->num_dst_ranges);

	// record our old ranges but make sure that current ranges exist
	if (viz->mode_ranges) {
		memcpy(old_ranges, viz->mode_ranges, old_num_ranges * sizeof(frange_t));
	}

	// allocate enough mem for the max number of valid ranges
	g_free(viz->src_ranges);
	g_free(viz->dst_ranges);
	viz->src_ranges = g_new0(ip_range_t, viz->num_src_widgets);
	viz->dst_ranges = g_new0(ip_range_t, viz->num_dst_widgets);

	// count the number of valid src/dst ranges we have
	viz->num_src_ranges = range_count_valid(viz->src_ranges, viz->num_src_widgets, viz->prefs_src_range_widgets, &viz->src_bound);
	viz->num_dst_ranges = range_count_valid(viz->dst_ranges, viz->num_dst_widgets, viz->prefs_dst_range_widgets, &viz->dst_bound);

	// free/reallocate up our current mode ranges
	g_free(viz->mode_ranges);
	viz->mode_ranges = g_new0(frange_t, viz->num_src_ranges + viz->num_dst_ranges);
	curr_range = viz->mode_ranges;

	// process our new ranges
	for (i = 0; i < viz->num_src_ranges; ++i) {
		curr_range->addr = g_htonl(viz->src_ranges[i].addr);
		curr_range->network_bits = g_htonl(viz->src_ranges[i].network_bits);

		g_debug("found src range: %d - %u/%u", i, viz->src_ranges[i].addr, viz->src_ranges[i].network_bits);
		curr_range++;
	}
	for (i = 0; i < viz->num_dst_ranges; ++i) {
		curr_range->addr = g_htonl(viz->dst_ranges[i].addr);
		curr_range->network_bits = g_htonl(viz->dst_ranges[i].network_bits);

		g_debug("found dst range: %d - %u/%u", i, viz->dst_ranges[i].addr, viz->dst_ranges[i].network_bits);
		curr_range++;
	}

	// assign our mode variables
	viz->mode.ranges_src = viz->num_src_ranges;
	viz->mode.ranges_dst = viz->num_dst_ranges;

	// if no valid ranges are found, give some sane values
	if (viz->num_src_ranges == 0) {
		viz->src_bound.min_x_coord = 0;
		viz->src_bound.max_x_coord = 0;
		viz->src_bound.min_y_coord = 0;
		viz->src_bound.max_y_coord = 0;
	}
	if (viz->num_dst_ranges == 0) {
		viz->dst_bound.min_x_coord = 0;
		viz->dst_bound.max_x_coord = 0;
		viz->dst_bound.min_y_coord = 0;
		viz->dst_bound.max_y_coord = 0;
	}

	// expand the src/dst bounding boxes to make it square
	range_square_bound(&viz->src_bound);
	range_square_bound(&viz->dst_bound);
	range_square_bound(&viz->src_dst_port_bound);

	// if there's a diff num of valid ranges, we definitely need an update
	if (old_num_ranges != (viz->num_src_ranges + viz->num_dst_ranges)) {
		g_free(old_ranges);
		return TRUE;
	}

	// if the content of the ranges is different, we need an update
	if (memcmp(old_ranges, viz->mode_ranges, old_num_ranges * sizeof(frange_t)) != 0) {
		g_free(old_ranges);
		return TRUE;
	}

	g_free(old_ranges);
	return FALSE;
}
