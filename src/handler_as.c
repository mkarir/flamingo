#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <glib.h>
#include <config.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <glade/glade.h>
#include <GL/gl.h>

#include "main.h"
#include "glext2d.h"
#include "glext3d.h"
#include "socket.h"


void handler_as_init_interface()
{
	// disable transparency by default
	viz->transparent = FALSE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(viz->transparent_check_box), FALSE);

	// enable lighting by default
	viz->disable_lighting = FALSE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(viz->lighting_check_box), FALSE);

	// disable labels by default
	viz->draw_labels = FALSE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(viz->labels_check_box), FALSE);

	// enable/disable appropriate scaling bars
	gtk_widget_set_sensitive(viz->line_scale_range, FALSE);
	gtk_widget_set_sensitive(viz->line_scale_label, FALSE);
	gtk_widget_set_sensitive(viz->slice_scale_range, FALSE);
	gtk_widget_set_sensitive(viz->slice_scale_label, FALSE);

	// disable our other 2 sets of ranges
	gtk_widget_set_sensitive(viz->port1_min_range, FALSE);
	gtk_widget_set_sensitive(viz->port1_max_range, FALSE);
	gtk_widget_set_sensitive(viz->port1_min_label, FALSE);
	gtk_widget_set_sensitive(viz->port1_max_label, FALSE);
	gtk_widget_set_sensitive(viz->port2_min_range, FALSE);
	gtk_widget_set_sensitive(viz->port2_max_range, FALSE);
	gtk_widget_set_sensitive(viz->port2_min_label, FALSE);
	gtk_widget_set_sensitive(viz->port2_max_label, FALSE);

	// set the correct labels on those ranges
	gtk_label_set_label(GTK_LABEL(viz->port1_min_label), "N/A:");
	gtk_label_set_label(GTK_LABEL(viz->port1_max_label), "N/A:");
	gtk_label_set_label(GTK_LABEL(viz->port2_min_label), "N/A:");
	gtk_label_set_label(GTK_LABEL(viz->port2_max_label), "N/A:");
}


coord_t *handler_as_get_node_coords(tree_node_t *node)
{
	return (&node->as.coords);
}


gboolean handler_as_check_2d_click(tree_node_t *node, GLfloat x_pos, GLfloat y_pos)
{
	// check if the click was within the boundries of the node
	if (x_pos >= node->as.coords.x_coord
			&& x_pos <= node->as.coords.x_coord + node->as.coords.x_width
			&& y_pos >= node->as.coords.y_coord
			&& y_pos <= node->as.coords.y_coord + node->as.coords.y_width) {
		return TRUE;
	} else {
		return FALSE;
	}
}


gboolean handler_as_check_visibility(tree_node_t *node)
{
	return (node->volume >= viz->threshold_min && node->volume <= viz->threshold_max);
}


static inline gboolean handler_as_render_2d_node(gpointer node_in, gpointer null, gpointer data)
{
	tree_node_t *node = node_in;

	// return if the node is not within the thresholds
	if (!viz->handler.check_visibility(node)) {
		return FALSE;
	}

	// precalculate these to avoid redundant calculations
	GLfloat x_width = node->as.coords.x_coord + node->as.coords.x_width;
	GLfloat y_width = node->as.coords.y_coord + node->as.coords.y_width;

	GLfloat quad_strip_v[] = {
					node->as.coords.x_coord, node->as.coords.y_coord,
					x_width, node->as.coords.y_coord,
					node->as.coords.x_coord, y_width,
					x_width, y_width
				 };

	// set color and transparency
	if (!viz->transparent) {
		node->as.coords.color[3] = 255;
	}
	glColor4ubv(node->as.coords.color);

	// render our arrays of vertices
	glVertexPointer(2, GL_FLOAT, 0, quad_strip_v);
	glDrawArrays(GL_QUAD_STRIP, 0, 4);

	return FALSE;
}


void handler_as_render_2d_scene(void)
{
	// draw a square outline
	glCallList(WHITE_2D_SQUARE_CALL_LIST);

	// draw our labels
	glCallList(LABELS_2D_AS_CALL_LIST);

	// render all our nodes!
	g_tree_foreach(viz->nodes, handler_as_render_2d_node, NULL);	
}


static inline gboolean handler_as_render_3d_node(gpointer node_in, gpointer null, gpointer data)
{
	tree_node_t *node = node_in;

	// only draw the node if it is between the thresholds
	if (!viz->handler.check_visibility(node)) {
		return FALSE;
	}

	// determine the node height based on the current z-axis scale
	gint max, diff;
	if (viz->disable_max_threshold) {
		max = gtk_spin_button_get_value(GTK_SPIN_BUTTON(viz->prefs_max_threshold_value));
	} else {
		max = viz->threshold_max;
	}
	diff = max - viz->threshold_min;
	GLfloat unit_size = CUBE_SIZE / diff;
	node->as.coords.height = (GLfloat) ((node->volume - viz->threshold_min) * unit_size);

	// precalculate these to avoid redundant calculations
	GLfloat x_width = node->as.coords.x_coord + node->as.coords.x_width;
	GLfloat y_width = node->as.coords.y_coord + node->as.coords.y_width;
	GLfloat z_height = node->as.coords.z_coord + node->as.coords.height + Z_HEIGHT_ADJ;

	// define our array of quadrilaterals
	GLfloat quads_v[] =     {
					// top face
					node->as.coords.x_coord, node->as.coords.y_coord, z_height,
					x_width, node->as.coords.y_coord, z_height,
					x_width, y_width, z_height,
					node->as.coords.x_coord, y_width, z_height,

					// "front" face
					x_width, node->as.coords.y_coord, node->as.coords.z_coord,
					x_width, y_width, node->as.coords.z_coord,
					x_width, y_width, z_height,
					x_width, node->as.coords.y_coord, z_height,

					// "back" face
					node->as.coords.x_coord, node->as.coords.y_coord, node->as.coords.z_coord,
					node->as.coords.x_coord, y_width, node->as.coords.z_coord,
					node->as.coords.x_coord, y_width, z_height,
					node->as.coords.x_coord, node->as.coords.y_coord, z_height,

					// "left" face
					node->as.coords.x_coord, node->as.coords.y_coord, node->as.coords.z_coord,
					x_width, node->as.coords.y_coord, node->as.coords.z_coord,
					x_width, node->as.coords.y_coord, z_height,
					node->as.coords.x_coord, node->as.coords.y_coord, z_height,

					// "right" face
					node->as.coords.x_coord, y_width, node->as.coords.z_coord,
					x_width, y_width, node->as.coords.z_coord,
					x_width, y_width, z_height,
					node->as.coords.x_coord, y_width, z_height
				};

	// set color and alpha transparency
	if (!viz->transparent) {
		node->as.coords.color[3] = 255;
	}
	glColor4ubv(node->as.coords.color);

	// render our arrays of vertices with given normals
	glNormalPointer(GL_FLOAT, 0, viz->quads_normal_3d);
	glVertexPointer(3, GL_FLOAT, 0, quads_v);
	glDrawArrays(GL_QUADS, 0, 20);


	// draw labels if necessary
	if (viz->draw_labels) {
		glDisable(GL_LIGHTING);

		gchar *node_label = g_strdup_printf("%d", node->as.as_num);

		// output the nodes label
		glColor3ubv(node->as.coords.color);
		glRasterPos3f(node->as.coords.x_coord + (node->as.coords.x_width / 2), node->as.coords.y_coord + (node->as.coords.y_width / 2), z_height + Z_LABEL_ADJ);
		glListBase(viz->font_list_3d);
		glCallLists(strlen(node_label), GL_UNSIGNED_BYTE, node_label);

		g_free(node_label);

		if (!viz->disable_lighting) {
			glEnable(GL_LIGHTING);
		}
	}

	return FALSE;
}


void handler_as_render_3d_scene(void)
{
	// white wire outline
	glCallList(WIRE_CUBE_CALL_LIST);

	// output our coord labels
	glCallList(LABELS_3D_AS_CALL_LIST);

	// output our z-axis labels
	glDisable(GL_LIGHTING);

	gint max, diff;
	if (viz->disable_max_threshold) {
		max = gtk_spin_button_get_value(GTK_SPIN_BUTTON(viz->prefs_max_threshold_value));
	} else {
		max = viz->threshold_max;
	}
	diff = max - viz->threshold_min;

	gchar *label0 = g_strdup_printf(" %d", viz->threshold_min);
	gchar *label1 = g_strdup_printf(" %d", viz->threshold_min + diff * 1/4);
	gchar *label2 = g_strdup_printf(" %d  FLOWS", viz->threshold_min + diff * 1/2);
	gchar *label3 = g_strdup_printf(" %d", viz->threshold_min + diff * 3/4);
	gchar *label4 = g_strdup_printf(" %d", max);

	// output our labels
	glColor3f(1.0, 1.0, 1.0);

	glRasterPos3f(-1.0, 1.0, -1.0);
	glListBase(viz->font_list_3d);

	glCallLists(strlen(label0), GL_UNSIGNED_BYTE, label0);

	glRasterPos3f(-1.0, 1.0, -0.5);
	glListBase(viz->font_list_3d);
	glCallLists(strlen(label1), GL_UNSIGNED_BYTE, label1);

	glRasterPos3f(-1.0, 1.0, 0.0);
	glListBase(viz->font_list_3d);
	glCallLists(strlen(label2), GL_UNSIGNED_BYTE, label2);

	glRasterPos3f(-1.0, 1.0, 0.5);
	glListBase(viz->font_list_3d);
	glCallLists(strlen(label3), GL_UNSIGNED_BYTE, label3);

	glRasterPos3f(-1.0, 1.0, 1.0);
	glListBase(viz->font_list_3d);
	glCallLists(strlen(label4), GL_UNSIGNED_BYTE, label4);

	g_free(label0);
	g_free(label1);
	g_free(label2);
	g_free(label3);
	g_free(label4);

	glEnable(GL_LIGHTING);

	// disable the lighting if necessary for a performance increase
	if (viz->disable_lighting) {
		glDisable(GL_LIGHTING);
	}

	// give the wire outline a solid floor
	glCallList(WHITE_3D_SQUARE_CALL_LIST);

	// render all our nodes!
	g_tree_foreach(viz->nodes, handler_as_render_3d_node, NULL);	

	if (!viz->transparent) {
		glCallList(FLAMINGO_LOGO_CALL_LIST);
	}
}


gint handler_as_compare_nodes(tree_node_t *node_a, tree_node_t *node_b)
{
	if (node_a->as.as_num < node_b->as.as_num) {
		return -1;
	} else if (node_a->as.as_num > node_b->as.as_num) {
		return 1;
	} else {
		return 0;
	}
}


tree_node_t *handler_as_lookup_node_from_string(gchar *node_str)
{
	tree_node_t *node = g_new0(tree_node_t, 1);

	node->as.as_num = atoi(node_str);

	return node;
}


void handler_as_input_process_nodes(GTree *tree, data_record_t *data, guint len)
{
	// loop through all received entries
	data_record_t *curr_record;
	guint i, num_entries = len / sizeof(data_record_t);

	for (i = 0; i < num_entries; ++i) {
		curr_record = (data_record_t *) data + i;

		// allocate mem for a new node
		tree_node_t *node = g_new0(tree_node_t, 1);

		// assign the node's vars
		node->as.as_num = g_ntohl(curr_record->address);
		node->volume = g_ntohl(curr_record->count);

		// assign our RGB colors of the node as specified by the server
		node->as.coords.color[0] = curr_record->red;
		node->as.coords.color[1] = curr_record->green;
		node->as.coords.color[2] = curr_record->blue;

		// set the transparency
		node->as.coords.color[3] = 100;

		// calculate the quadtree coordinates
		node->as.coords.x_coord = 0.0;
		node->as.coords.y_coord = 0.0;

		// calculate width of the node
		GLfloat node_width = COORD_SIZE / (pow(2.0, 8));

		// loop to find the coordinates where our quadtree begins
		gint i, loops = 8;
		GLfloat adjustment = COORD_SIZE;
		for (i = 0; i < loops; ++i) {
			adjustment /= 2.0;

			// for a COORD_SIZE x COORD_SIZE grid
			switch (SHIFT(node->as.as_num, ((7-i)*2), 0x03)) {
				// 00
				case 0:
					// do nothing
				break;

				// 01
				case 1:
					node->as.coords.y_coord += adjustment;
				break;

				// 10
				case 2:
					node->as.coords.x_coord += adjustment;
				break;

				// 11
				case 3:
					node->as.coords.x_coord += adjustment;
					node->as.coords.y_coord += adjustment;
				break;
			}
		}

		// translate coords and dimensions onto -1 through 1 coord system
		node->as.coords.x_coord = (GLfloat) (node->as.coords.x_coord * STANDARD_UNIT_SIZE) - 1.0;
		node->as.coords.y_coord = (GLfloat) (node->as.coords.y_coord * STANDARD_UNIT_SIZE) - 1.0;
		node->as.coords.z_coord = (GLfloat) -1.0;
		node->as.coords.x_width = (GLfloat) (node_width * STANDARD_UNIT_SIZE);
		node->as.coords.y_width = (GLfloat) (node_width * STANDARD_UNIT_SIZE);

		// add the node to our GTree
		g_tree_insert(tree, node, NULL);
	}
}


gchar *handler_as_get_node_description(tree_node_t *node)
{
	gchar *desc;

	if (node->as.as_num >= viz->asnames->len) {
		desc = "Unknown AS";
	} else {
		desc = g_ptr_array_index(viz->asnames, node->as.as_num);

		if (desc == NULL) {
			desc = "Unknown AS";
		}
	}

	return g_strdup(desc);
}


gchar *handler_as_get_node_title(void)
{
	return (g_strdup("AS Number"));
}


gchar *handler_as_get_node_value(tree_node_t *node)
{
	return (g_strdup_printf("%d", node->as.as_num));
}
