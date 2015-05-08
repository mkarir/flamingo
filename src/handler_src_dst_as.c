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


void handler_src_dst_as_init_interface()
{
	// enable transparency by default
	viz->transparent = TRUE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(viz->transparent_check_box), TRUE);

	// enable lighting by default
	viz->disable_lighting = FALSE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(viz->lighting_check_box), FALSE);

	// disable labels by default
	viz->draw_labels = FALSE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(viz->labels_check_box), FALSE);

	// enable/disable appropriate scaling bars
	gtk_widget_set_sensitive(viz->line_scale_range, TRUE);
	gtk_widget_set_sensitive(viz->line_scale_label, TRUE);
	gtk_widget_set_sensitive(viz->slice_scale_range, FALSE);
	gtk_widget_set_sensitive(viz->slice_scale_label, FALSE);
	
	// disable other ranges
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


coord_t *handler_src_dst_as_get_node_coords(tree_node_t *node)
{
	return (&node->src_dst_as.src_coords);
}


gboolean handler_src_dst_as_check_2d_click(tree_node_t *node, GLfloat x_pos, GLfloat y_pos)
{
	// check if the click was within the boundries of the src node
	if (x_pos >= node->src_dst_as.src_coords.y_coord
			&& x_pos <= node->src_dst_as.src_coords.y_coord + node->src_dst_as.src_coords.x_width
			&& y_pos >= node->src_dst_as.src_coords.z_coord
			&& y_pos <= node->src_dst_as.src_coords.z_coord + node->src_dst_as.src_coords.y_width) {
		return TRUE;
	}

	return FALSE;
}


gboolean handler_src_dst_as_check_visibility(tree_node_t *node)
{
	return (node->volume >= viz->threshold_min && node->volume <= viz->threshold_max);
}


static inline gboolean handler_src_dst_as_render_2d_node(gpointer node_in, gpointer null, gpointer data)
{
	tree_node_t *node = node_in;

	// return if the node is not within the thresholds
	if (!viz->handler.check_visibility(node)) {
		return FALSE;
	}

	// precalculate these to avoid redundant calculations
	GLfloat x_width = node->src_dst_as.src_coords.y_coord + node->src_dst_as.src_coords.x_width;
	GLfloat y_width = node->src_dst_as.src_coords.z_coord + node->src_dst_as.src_coords.y_width;

	GLfloat quad_src_dst_as_v[] = {
					node->src_dst_as.src_coords.y_coord, node->src_dst_as.src_coords.z_coord,
					x_width, node->src_dst_as.src_coords.z_coord,
					node->src_dst_as.src_coords.y_coord, y_width,
					x_width, y_width
				 };

	// set color and transparency
	if (!viz->transparent) {
		node->src_dst_as.src_coords.color[3] = 255;
	}
	glColor4ubv(node->src_dst_as.src_coords.color);

	// render our arrays of vertices
	glVertexPointer(2, GL_FLOAT, 0, quad_src_dst_as_v);
	glDrawArrays(GL_QUAD_STRIP, 0, 4);

	return FALSE;
}


void handler_src_dst_as_render_2d_scene(void)
{
	// draw a square outline
	glCallList(WHITE_2D_SQUARE_CALL_LIST);

	// draw our labels
	glCallList(LABELS_2D_AS_CALL_LIST);

	// render all our nodes!
	g_tree_foreach(viz->nodes, handler_src_dst_as_render_2d_node, NULL);
}


static inline gboolean handler_src_dst_as_render_3d_node(gpointer node_in, gpointer null, gpointer data)
{
	tree_node_t *node = node_in;

	// only draw the node if it is between the thresholds
	if (!viz->handler.check_visibility(node)) {
		return FALSE;
	}

	// precalculate these to avoid redundant calculations
	GLfloat src_y_width = node->src_dst_as.src_coords.y_coord + node->src_dst_as.src_coords.x_width;
	GLfloat src_z_width = node->src_dst_as.src_coords.z_coord + node->src_dst_as.src_coords.y_width;
	GLfloat dst_y_width = node->src_dst_as.dst_coords.y_coord + node->src_dst_as.dst_coords.x_width;
	GLfloat dst_z_width = node->src_dst_as.dst_coords.z_coord + node->src_dst_as.dst_coords.y_width;

	// define our array of quadrilaterals
	GLfloat quads_v[] =     {
					// node on left side of cube cube
					-1.0 + Z_HEIGHT_ADJ, node->src_dst_as.src_coords.y_coord, node->src_dst_as.src_coords.z_coord,
					-1.0 + Z_HEIGHT_ADJ, src_y_width, node->src_dst_as.src_coords.z_coord,
					-1.0 + Z_HEIGHT_ADJ, src_y_width, src_z_width,
					-1.0 + Z_HEIGHT_ADJ, node->src_dst_as.src_coords.y_coord, src_z_width,

					// node on right side of cube cube
					1.0 - Z_HEIGHT_ADJ, node->src_dst_as.dst_coords.y_coord, node->src_dst_as.dst_coords.z_coord,
					1.0 - Z_HEIGHT_ADJ, dst_y_width, node->src_dst_as.dst_coords.z_coord,
					1.0 - Z_HEIGHT_ADJ, dst_y_width, dst_z_width,
					1.0 - Z_HEIGHT_ADJ, node->src_dst_as.dst_coords.y_coord, dst_z_width
				};

	// set color and alpha transparency
	if (!viz->transparent) {
		node->src_dst_as.src_coords.color[3] = 255;
		node->src_dst_as.dst_coords.color[3] = 255;
	}

	// render our source node on the left side of the cube
	glColor4ubv(node->src_dst_as.src_coords.color);
	glNormalPointer(GL_FLOAT, 0, viz->quads_normal_3d + 12);
	glVertexPointer(3, GL_FLOAT, 0, quads_v);
	glDrawArrays(GL_QUADS, 0, 4);

	// render our dst node on the right side of the cube
	glColor4ubv(node->src_dst_as.dst_coords.color);
	glNormalPointer(GL_FLOAT, 0, viz->quads_normal_3d + 24);
	glVertexPointer(3, GL_FLOAT, 0, quads_v + 12);
	glDrawArrays(GL_QUADS, 0, 4);


	// draw our line connecting the src to dst
	glDisable(GL_LIGHTING);
	glLineWidth((node->volume * gtk_range_get_value(GTK_RANGE(viz->line_scale_range)) + .01) / 5000.0);
	glBegin(GL_LINES);
		glColor3ubv(node->src_dst_as.src_coords.color);
		glVertex3f(-1.0 + Z_HEIGHT_ADJ, node->src_dst_as.src_coords.y_coord + (node->src_dst_as.src_coords.x_width / 2), node->src_dst_as.src_coords.z_coord + (node->src_dst_as.src_coords.y_width / 2));
		glVertex3f(1.0 - Z_HEIGHT_ADJ, node->src_dst_as.dst_coords.y_coord + (node->src_dst_as.dst_coords.x_width / 2), node->src_dst_as.dst_coords.z_coord + (node->src_dst_as.dst_coords.y_width / 2));
	glEnd();
	if (!viz->disable_lighting) {
		glEnable(GL_LIGHTING);
	}

	// draw labels if necessary
	if (viz->draw_labels) {
		glDisable(GL_LIGHTING);

		gchar *src_node_label = g_strdup_printf("%d", node->src_dst_as.src_as);
		gchar *dst_node_label = g_strdup_printf("%d", node->src_dst_as.dst_as);

		// output the source nodes label
		glColor3ubv(node->src_dst_as.src_coords.color);
		glRasterPos3f(-1.0 + (Z_HEIGHT_ADJ * 10), node->src_dst_as.src_coords.y_coord + (node->src_dst_as.src_coords.x_width / 2), node->src_dst_as.src_coords.z_coord + (node->src_dst_as.src_coords.y_width / 2) + Z_LABEL_ADJ);
		glListBase(viz->font_list_3d);
		glCallLists(strlen(src_node_label), GL_UNSIGNED_BYTE, src_node_label);

		// output the dst nodes label
		glColor3ubv(node->src_dst_as.dst_coords.color);
		glRasterPos3f(1.0 - (Z_HEIGHT_ADJ * 10), node->src_dst_as.dst_coords.y_coord + (node->src_dst_as.dst_coords.x_width / 2), node->src_dst_as.dst_coords.z_coord + (node->src_dst_as.dst_coords.y_width / 2) + Z_LABEL_ADJ);
		glListBase(viz->font_list_3d);
		glCallLists(strlen(dst_node_label), GL_UNSIGNED_BYTE, dst_node_label);

		g_free(src_node_label);
		g_free(dst_node_label);

		if (!viz->disable_lighting) {
			glEnable(GL_LIGHTING);
		}
	}

	return FALSE;
}


void handler_src_dst_as_render_3d_scene(void)
{
	// draw our wire cube outline
	glCallList(WIRE_CUBE_CALL_LIST);

	// output our coord labels
	glCallList(LABELS_3D_SRC_DEST_AS_CALL_LIST);

	// disable the lighting if necessary for a performance increase
	if (viz->disable_lighting) {
		glDisable(GL_LIGHTING);
	}

	// give the wire outline two solid sides
	glCallList(WHITE_3D_TWO_SIDED_CALL_LIST);

	// render all our nodes!
	g_tree_foreach(viz->nodes, handler_src_dst_as_render_3d_node, NULL);	
}


gint handler_src_dst_as_compare_nodes(tree_node_t *node_a, tree_node_t *node_b)
{
	guint src_as_a = node_a->src_dst_as.src_as;
	guint src_as_b = node_b->src_dst_as.src_as;

	// check the src as first
	if (src_as_a < src_as_b) {
		return -1;
	} else if (src_as_a > src_as_b) {
		return 1;
	}

	guint dst_as_a = node_a->src_dst_as.dst_as;
	guint dst_as_b = node_b->src_dst_as.dst_as;

	// if we hit here, we know src as are equal, so compare dst as
	if (dst_as_a < dst_as_b) {
		return -1;
	} else if (dst_as_a > dst_as_b) {
		return 1;
	}

	// if we hit here, we know both the src and dst as are equal
	return 0;
}


tree_node_t *handler_src_dst_as_lookup_node_from_string(gchar *node_str)
{
	tree_node_t *node;
	gchar **line_split;

	node = g_new0(tree_node_t, 1);

	// split up the two as numbers
	line_split = g_strsplit(node_str, "->", 2);

	line_split[0] = g_strstrip(line_split[0]);
	line_split[1] = g_strstrip(line_split[1]);

	node->src_dst_as.src_as = atoi(line_split[0]);
	node->src_dst_as.dst_as = atoi(line_split[1]);

	g_strfreev(line_split);

	return node;
}


void handler_src_dst_as_input_process_nodes(GTree *tree, data_record_t *data, guint len)
{
	// loop through all received entries
	data_record_t *curr_record;
	guint i, num_entries = len / sizeof(data_record_t);
	for (i = 0; i < num_entries; ++i) {
		curr_record = (data_record_t *) data + i;

		// allocate mem for a new node
		tree_node_t *node = g_new0(tree_node_t, 1);

		// assign the node's vars
		node->src_dst_as.src_as = g_ntohl(curr_record->address);
		node->src_dst_as.dst_as = g_ntohl(curr_record->dstaddr);
		node->volume = g_ntohl(curr_record->count);

		// assign our RGB colors of the node as specified by the server
		node->src_dst_as.src_coords.color[0] = curr_record->red;
		node->src_dst_as.src_coords.color[1] = curr_record->green;
		node->src_dst_as.src_coords.color[2] = curr_record->blue;
		node->src_dst_as.dst_coords.color[0] = curr_record->red2;
		node->src_dst_as.dst_coords.color[1] = curr_record->green2;
		node->src_dst_as.dst_coords.color[2] = curr_record->blue2;

		// transparency values
		node->src_dst_as.src_coords.color[3] = 127;
		node->src_dst_as.dst_coords.color[3] = 127;

		// calculate the quadtree coordinates
		node->src_dst_as.src_coords.y_coord = 0.0;
		node->src_dst_as.src_coords.z_coord = 0.0;
		node->src_dst_as.dst_coords.y_coord = 0.0;
		node->src_dst_as.dst_coords.z_coord = 0.0;


		// loop to find the coordinates where our quadtree begins
		gint i, loops = 8;
		GLfloat adjustment = COORD_SIZE;
		for (i = 0; i < loops; ++i) {
			adjustment /= 2.0;

			// for a COORD_SIZE x COORD_SIZE grid
			switch (SHIFT(node->src_dst_as.src_as, ((7-i)*2), 0x03)) {
				// 00
				case 0:
					// do nothing
				break;

				// 01
				case 1:
					node->src_dst_as.src_coords.z_coord += adjustment;
				break;

				// 10
				case 2:
					node->src_dst_as.src_coords.y_coord += adjustment;
				break;

				// 11
				case 3:
					node->src_dst_as.src_coords.y_coord += adjustment;
					node->src_dst_as.src_coords.z_coord += adjustment;
				break;
			}
		}

		// loop to find the coordinates where our quadtree begins
		loops = 8;
		adjustment = COORD_SIZE;
		for (i = 0; i < loops; ++i) {
			adjustment /= 2.0;

			// for a COORD_SIZE x COORD_SIZE grid
			switch (SHIFT(node->src_dst_as.dst_as, ((7-i)*2), 0x03)) {
				// 00
				case 0:
					// do nothing
				break;

				// 01
				case 1:
					node->src_dst_as.dst_coords.z_coord += adjustment;
				break;

				// 10
				case 2:
					node->src_dst_as.dst_coords.y_coord += adjustment;
				break;

				// 11
				case 3:
					node->src_dst_as.dst_coords.y_coord += adjustment;
					node->src_dst_as.dst_coords.z_coord += adjustment;
				break;
			}
		}


		// calculate width of the node
		GLfloat node_width = COORD_SIZE / (pow(2.0, 8));

		// translate src coords and dimensions onto the left side of the cube
		node->src_dst_as.src_coords.y_coord = (GLfloat) (node->src_dst_as.src_coords.y_coord * STANDARD_UNIT_SIZE) - 1.0;
		node->src_dst_as.src_coords.z_coord = (GLfloat) (node->src_dst_as.src_coords.z_coord * STANDARD_UNIT_SIZE) - 1.0;
		node->src_dst_as.src_coords.x_width = (GLfloat) (node_width * STANDARD_UNIT_SIZE);
		node->src_dst_as.src_coords.y_width = (GLfloat) (node_width * STANDARD_UNIT_SIZE);

		// translate dst coords and dimensions onto the right side of the cube
		node->src_dst_as.dst_coords.y_coord = (GLfloat) (node->src_dst_as.dst_coords.y_coord * STANDARD_UNIT_SIZE) - 1.0;
		node->src_dst_as.dst_coords.z_coord = (GLfloat) (node->src_dst_as.dst_coords.z_coord * STANDARD_UNIT_SIZE) - 1.0;
		node->src_dst_as.dst_coords.x_width = (GLfloat) (node_width * STANDARD_UNIT_SIZE);
		node->src_dst_as.dst_coords.y_width = (GLfloat) (node_width * STANDARD_UNIT_SIZE);

		// add the node to our GTree
		g_tree_insert(tree, node, NULL);
	}
}


gchar *handler_src_dst_as_get_node_description(tree_node_t *node)
{	
	return (strdup("N/A"));
}


gchar *handler_src_dst_as_get_node_title(void)
{
	return (g_strdup("Src AS -> Dst AS"));
}


gchar *handler_src_dst_as_get_node_value(tree_node_t *node)
{
	return (g_strdup_printf("%d -> %d", node->src_dst_as.src_as, node->src_dst_as.dst_as));
}
