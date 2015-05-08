#ifndef __RANGE_H__
#define __RANGE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <config.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <glade/glade.h>
#include <GL/gl.h>


typedef struct {
	GLfloat min_x_coord;
	GLfloat max_x_coord;
	GLfloat min_y_coord;
	GLfloat max_y_coord;
} bound_t;

typedef struct {
	guint addr;
	guint network_bits;
	GLfloat width;
	GLfloat x_coord;
	GLfloat y_coord;
} ip_range_t;

gboolean range_add_all_from_widgets();
void range_text_changed(GtkEditable *editable, gpointer range_data);
void range_reverse_quadtree(bound_t *bound, struct in_addr *low_addr, struct in_addr *high_addr);

#endif
