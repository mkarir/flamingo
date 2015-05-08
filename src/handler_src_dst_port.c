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

#define NUM_PORTS 65536


void handler_src_dst_port_init_interface()
{
	// enable transparency by default
	viz->transparent = TRUE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(viz->transparent_check_box), TRUE);

	// disable lighting by default
	viz->disable_lighting = TRUE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(viz->lighting_check_box), TRUE);

	// disable labels by default
	viz->draw_labels = FALSE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(viz->labels_check_box), FALSE);

	// enable/disable appropriate scaling bars
	gtk_widget_set_sensitive(viz->line_scale_range, TRUE);
	gtk_widget_set_sensitive(viz->line_scale_label, TRUE);
	gtk_widget_set_sensitive(viz->slice_scale_range, TRUE);
	gtk_widget_set_sensitive(viz->slice_scale_label, TRUE);

	// enable our 2 sets of ranges and labels
	gtk_widget_set_sensitive(viz->port1_min_range, TRUE);
	gtk_widget_set_sensitive(viz->port1_max_range, TRUE);
	gtk_widget_set_sensitive(viz->port1_min_label, TRUE);
	gtk_widget_set_sensitive(viz->port1_max_label, TRUE);
	gtk_widget_set_sensitive(viz->port2_min_range, TRUE);
	gtk_widget_set_sensitive(viz->port2_max_range, TRUE);
	gtk_widget_set_sensitive(viz->port2_min_label, TRUE);
	gtk_widget_set_sensitive(viz->port2_max_label, TRUE);

	// set the correct labels on those ranges
	gtk_label_set_label(GTK_LABEL(viz->port1_min_label), "Min Src Port:");
	gtk_label_set_label(GTK_LABEL(viz->port1_max_label), "Max Src Port:");
	gtk_label_set_label(GTK_LABEL(viz->port2_min_label), "Min Dst Port:");
	gtk_label_set_label(GTK_LABEL(viz->port2_max_label), "Max Dst Port:");
}


coord_t *handler_src_dst_port_get_node_coords(tree_node_t *node)
{
	return (&node->src_dst_port.src_coords);
}


gboolean handler_src_dst_port_check_2d_click(tree_node_t *node, GLfloat x_pos, GLfloat y_pos)
{
	// check if the click was within the boundries of the src node
	if (x_pos >= node->src_dst_port.src_coords.x_coord
			&& x_pos <= node->src_dst_port.src_coords.x_coord + node->src_dst_port.src_coords.x_width
			&& y_pos >= node->src_dst_port.src_coords.y_coord
			&& y_pos <= node->src_dst_port.src_coords.y_coord + node->src_dst_port.src_coords.y_width) {
		return TRUE;
	}

	// check if the click was within the boundries of the dst node
	if (x_pos >= node->src_dst_port.dst_coords.x_coord
			&& x_pos <= node->src_dst_port.dst_coords.x_coord + node->src_dst_port.dst_coords.x_width
			&& y_pos >= node->src_dst_port.dst_coords.y_coord
			&& y_pos <= node->src_dst_port.dst_coords.y_coord + node->src_dst_port.dst_coords.y_width) {
		return TRUE;
	}

	return FALSE;
}


gboolean handler_src_dst_port_check_visibility(tree_node_t *node)
{
	return (node->volume >= viz->threshold_min &&
		node->volume <= viz->threshold_max &&
		node->src_dst_port.src_port >= viz->port1_min &&
		node->src_dst_port.src_port <= viz->port1_max &&
		node->src_dst_port.dst_port >= viz->port2_min &&
		node->src_dst_port.dst_port <= viz->port2_max);
}


static inline gboolean handler_src_dst_port_render_2d_node(gpointer node_in, gpointer null, gpointer data)
{
	tree_node_t *node = node_in;

	// return if the node is not within the thresholds
	if (!viz->handler.check_visibility(node)) {
		return FALSE;
	}

	// precalculate these to avoid redundant calculations
	GLfloat src_x_width = node->src_dst_port.src_coords.x_coord + node->src_dst_port.src_coords.x_width;
	GLfloat src_y_width = node->src_dst_port.src_coords.y_coord + node->src_dst_port.src_coords.y_width;
	GLfloat dst_x_width = node->src_dst_port.dst_coords.x_coord + node->src_dst_port.dst_coords.x_width;
	GLfloat dst_y_width = node->src_dst_port.dst_coords.y_coord + node->src_dst_port.dst_coords.y_width;


	GLfloat src_quad_strip_v[] = {
					node->src_dst_port.src_coords.x_coord, node->src_dst_port.src_coords.y_coord,
					src_x_width, node->src_dst_port.src_coords.y_coord,
					node->src_dst_port.src_coords.x_coord, src_y_width,
					src_x_width, src_y_width
				 };

	GLfloat dst_quad_strip_v[] = {
					node->src_dst_port.dst_coords.x_coord, node->src_dst_port.dst_coords.y_coord,
					dst_x_width, node->src_dst_port.dst_coords.y_coord,
					node->src_dst_port.dst_coords.x_coord, dst_y_width,
					dst_x_width, dst_y_width
				 };


	// set color and transparency
	if (!viz->transparent) {
		node->src_dst_port.src_coords.color[3] = 255;
		node->src_dst_port.dst_coords.color[3] = 255;
	}

	// render our arrays of source vertices
	glColor4ubv(node->src_dst_port.src_coords.color);
	glVertexPointer(2, GL_FLOAT, 0, src_quad_strip_v);
	glDrawArrays(GL_QUAD_STRIP, 0, 4);

	// render our arrays of dst vertices
	glColor4ubv(node->src_dst_port.dst_coords.color);
	glVertexPointer(2, GL_FLOAT, 0, dst_quad_strip_v);
	glDrawArrays(GL_QUAD_STRIP, 0, 4);

	return FALSE;
}


void handler_src_dst_port_render_2d_scene(void)
{
	guint i;
	GLfloat unit_size = CUBE_SIZE / (GLfloat) (viz->src_dst_port_bound.max_x_coord - viz->src_dst_port_bound.min_x_coord);

	// draw a square outline
	glCallList(GREY_2D_SQUARE_CALL_LIST);

	// draw a white square representing src range over our grey base
	for (i = 0; i < viz->num_src_ranges; ++i) {
		GLfloat x_coord = (GLfloat) ((viz->src_ranges[i].x_coord - viz->src_dst_port_bound.min_x_coord) * unit_size) - 1.0;
		GLfloat y_coord = (GLfloat) ((viz->src_ranges[i].y_coord - viz->src_dst_port_bound.min_y_coord) * unit_size) - 1.0;
		GLfloat width = (GLfloat) (viz->src_ranges[i].width * unit_size);

		glBegin(GL_QUAD_STRIP);
		glNormal3f(0.0, 0.0, 1.0);
		glColor3ub(255, 255, 255);

		glVertex2f(x_coord, y_coord);
		glVertex2f(x_coord, y_coord + width);
		glVertex2f(x_coord + width, y_coord);
		glVertex2f(x_coord + width, y_coord + width);

		glEnd();
	}

	// draw a white square representing dst range over our grey base
	for (i = 0; i < viz->num_dst_ranges; ++i) {
		GLfloat x_coord = (GLfloat) ((viz->dst_ranges[i].x_coord - viz->src_dst_port_bound.min_x_coord) * unit_size) - 1.0;
		GLfloat y_coord = (GLfloat) ((viz->dst_ranges[i].y_coord - viz->src_dst_port_bound.min_y_coord) * unit_size) - 1.0;
		GLfloat width = (GLfloat) (viz->dst_ranges[i].width * unit_size);

		glBegin(GL_QUAD_STRIP);
		glNormal3f(0.0, 0.0, 1.0);
		glColor3ub(255, 255, 255);

		glVertex2f(x_coord, y_coord);
		glVertex2f(x_coord, y_coord + width);
		glVertex2f(x_coord + width, y_coord);
		glVertex2f(x_coord + width, y_coord + width);

		glEnd();
	}

	// derive address based on the quadtree coordinates of our bounding box
	struct in_addr low_addr;
	struct in_addr high_addr;
	range_reverse_quadtree(&viz->src_dst_port_bound, &low_addr, &high_addr);

	gchar *low_str = g_strdup_printf(" %s", inet_ntoa(low_addr));
	gchar *high_str = g_strdup_printf(" %s", inet_ntoa(high_addr));

	// lower address range label
	glColor3f(1.0, 1.0, 1.0);
	glRasterPos2f(-1.0, -1.0);
	glListBase(viz->font_list_2d);
	glCallLists(strlen(low_str), GL_UNSIGNED_BYTE, low_str);

	// higher address range label
	glRasterPos2f(1.0, 1.0);
	glListBase(viz->font_list_2d);
	glCallLists(strlen(high_str), GL_UNSIGNED_BYTE, high_str);

	g_free(low_str);
	g_free(high_str);

	// render all our nodes!
	g_tree_foreach(viz->nodes, handler_src_dst_port_render_2d_node, NULL);
}


static inline gboolean handler_src_dst_port_render_3d_node(gpointer node_in, gpointer null, gpointer data)
{
	tree_node_t *node = node_in;

	// only draw the node if it is between the thresholds
	if (!viz->handler.check_visibility(node)) {
		return FALSE;
	}

	// determine the z_coord based on the current port range
	gint range_min = MIN(viz->port1_min, viz->port2_min);
	gint range_max = MAX(viz->port1_max, viz->port2_max);
	gint range_diff = range_max - range_min;
	node->src_dst_port.src_coords.z_coord = (GLfloat) ((GLfloat) ((COORD_SIZE / (GLfloat) (range_diff))) * (node->src_dst_port.src_port - range_min) * STANDARD_UNIT_SIZE) - 1.0;
	node->src_dst_port.dst_coords.z_coord = (GLfloat) ((GLfloat) ((COORD_SIZE / (GLfloat) (range_diff))) * (node->src_dst_port.dst_port - range_min) * STANDARD_UNIT_SIZE) - 1.0;

	node->src_dst_port.src_coords.height = (GLfloat) gtk_range_get_value(GTK_RANGE(viz->slice_scale_range)) * STANDARD_UNIT_SIZE;
	node->src_dst_port.dst_coords.height = (GLfloat) gtk_range_get_value(GTK_RANGE(viz->slice_scale_range)) * STANDARD_UNIT_SIZE;

	// precalculate these to avoid redundant calculations
	GLfloat src_x_width = node->src_dst_port.src_coords.x_coord + node->src_dst_port.src_coords.x_width;
	GLfloat src_y_width = node->src_dst_port.src_coords.y_coord + node->src_dst_port.src_coords.y_width;
	GLfloat src_z_height = node->src_dst_port.src_coords.z_coord + node->src_dst_port.src_coords.height;
	GLfloat dst_x_width = node->src_dst_port.dst_coords.x_coord + node->src_dst_port.dst_coords.x_width;
	GLfloat dst_y_width = node->src_dst_port.dst_coords.y_coord + node->src_dst_port.dst_coords.y_width;
	GLfloat dst_z_height = node->src_dst_port.dst_coords.z_coord + node->src_dst_port.dst_coords.height;

	// define our array of src/dst quadrilaterals
	GLfloat src_quads_v[] =     {
					// top face
					node->src_dst_port.src_coords.x_coord, node->src_dst_port.src_coords.y_coord, src_z_height,
					src_x_width, node->src_dst_port.src_coords.y_coord, src_z_height,
					src_x_width, src_y_width, src_z_height,
					node->src_dst_port.src_coords.x_coord, src_y_width, src_z_height,

					// "front" face
					src_x_width, node->src_dst_port.src_coords.y_coord, node->src_dst_port.src_coords.z_coord,
					src_x_width, src_y_width, node->src_dst_port.src_coords.z_coord,
					src_x_width, src_y_width, src_z_height,
					src_x_width, node->src_dst_port.src_coords.y_coord, src_z_height,

					// "back" face
					node->src_dst_port.src_coords.x_coord, node->src_dst_port.src_coords.y_coord, node->src_dst_port.src_coords.z_coord,
					node->src_dst_port.src_coords.x_coord, src_y_width, node->src_dst_port.src_coords.z_coord,
					node->src_dst_port.src_coords.x_coord, src_y_width, src_z_height,
					node->src_dst_port.src_coords.x_coord, node->src_dst_port.src_coords.y_coord, src_z_height,

					// "left" face
					node->src_dst_port.src_coords.x_coord, node->src_dst_port.src_coords.y_coord, node->src_dst_port.src_coords.z_coord,
					src_x_width, node->src_dst_port.src_coords.y_coord, node->src_dst_port.src_coords.z_coord,
					src_x_width, node->src_dst_port.src_coords.y_coord, src_z_height,
					node->src_dst_port.src_coords.x_coord, node->src_dst_port.src_coords.y_coord, src_z_height,

					// "right" face
					node->src_dst_port.src_coords.x_coord, src_y_width, node->src_dst_port.src_coords.z_coord,
					src_x_width, src_y_width, node->src_dst_port.src_coords.z_coord,
					src_x_width, src_y_width, src_z_height,
					node->src_dst_port.src_coords.x_coord, src_y_width, src_z_height,

					// bottom square at base of cube
					node->src_dst_port.src_coords.x_coord, node->src_dst_port.src_coords.y_coord, -1.0 + Z_HEIGHT_ADJ,
					src_x_width, node->src_dst_port.src_coords.y_coord, -1.0 + Z_HEIGHT_ADJ,
					src_x_width, src_y_width, -1.0 + Z_HEIGHT_ADJ,
					node->src_dst_port.src_coords.x_coord, src_y_width, -1.0 + Z_HEIGHT_ADJ
				};

	GLfloat dst_quads_v[] =     {
					// top face
					node->src_dst_port.dst_coords.x_coord, node->src_dst_port.dst_coords.y_coord, dst_z_height,
					dst_x_width, node->src_dst_port.dst_coords.y_coord, dst_z_height,
					dst_x_width, dst_y_width, dst_z_height,
					node->src_dst_port.dst_coords.x_coord, dst_y_width, dst_z_height,

					// "front" face
					dst_x_width, node->src_dst_port.dst_coords.y_coord, node->src_dst_port.dst_coords.z_coord,
					dst_x_width, dst_y_width, node->src_dst_port.dst_coords.z_coord,
					dst_x_width, dst_y_width, dst_z_height,
					dst_x_width, node->src_dst_port.dst_coords.y_coord, dst_z_height,

					// "back" face
					node->src_dst_port.dst_coords.x_coord, node->src_dst_port.dst_coords.y_coord, node->src_dst_port.dst_coords.z_coord,
					node->src_dst_port.dst_coords.x_coord, dst_y_width, node->src_dst_port.dst_coords.z_coord,
					node->src_dst_port.dst_coords.x_coord, dst_y_width, dst_z_height,
					node->src_dst_port.dst_coords.x_coord, node->src_dst_port.dst_coords.y_coord, dst_z_height,

					// "left" face
					node->src_dst_port.dst_coords.x_coord, node->src_dst_port.dst_coords.y_coord, node->src_dst_port.dst_coords.z_coord,
					dst_x_width, node->src_dst_port.dst_coords.y_coord, node->src_dst_port.dst_coords.z_coord,
					dst_x_width, node->src_dst_port.dst_coords.y_coord, dst_z_height,
					node->src_dst_port.dst_coords.x_coord, node->src_dst_port.dst_coords.y_coord, dst_z_height,

					// "right" face
					node->src_dst_port.dst_coords.x_coord, dst_y_width, node->src_dst_port.dst_coords.z_coord,
					dst_x_width, dst_y_width, node->src_dst_port.dst_coords.z_coord,
					dst_x_width, dst_y_width, dst_z_height,
					node->src_dst_port.dst_coords.x_coord, dst_y_width, dst_z_height,

					// bottom square at base of cube
					node->src_dst_port.dst_coords.x_coord, node->src_dst_port.dst_coords.y_coord, -1.0 + Z_HEIGHT_ADJ,
					dst_x_width, node->src_dst_port.dst_coords.y_coord, -1.0 + Z_HEIGHT_ADJ,
					dst_x_width, dst_y_width, -1.0 + Z_HEIGHT_ADJ,
					node->src_dst_port.dst_coords.x_coord, dst_y_width, -1.0 + Z_HEIGHT_ADJ
				};

	// set color and alpha transparency
	if (!viz->transparent) {
		node->src_dst_port.src_coords.color[3] = 255;
		node->src_dst_port.dst_coords.color[3] = 255;
	}

	// render our arrays of src vertices with given normals
	glColor4ubv(node->src_dst_port.src_coords.color);
	glNormalPointer(GL_FLOAT, 0, viz->quads_normal_3d);
	glVertexPointer(3, GL_FLOAT, 0, src_quads_v);
	glDrawArrays(GL_QUADS, 0, 24);

	// render our arrays of dst vertices with given normals
	glColor4ubv(node->src_dst_port.dst_coords.color);
	glNormalPointer(GL_FLOAT, 0, viz->quads_normal_3d);
	glVertexPointer(3, GL_FLOAT, 0, dst_quads_v);
	glDrawArrays(GL_QUADS, 0, 24);

	// render our line between the src/dst ports
	glDisable(GL_LIGHTING);
	glLineWidth((node->volume * gtk_range_get_value(GTK_RANGE(viz->line_scale_range)) + .01) / 5000.0);
	glBegin(GL_LINES);
		glColor3ubv(node->src_dst_port.src_coords.color);
		glVertex3f(node->src_dst_port.src_coords.x_coord + (node->src_dst_port.src_coords.x_width * 1/4), node->src_dst_port.src_coords.y_coord + (node->src_dst_port.src_coords.y_width * 1/4), node->src_dst_port.src_coords.z_coord);
		glVertex3f(node->src_dst_port.dst_coords.x_coord + (node->src_dst_port.dst_coords.x_width * 3/4), node->src_dst_port.dst_coords.y_coord + (node->src_dst_port.dst_coords.y_width * 3/4), node->src_dst_port.dst_coords.z_coord);
	glEnd();
	if (!viz->disable_lighting) {
		glEnable(GL_LIGHTING);
	}


	// draw labels if necessary
	if (viz->draw_labels) {
		glDisable(GL_LIGHTING);

		gchar *src_node_label = g_strdup_printf("%s:%d", inet_ntoa(node->src_dst_port.src_addr), node->src_dst_port.src_port);
		gchar *dst_node_label = g_strdup_printf("%s:%d", inet_ntoa(node->src_dst_port.dst_addr), node->src_dst_port.dst_port);

		// output the src nodes label
		glColor3ubv(node->src_dst_port.src_coords.color);
		glRasterPos3f(node->src_dst_port.src_coords.x_coord + (node->src_dst_port.src_coords.x_width / 2), node->src_dst_port.src_coords.y_coord + (node->src_dst_port.src_coords.y_width / 2), src_z_height + Z_LABEL_ADJ);
		glListBase(viz->font_list_3d);
		glCallLists(strlen(src_node_label), GL_UNSIGNED_BYTE, src_node_label);

		// output the dst nodes label
		glColor3ubv(node->src_dst_port.dst_coords.color);
		glRasterPos3f(node->src_dst_port.dst_coords.x_coord + (node->src_dst_port.dst_coords.x_width / 2), node->src_dst_port.dst_coords.y_coord + (node->src_dst_port.dst_coords.y_width / 2), dst_z_height + Z_LABEL_ADJ);
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


void handler_src_dst_port_render_3d_scene(void)
{
	guint i;
	GLfloat unit_size = CUBE_SIZE / (GLfloat) (viz->src_dst_port_bound.max_x_coord - viz->src_dst_port_bound.min_x_coord);

	// give the wire outline a solid floor
	glCallList(GREY_3D_SQUARE_CALL_LIST);

	// draw a white square representing src range over our grey base
	for (i = 0; i < viz->num_src_ranges; ++i) {
		GLfloat x_coord = (GLfloat) ((viz->src_ranges[i].x_coord - viz->src_dst_port_bound.min_x_coord) * unit_size) - 1.0;
		GLfloat y_coord = (GLfloat) ((viz->src_ranges[i].y_coord - viz->src_dst_port_bound.min_y_coord) * unit_size) - 1.0;
		GLfloat width = (GLfloat) (viz->src_ranges[i].width * unit_size);

		glBegin(GL_QUAD_STRIP);
		glNormal3f(0.0, 0.0, 1.0);
		glColor3ub(255, 255, 255);

		glVertex3f(x_coord, y_coord, -1.0 + (Z_HEIGHT_ADJ / 2.0));
		glVertex3f(x_coord, y_coord + width, -1.0 + (Z_HEIGHT_ADJ / 2.0));
		glVertex3f(x_coord + width, y_coord, -1.0 + (Z_HEIGHT_ADJ / 2.0));
		glVertex3f(x_coord + width, y_coord + width, -1.0 + (Z_HEIGHT_ADJ / 2.0));

		glEnd();
	}

	// draw a white square representing dst range over our grey base
	for (i = 0; i < viz->num_dst_ranges; ++i) {
		GLfloat x_coord = (GLfloat) ((viz->dst_ranges[i].x_coord - viz->src_dst_port_bound.min_x_coord) * unit_size) - 1.0;
		GLfloat y_coord = (GLfloat) ((viz->dst_ranges[i].y_coord - viz->src_dst_port_bound.min_y_coord) * unit_size) - 1.0;
		GLfloat width = (GLfloat) (viz->dst_ranges[i].width * unit_size);

		glBegin(GL_QUAD_STRIP);
		glNormal3f(0.0, 0.0, 1.0);
		glColor3ub(255, 255, 255);

		glVertex3f(x_coord, y_coord, -1.0 + (Z_HEIGHT_ADJ / 2.0));
		glVertex3f(x_coord, y_coord + width, -1.0 + (Z_HEIGHT_ADJ / 2.0));
		glVertex3f(x_coord + width, y_coord, -1.0 + (Z_HEIGHT_ADJ / 2.0));
		glVertex3f(x_coord + width, y_coord + width, -1.0 + (Z_HEIGHT_ADJ / 2.0));

		glEnd();
	}

	// white wire outline
	glCallList(WIRE_CUBE_CALL_LIST);


	// output our coord labels
	glDisable(GL_LIGHTING);

	// derive address based on the quadtree coordinates of our bounding box
	struct in_addr low_addr;
	struct in_addr high_addr;
	range_reverse_quadtree(&viz->src_dst_port_bound, &low_addr, &high_addr);

	gchar *low_str = g_strdup_printf(" %s", inet_ntoa(low_addr));
	gchar *high_str = g_strdup_printf(" %s", inet_ntoa(high_addr));

	// lower address range label
	glColor3f(1.0, 1.0, 1.0);
	glRasterPos3f(-1.0, -1.0, -0.9);
	glListBase(viz->font_list_3d);
	glCallLists(strlen(low_str), GL_UNSIGNED_BYTE, low_str);

	// higher address range label
	glRasterPos3f(1.0, 1.0, -0.9);
	glListBase(viz->font_list_3d);
	glCallLists(strlen(high_str), GL_UNSIGNED_BYTE, high_str);

	g_free(low_str);
	g_free(high_str);

	glEnable(GL_LIGHTING);


	// output our z-axis labels
	glDisable(GL_LIGHTING);

	// determine range of ports to be labeled
	gint range_min = MIN(viz->port1_min, viz->port2_min);
	gint range_max = MAX(viz->port1_max, viz->port2_max);
	gint range_diff = range_max - range_min;

	gchar *label0 = g_strdup_printf(" %d", range_min);
	gchar *label1 = g_strdup_printf(" %d", range_min + range_diff * 1/4);
	gchar *label2 = g_strdup_printf(" %d  PORT", range_min + range_diff * 1/2);
	gchar *label3 = g_strdup_printf(" %d", range_min + range_diff * 3/4);
	gchar *label4 = g_strdup_printf(" %d", range_max);

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

	// render all our nodes!
	g_tree_foreach(viz->nodes, handler_src_dst_port_render_3d_node, NULL);

	if (!viz->transparent) {
		glCallList(FLAMINGO_LOGO_CALL_LIST);
	}
}


gint handler_src_dst_port_compare_nodes(tree_node_t *node_a, tree_node_t *node_b)
{
	guint src_mask_a = pow(2, 32 - node_a->src_dst_port.src_network_bits) - 1;
	guint src_mask_b = pow(2, 32 - node_b->src_dst_port.src_network_bits) - 1;
	guint src_addr_a = g_ntohl(node_a->src_dst_port.src_addr.s_addr) & ~src_mask_a;
	guint src_addr_b = g_ntohl(node_b->src_dst_port.src_addr.s_addr) & ~src_mask_b;

	// check if the src addresses are less than or greater than the other
	if (src_addr_a < src_addr_b) {
		return -1;
	} else if (src_addr_a > src_addr_b) {
		return 1;
	}

	guint dst_mask_a = pow(2, 32 - node_a->src_dst_port.dst_network_bits) - 1;
	guint dst_mask_b = pow(2, 32 - node_b->src_dst_port.dst_network_bits) - 1;
	guint dst_addr_a = g_ntohl(node_a->src_dst_port.dst_addr.s_addr) & ~dst_mask_a;
	guint dst_addr_b = g_ntohl(node_b->src_dst_port.dst_addr.s_addr) & ~dst_mask_b;

	// check if the dst addresses are less than or greater than the other
	if (dst_addr_a < dst_addr_b) {
		return -1;
	} else if (dst_addr_a > dst_addr_b) {
		return 1;
	}

	// if we get to here the addresses must be equal, so compare the source ports
	if (node_a->src_dst_port.src_port < node_b->src_dst_port.src_port) {
		return -1;
	} else if (node_a->src_dst_port.src_port > node_b->src_dst_port.src_port) {
		return 1;
	}

	// if we get to here the source ports must be equal, so compare the dst ports
	if (node_a->src_dst_port.dst_port < node_b->src_dst_port.dst_port) {
		return -1;
	} else if (node_a->src_dst_port.dst_port > node_b->src_dst_port.dst_port) {
		return 1;
	}

	// if we get to here, both are identical
	return 0;
}


tree_node_t *handler_src_dst_port_lookup_node_from_string(gchar *node_str)
{
	tree_node_t *node;
	gchar **line_split, **src_split, **dst_split, **src_addr_split, **dst_addr_split;
	struct in_addr src_addr, dst_addr;

	node = g_new0(tree_node_t, 1);

	// split up the two address/network bits:port pairs
	line_split = g_strsplit(node_str, "->", 2);

	// split up the src and dst addr/length
	src_split = g_strsplit(line_split[0], ":", 2);
	dst_split = g_strsplit(line_split[1], ":", 2);

	src_addr_split = g_strsplit(src_split[0], "/", 2);
	dst_addr_split = g_strsplit(dst_split[0], "/", 2);

	// strip whitespace
	src_split[1] = g_strstrip(src_split[1]);
	dst_split[1] = g_strstrip(dst_split[1]);
	src_addr_split[0] = g_strstrip(src_addr_split[0]);
	src_addr_split[1] = g_strstrip(src_addr_split[1]);
	dst_addr_split[0] = g_strstrip(dst_addr_split[0]);
	dst_addr_split[1] = g_strstrip(dst_addr_split[1]);

	inet_aton(src_addr_split[0], &src_addr);
	inet_aton(dst_addr_split[0], &dst_addr);

	node->src_dst_port.src_addr.s_addr = src_addr.s_addr;
	node->src_dst_port.src_network_bits = atoi(src_addr_split[1]);
	node->src_dst_port.src_port = atoi(src_split[1]);

	node->src_dst_port.dst_addr.s_addr = dst_addr.s_addr;
	node->src_dst_port.dst_network_bits = atoi(dst_addr_split[1]);
	node->src_dst_port.dst_port = atoi(dst_split[1]);

	g_strfreev(line_split);
	g_strfreev(src_addr_split);
	g_strfreev(dst_addr_split);
	g_strfreev(src_split);
	g_strfreev(dst_split);

	return node;
}


void handler_src_dst_port_input_process_nodes(GTree *tree, data_record_t *data, guint len)
{
	// loop through all received entries
	data_record_t *curr_record;
	guint i, num_entries = len / sizeof(data_record_t);

	for (i = 0; i < num_entries; ++i) {
		curr_record = (data_record_t *) data + i;

		// allocate mem for a new node
		tree_node_t *node = g_new0(tree_node_t, 1);

		// assign the node's vars
		node->src_dst_port.src_addr.s_addr = curr_record->address;
		node->src_dst_port.src_network_bits = g_ntohl(curr_record->length);
		node->src_dst_port.src_port = g_ntohl(curr_record->port);
		node->src_dst_port.src_origin_as = g_ntohl(curr_record->originas);
		node->src_dst_port.dst_addr.s_addr = curr_record->dstaddr;
		node->src_dst_port.dst_network_bits = g_ntohl(curr_record->dstlen);
		node->src_dst_port.dst_port = g_ntohl(curr_record->dstport);
		node->src_dst_port.dst_origin_as = g_ntohl(curr_record->dstoriginas);
		node->src_dst_port.protocol = g_ntohl(curr_record->proto);
		node->volume = g_ntohl(curr_record->count);

		// assign our RGB colors of the node as specified by the server
		node->src_dst_port.src_coords.color[0] = curr_record->red;
		node->src_dst_port.src_coords.color[1] = curr_record->green;
		node->src_dst_port.src_coords.color[2] = curr_record->blue;
		node->src_dst_port.dst_coords.color[0] = curr_record->red2;
		node->src_dst_port.dst_coords.color[1] = curr_record->green2;
		node->src_dst_port.dst_coords.color[2] = curr_record->blue2;

		// calculate the transparency of the node based on its network bits
		node->src_dst_port.src_coords.color[3] = 255 - ((24 - node->src_dst_port.src_network_bits) * 10);
		node->src_dst_port.dst_coords.color[3] = 255 - ((24 - node->src_dst_port.dst_network_bits) * 10);

		// calculate the src quadtree coordinates
		node->src_dst_port.src_coords.x_coord = 0.0;
		node->src_dst_port.src_coords.y_coord = 0.0;

		// calculate width of the src/dst node
		guint src_num_of_quad_divisions = (node->src_dst_port.src_network_bits + 1) / 2;
		guint dst_num_of_quad_divisions = (node->src_dst_port.dst_network_bits + 1) / 2;
		GLfloat src_node_x_width = COORD_SIZE / (pow(2.0, src_num_of_quad_divisions));
		GLfloat src_node_y_width = COORD_SIZE / (pow(2.0, src_num_of_quad_divisions));
		GLfloat dst_node_x_width = COORD_SIZE / (pow(2.0, dst_num_of_quad_divisions));
		GLfloat dst_node_y_width = COORD_SIZE / (pow(2.0, dst_num_of_quad_divisions));

		// loop to find the coordinates where our quadtree begins
		gboolean ignore_last = FALSE;
		guint i, loops = (node->src_dst_port.src_network_bits + 1) / 2;
		GLfloat adjustment = COORD_SIZE;
		for (i = 0; i < loops; ++i) {
			adjustment /= 2.0;

			// if its our last loop and we have an odd num of network bits, ignore the last bit and double our y_width
			if ((i == (loops - 1)) && (node->src_dst_port.src_network_bits % 2 == 1)) {
				ignore_last = TRUE;
				src_node_y_width *= 2.0;
			}

			// for a COORD_SIZE x COORD_SIZE grid
			switch (SHIFT(node->src_dst_port.src_addr.s_addr, ((((i/4)+1)*8) - 2*((i%4)+1)), 0x3)) {
				// 00
				case 0:
					// do nothing
				break;

				// 01
				case 1:
					if (!ignore_last) {
						node->src_dst_port.src_coords.y_coord += adjustment;
					}
				break;

				// 10
				case 2:
					node->src_dst_port.src_coords.x_coord += adjustment;
				break;

				// 11
				case 3:
					node->src_dst_port.src_coords.x_coord += adjustment;

					if (!ignore_last) {
						node->src_dst_port.src_coords.y_coord += adjustment;
					}
				break;
			}
		}

		// calculate the dst quadtree coordinates
		node->src_dst_port.dst_coords.x_coord = 0.0;
		node->src_dst_port.dst_coords.y_coord = 0.0;

		// loop to find the coordinates where our quadtree begins
		ignore_last = FALSE;
		loops = (node->src_dst_port.dst_network_bits + 1) / 2;
		adjustment = COORD_SIZE;
		for (i = 0; i < loops; ++i) {
			adjustment /= 2.0;

			// if its our last loop and we have an odd num of network bits, ignore the last bit and double our y_width
			if ((i == (loops - 1)) && (node->src_dst_port.dst_network_bits % 2 == 1)) {
				ignore_last = TRUE;
				dst_node_y_width *= 2.0;
			}

			// for a COORD_SIZE x COORD_SIZE grid
			switch (SHIFT(node->src_dst_port.dst_addr.s_addr, ((((i/4)+1)*8) - 2*((i%4)+1)), 0x3)) {
				// 00
				case 0:
					// do nothing
				break;

				// 01
				case 1:
					if (!ignore_last) {
						node->src_dst_port.dst_coords.y_coord += adjustment;
					}
				break;

				// 10
				case 2:
					node->src_dst_port.dst_coords.x_coord += adjustment;
				break;

				// 11
				case 3:
					node->src_dst_port.dst_coords.x_coord += adjustment;

					if (!ignore_last) {
						node->src_dst_port.dst_coords.y_coord += adjustment;
					}
				break;
			}
		}

		GLfloat unit_size = CUBE_SIZE / (GLfloat) (viz->src_dst_port_bound.max_x_coord - viz->src_dst_port_bound.min_x_coord);

		// adjust to the bounded box coords
		node->src_dst_port.src_coords.x_coord -= viz->src_dst_port_bound.min_x_coord;
		node->src_dst_port.src_coords.y_coord -= viz->src_dst_port_bound.min_y_coord;
		node->src_dst_port.dst_coords.x_coord -= viz->src_dst_port_bound.min_x_coord;
		node->src_dst_port.dst_coords.y_coord -= viz->src_dst_port_bound.min_y_coord;

		// translate src coords and dimensions
		node->src_dst_port.src_coords.x_coord = (GLfloat) (node->src_dst_port.src_coords.x_coord * unit_size) - 1.0;
		node->src_dst_port.src_coords.y_coord = (GLfloat) (node->src_dst_port.src_coords.y_coord * unit_size) - 1.0;
		node->src_dst_port.src_coords.x_width = (GLfloat) (src_node_x_width * unit_size);
		node->src_dst_port.src_coords.y_width = (GLfloat) (src_node_y_width * unit_size);

		// translate dst coords and dimensions
		node->src_dst_port.dst_coords.x_coord = (GLfloat) (node->src_dst_port.dst_coords.x_coord * unit_size) - 1.0;
		node->src_dst_port.dst_coords.y_coord = (GLfloat) (node->src_dst_port.dst_coords.y_coord * unit_size) - 1.0;
		node->src_dst_port.dst_coords.x_width = (GLfloat) (dst_node_x_width * unit_size);
		node->src_dst_port.dst_coords.y_width = (GLfloat) (dst_node_y_width * unit_size);

		// add the node to our GTree
		g_tree_insert(tree, node, NULL);
	}
}


gchar *handler_src_dst_port_get_node_description(tree_node_t *node)
{
	gchar *src_port_desc, *dst_port_desc, *proto_desc, *full_desc;

	if (node->src_dst_port.src_port >= viz->port_numbers->len) {
		src_port_desc = "Private Port";
	} else {
		src_port_desc = g_ptr_array_index(viz->port_numbers, node->src_dst_port.src_port);

		if (src_port_desc == NULL) {
			src_port_desc = "Unknown Port";
		}
	}

	if (node->src_dst_port.dst_port >= viz->port_numbers->len) {
		dst_port_desc = "Private Port";
	} else {
		dst_port_desc = g_ptr_array_index(viz->port_numbers, node->src_dst_port.dst_port);

		if (dst_port_desc == NULL) {
			dst_port_desc = "Unknown Port";
		}
	}

	if (node->src_dst_port.protocol >= viz->proto_numbers->len) {
		proto_desc = "Unknown";
	} else {
		proto_desc = g_ptr_array_index(viz->proto_numbers, node->src_dst_port.protocol);

		if (proto_desc == NULL) {
			proto_desc = "Unknown";
		}
	}

	full_desc = g_strdup_printf("Proto %d (%s) - Src Port #%d (%s) - Dst Port #%d (%s)", node->src_dst_port.protocol, proto_desc, node->src_dst_port.src_port, src_port_desc, node->src_dst_port.dst_port, dst_port_desc);

	return (full_desc);
}


gchar *handler_src_dst_port_get_node_title(void)
{
	return (g_strdup("Address:Port -> Address:Port"));
}


gchar *handler_src_dst_port_get_node_value(tree_node_t *node)
{
	gchar *src_str = g_strdup_printf("%s/%d:%d", inet_ntoa(node->src_dst_port.src_addr), node->src_dst_port.src_network_bits, node->src_dst_port.src_port);
	gchar *dst_str = g_strdup_printf("%s/%d:%d", inet_ntoa(node->src_dst_port.dst_addr), node->src_dst_port.dst_network_bits, node->src_dst_port.dst_port);

	gchar *final_str =  g_strdup_printf("%s -> %s", src_str, dst_str);

	g_free(src_str);
	g_free(dst_str);

	return (final_str);
}
