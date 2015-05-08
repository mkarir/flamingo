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


void handler_src_dst_ip_init_interface()
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


coord_t *handler_src_dst_ip_get_node_coords(tree_node_t *node)
{
	return (&node->src_dst_ip.src_coords);
}


gboolean handler_src_dst_ip_check_2d_click(tree_node_t *node, GLfloat x_pos, GLfloat y_pos)
{
	// check if the click was within the boundries of the src node
	if (x_pos >= node->src_dst_ip.src_coords.y_coord
			&& x_pos <= node->src_dst_ip.src_coords.y_coord + node->src_dst_ip.src_coords.x_width
			&& y_pos >= node->src_dst_ip.src_coords.z_coord
			&& y_pos <= node->src_dst_ip.src_coords.z_coord + node->src_dst_ip.src_coords.y_width) {
		return TRUE;
	}

	return FALSE;
}


gboolean handler_src_dst_ip_check_visibility(tree_node_t *node)
{
	return (node->volume >= viz->threshold_min && node->volume <= viz->threshold_max);
}


static inline gboolean handler_src_dst_ip_render_2d_node(gpointer node_in, gpointer null, gpointer data)
{
	tree_node_t *node = node_in;

	// return if the node is not within the thresholds
	if (!viz->handler.check_visibility(node)) {
		return FALSE;
	}

	// precalculate these to avoid redundant calculations
	GLfloat x_width = node->src_dst_ip.src_coords.y_coord + node->src_dst_ip.src_coords.x_width;
	GLfloat y_width = node->src_dst_ip.src_coords.z_coord + node->src_dst_ip.src_coords.y_width;

	GLfloat quad_src_dst_ip_v[] = {
					node->src_dst_ip.src_coords.y_coord, node->src_dst_ip.src_coords.z_coord,
					x_width, node->src_dst_ip.src_coords.z_coord,
					node->src_dst_ip.src_coords.y_coord, y_width,
					x_width, y_width
				 };

	// set color and transparency
	if (!viz->transparent) {
		node->src_dst_ip.src_coords.color[3] = 255;
	}
	glColor4ubv(node->src_dst_ip.src_coords.color);

	// render our arrays of vertices
	glVertexPointer(2, GL_FLOAT, 0, quad_src_dst_ip_v);
	glDrawArrays(GL_QUAD_STRIP, 0, 4);

	return FALSE;
}


void handler_src_dst_ip_render_2d_scene(void)
{
	guint i;
	GLfloat unit_size = CUBE_SIZE  / (GLfloat) (viz->src_bound.max_x_coord - viz->src_bound.min_x_coord);

	// draw a square outline
	glCallList(GREY_2D_SQUARE_CALL_LIST);

	// draw a white square representing each range over our grey base
	for (i = 0; i < viz->num_src_ranges; ++i) {
		GLfloat x_coord = (GLfloat) ((viz->src_ranges[i].x_coord - viz->src_bound.min_x_coord) * unit_size) - 1.0;
		GLfloat y_coord = (GLfloat) ((viz->src_ranges[i].y_coord - viz->src_bound.min_y_coord) * unit_size) - 1.0;
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

	// derive address based on the quadtree coordinates of our bounding box
	struct in_addr low_addr;
	struct in_addr high_addr;
	range_reverse_quadtree(&viz->src_bound, &low_addr, &high_addr);

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
	g_tree_foreach(viz->nodes, handler_src_dst_ip_render_2d_node, NULL);
}


static inline gboolean handler_src_dst_ip_render_3d_node(gpointer node_in, gpointer null, gpointer data)
{
	tree_node_t *node = node_in;

	// only draw the node if it is between the thresholds
	if (!viz->handler.check_visibility(node)) {
		return FALSE;
	}

	// precalculate these to avoid redundant calculations
	GLfloat src_y_width = node->src_dst_ip.src_coords.y_coord + node->src_dst_ip.src_coords.x_width;
	GLfloat src_z_width = node->src_dst_ip.src_coords.z_coord + node->src_dst_ip.src_coords.y_width;
	GLfloat dst_y_width = node->src_dst_ip.dst_coords.y_coord + node->src_dst_ip.dst_coords.x_width;
	GLfloat dst_z_width = node->src_dst_ip.dst_coords.z_coord + node->src_dst_ip.dst_coords.y_width;

	// define our array of quadrilaterals
	GLfloat quads_v[] =     {
					// node on left side of cube cube
					-1.0 + Z_HEIGHT_ADJ, node->src_dst_ip.src_coords.y_coord, node->src_dst_ip.src_coords.z_coord,
					-1.0 + Z_HEIGHT_ADJ, src_y_width, node->src_dst_ip.src_coords.z_coord,
					-1.0 + Z_HEIGHT_ADJ, src_y_width, src_z_width,
					-1.0 + Z_HEIGHT_ADJ, node->src_dst_ip.src_coords.y_coord, src_z_width,

					// node on right side of cube cube
					1.0 - Z_HEIGHT_ADJ, node->src_dst_ip.dst_coords.y_coord, node->src_dst_ip.dst_coords.z_coord,
					1.0 - Z_HEIGHT_ADJ, dst_y_width, node->src_dst_ip.dst_coords.z_coord,
					1.0 - Z_HEIGHT_ADJ, dst_y_width, dst_z_width,
					1.0 - Z_HEIGHT_ADJ, node->src_dst_ip.dst_coords.y_coord, dst_z_width
				};

	// set color and alpha transparency
	if (!viz->transparent) {
		node->src_dst_ip.src_coords.color[3] = 255;
		node->src_dst_ip.dst_coords.color[3] = 255;
	}

	// render our source node on the left side of the cube
	glColor4ubv(node->src_dst_ip.src_coords.color);
	glNormalPointer(GL_FLOAT, 0, viz->quads_normal_3d + 12);
	glVertexPointer(3, GL_FLOAT, 0, quads_v);
	glDrawArrays(GL_QUADS, 0, 4);

	// render our dst node on the right side of the cube
	glColor4ubv(node->src_dst_ip.dst_coords.color);
	glNormalPointer(GL_FLOAT, 0, viz->quads_normal_3d + 24);
	glVertexPointer(3, GL_FLOAT, 0, quads_v + 12);
	glDrawArrays(GL_QUADS, 0, 4);

	// draw our line connecting the src to dst
	glDisable(GL_LIGHTING);
	glLineWidth((node->volume * gtk_range_get_value(GTK_RANGE(viz->line_scale_range)) + .01) / 5000.0);
	glBegin(GL_LINES);
		glColor3ubv(node->src_dst_ip.src_coords.color);
		glVertex3f(-1.0 + Z_HEIGHT_ADJ, node->src_dst_ip.src_coords.y_coord + (node->src_dst_ip.src_coords.x_width / 2), node->src_dst_ip.src_coords.z_coord + (node->src_dst_ip.src_coords.y_width / 2));
		glVertex3f(1.0 - Z_HEIGHT_ADJ, node->src_dst_ip.dst_coords.y_coord + (node->src_dst_ip.dst_coords.x_width / 2), node->src_dst_ip.dst_coords.z_coord + (node->src_dst_ip.dst_coords.y_width / 2));
	glEnd();
	if (!viz->disable_lighting) {
		glEnable(GL_LIGHTING);
	}

	// draw labels if necessary
	if (viz->draw_labels) {
		glDisable(GL_LIGHTING);

		gchar *src_node_label = g_strdup_printf("%s/%d", inet_ntoa(node->src_dst_ip.src_addr), node->src_dst_ip.src_network_bits);
		gchar *dst_node_label = g_strdup_printf("%s/%d", inet_ntoa(node->src_dst_ip.dst_addr), node->src_dst_ip.dst_network_bits);

		// output the source nodes label
		glColor3ubv(node->src_dst_ip.src_coords.color);
		glRasterPos3f(-1.0 + (Z_HEIGHT_ADJ * 10), node->src_dst_ip.src_coords.y_coord + (node->src_dst_ip.src_coords.x_width / 2), node->src_dst_ip.src_coords.z_coord + (node->src_dst_ip.src_coords.y_width / 2) + Z_LABEL_ADJ);
		glListBase(viz->font_list_3d);
		glCallLists(strlen(src_node_label), GL_UNSIGNED_BYTE, src_node_label);

		// output the dst nodes label
		glColor3ubv(node->src_dst_ip.dst_coords.color);
		glRasterPos3f(1.0 - (Z_HEIGHT_ADJ * 10), node->src_dst_ip.dst_coords.y_coord + (node->src_dst_ip.dst_coords.x_width / 2), node->src_dst_ip.dst_coords.z_coord + (node->src_dst_ip.dst_coords.y_width / 2) + Z_LABEL_ADJ);
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


void handler_src_dst_ip_render_3d_scene(void)
{
	guint i;
	GLfloat src_unit_size = CUBE_SIZE  / (GLfloat) (viz->src_bound.max_x_coord - viz->src_bound.min_x_coord);
	GLfloat dst_unit_size = CUBE_SIZE  / (GLfloat) (viz->dst_bound.max_x_coord - viz->dst_bound.min_x_coord);

	// draw a white square representing each src range on the left side of the cube
	for (i = 0; i < viz->num_src_ranges; ++i) {
		GLfloat x_coord = (GLfloat) ((viz->src_ranges[i].x_coord - viz->src_bound.min_x_coord) * src_unit_size) - 1.0;
		GLfloat y_coord = (GLfloat) ((viz->src_ranges[i].y_coord - viz->src_bound.min_y_coord) * src_unit_size) - 1.0;
		GLfloat width = (GLfloat) (viz->src_ranges[i].width * src_unit_size);

		glBegin(GL_QUAD_STRIP);
		glNormal3f(0.0, 0.0, 1.0);
		glColor3ub(255, 255, 255);

		glVertex3f(-1.0 + (Z_HEIGHT_ADJ / 2.0), x_coord, y_coord);
		glVertex3f(-1.0 + (Z_HEIGHT_ADJ / 2.0), x_coord, y_coord + width);
		glVertex3f(-1.0 + (Z_HEIGHT_ADJ / 2.0), x_coord + width, y_coord);
		glVertex3f(-1.0 + (Z_HEIGHT_ADJ / 2.0), x_coord + width, y_coord + width);

		glEnd();
	}

	// draw a white square representing each dst range on the right side of the cube
	for (i = 0; i < viz->num_dst_ranges; ++i) {
		GLfloat x_coord = (GLfloat) ((viz->dst_ranges[i].x_coord - viz->dst_bound.min_x_coord) * dst_unit_size) - 1.0;
		GLfloat y_coord = (GLfloat) ((viz->dst_ranges[i].y_coord - viz->dst_bound.min_y_coord) * dst_unit_size) - 1.0;
		GLfloat width = (GLfloat) (viz->dst_ranges[i].width * dst_unit_size);

		glBegin(GL_QUAD_STRIP);
		glNormal3f(0.0, 0.0, 1.0);
		glColor3ub(255, 255, 255);

		glVertex3f(1.0 - (Z_HEIGHT_ADJ / 2.0), x_coord, y_coord);
		glVertex3f(1.0 - (Z_HEIGHT_ADJ / 2.0), x_coord, y_coord + width);
		glVertex3f(1.0 - (Z_HEIGHT_ADJ / 2.0), x_coord + width, y_coord);
		glVertex3f(1.0 - (Z_HEIGHT_ADJ / 2.0), x_coord + width, y_coord + width);

		glEnd();
	}


	// draw our wire cube outline
	glCallList(WIRE_CUBE_CALL_LIST);


	// output our src coord labels
	glDisable(GL_LIGHTING);

	// derive address based on the quadtree coordinates of our bounding box
	struct in_addr src_low_addr;
	struct in_addr src_high_addr;
	range_reverse_quadtree(&viz->src_bound, &src_low_addr, &src_high_addr);

	gchar *src_low_str = g_strdup_printf(" SRC %s", inet_ntoa(src_low_addr));
	gchar *src_high_str = g_strdup_printf(" SRC %s", inet_ntoa(src_high_addr));

	// lower address range label
	glColor3f(1.0, 1.0, 1.0);
	glRasterPos3f(-1.0, -1.0, -1.0);
	glListBase(viz->font_list_3d);
	glCallLists(strlen(src_low_str), GL_UNSIGNED_BYTE, src_low_str);

	// higher address range label
	glRasterPos3f(-1.0, 1.0, 1.0);
	glListBase(viz->font_list_3d);
	glCallLists(strlen(src_high_str), GL_UNSIGNED_BYTE, src_high_str);

	g_free(src_low_str);
	g_free(src_high_str);

	glEnable(GL_LIGHTING);


	// output our dst coord labels
	glDisable(GL_LIGHTING);

	// derive address based on the quadtree coordinates of our bounding box
	struct in_addr dst_low_addr;
	struct in_addr dst_high_addr;
	range_reverse_quadtree(&viz->dst_bound, &dst_low_addr, &dst_high_addr);

	gchar *dst_low_str = g_strdup_printf(" DST %s", inet_ntoa(dst_low_addr));
	gchar *dst_high_str = g_strdup_printf(" DST %s", inet_ntoa(dst_high_addr));

	// lower address range label
	glColor3f(1.0, 1.0, 1.0);
	glRasterPos3f(1.0, -1.0, -1.0);
	glListBase(viz->font_list_3d);
	glCallLists(strlen(dst_low_str), GL_UNSIGNED_BYTE, dst_low_str);

	// higher address range label
	glRasterPos3f(1.0, 1.0, 1.0);
	glListBase(viz->font_list_3d);
	glCallLists(strlen(dst_high_str), GL_UNSIGNED_BYTE, dst_high_str);

	g_free(dst_low_str);
	g_free(dst_high_str);

	glEnable(GL_LIGHTING);


	// disable the lighting if necessary for a performance increase
	if (viz->disable_lighting) {
		glDisable(GL_LIGHTING);
	}

	// give the wire outline two solid sides
	glCallList(GREY_3D_TWO_SIDED_CALL_LIST);

	// render all our nodes!
	g_tree_foreach(viz->nodes, handler_src_dst_ip_render_3d_node, NULL);	
}


gint handler_src_dst_ip_compare_nodes(tree_node_t *node_a, tree_node_t *node_b)
{
	guint src_mask_a = pow(2, 32 - node_a->src_dst_ip.src_network_bits) - 1;
	guint src_mask_b = pow(2, 32 - node_b->src_dst_ip.src_network_bits) - 1;
	guint src_addr_a = g_ntohl(node_a->src_dst_ip.src_addr.s_addr) & ~src_mask_a;
	guint src_addr_b = g_ntohl(node_b->src_dst_ip.src_addr.s_addr) & ~src_mask_b;

	// check the src addresses first
	if (src_addr_a < src_addr_b) {
		return -1;
	} else if (src_addr_a > src_addr_b) {
		return 1;
	}

	guint dst_mask_a = pow(2, 32 - node_a->src_dst_ip.dst_network_bits) - 1;
	guint dst_mask_b = pow(2, 32 - node_b->src_dst_ip.dst_network_bits) - 1;
	guint dst_addr_a = g_ntohl(node_a->src_dst_ip.dst_addr.s_addr) & ~dst_mask_a;
	guint dst_addr_b = g_ntohl(node_b->src_dst_ip.dst_addr.s_addr) & ~dst_mask_b;

	// if we hit here, we know src_addresses are equal, so compare dst_address
	if (dst_addr_a < dst_addr_b) {
		return -1;
	} else if (dst_addr_a > dst_addr_b) {
		return 1;
	}

	// if we hit here, we know both the src and dst addresses are equal
	return 0;
}


tree_node_t *handler_src_dst_ip_lookup_node_from_string(gchar *node_str)
{
	tree_node_t *node;
	gchar **line_split, **src_split, **dst_split;
	struct in_addr src_addr, dst_addr;

	node = g_new0(tree_node_t, 1);

	// split up the two address/network bits pairs
	line_split = g_strsplit(node_str, "->", 2);

	// split up the src and dst addr/length
	src_split = g_strsplit(line_split[0], "/", 2);
	dst_split = g_strsplit(line_split[1], "/", 2);

	src_split[0] = g_strstrip(src_split[0]);
	src_split[1] = g_strstrip(src_split[1]);
	dst_split[0] = g_strstrip(dst_split[0]);
	dst_split[1] = g_strstrip(dst_split[1]);

	inet_aton(src_split[0], &src_addr);
	inet_aton(dst_split[0], &dst_addr);

	node->src_dst_ip.src_addr.s_addr = src_addr.s_addr;
	node->src_dst_ip.src_network_bits = atoi(src_split[1]);
	node->src_dst_ip.dst_addr.s_addr = dst_addr.s_addr;
	node->src_dst_ip.dst_network_bits = atoi(dst_split[1]);

	g_strfreev(line_split);
	g_strfreev(src_split);
	g_strfreev(dst_split);

	return node;
}


void handler_src_dst_ip_input_process_nodes(GTree *tree, data_record_t *data, guint len)
{
	// loop through all received entries
	data_record_t *curr_record;
	guint i, num_entries = len / sizeof(data_record_t);
	for (i = 0; i < num_entries; ++i) {
		curr_record = (data_record_t *) data + i;

		// allocate mem for a new node
		tree_node_t *node = g_new0(tree_node_t, 1);

		// assign the node's vars
		node->src_dst_ip.src_addr.s_addr = curr_record->address;
		node->src_dst_ip.dst_addr.s_addr = curr_record->dstaddr;
		node->src_dst_ip.src_network_bits = g_ntohl(curr_record->length);
		node->src_dst_ip.dst_network_bits = g_ntohl(curr_record->dstlen);
		node->src_dst_ip.src_origin_as = g_ntohl(curr_record->originas);
		node->src_dst_ip.dst_origin_as = g_ntohl(curr_record->dstoriginas);
		node->volume = g_ntohl(curr_record->count);

		// assign our RGB colors of the node as specified by the server
		node->src_dst_ip.src_coords.color[0] = curr_record->red;
		node->src_dst_ip.src_coords.color[1] = curr_record->green;
		node->src_dst_ip.src_coords.color[2] = curr_record->blue;
		node->src_dst_ip.dst_coords.color[0] = curr_record->red2;
		node->src_dst_ip.dst_coords.color[1] = curr_record->green2;
		node->src_dst_ip.dst_coords.color[2] = curr_record->blue2;

		// calculate the transparency of the node based on its network bits
		node->src_dst_ip.src_coords.color[3] = 255 - ((24 - node->src_dst_ip.src_network_bits) * 10);
		node->src_dst_ip.dst_coords.color[3] = 255 - ((24 - node->src_dst_ip.dst_network_bits) * 10);

		// calculate the quadtree coordinates
		node->src_dst_ip.src_coords.y_coord = 0.0;
		node->src_dst_ip.src_coords.z_coord = 0.0;
		node->src_dst_ip.dst_coords.y_coord = 0.0;
		node->src_dst_ip.dst_coords.z_coord = 0.0;

		// calculate width of the src/dst node
		guint src_num_of_quad_divisions = (node->src_dst_ip.src_network_bits + 1) / 2;
		guint dst_num_of_quad_divisions = (node->src_dst_ip.dst_network_bits + 1) / 2;
		GLfloat src_node_x_width = COORD_SIZE / (pow(2.0, src_num_of_quad_divisions));
		GLfloat src_node_y_width = COORD_SIZE / (pow(2.0, src_num_of_quad_divisions));
		GLfloat dst_node_x_width = COORD_SIZE / (pow(2.0, dst_num_of_quad_divisions));
		GLfloat dst_node_y_width = COORD_SIZE / (pow(2.0, dst_num_of_quad_divisions));

		// loop to find the coordinates where our quadtree begins for our src_address
		gboolean ignore_last = FALSE;
		guint i, loops = (node->src_dst_ip.src_network_bits + 1) / 2;
		GLfloat adjustment = COORD_SIZE;
		for (i = 0; i < loops; ++i) {
			adjustment /= 2.0;

			// if its our last loop and we have an odd num of network bits, ignore the last bit and double our y_width
			if ((i == (loops - 1)) && (node->src_dst_ip.src_network_bits % 2 == 1)) {
				ignore_last = TRUE;
				src_node_y_width *= 2.0;
			}

			// for a COORD_SIZE x COORD_SIZE grid
			switch (SHIFT(node->src_dst_ip.src_addr.s_addr, ((((i/4)+1)*8) - 2*((i%4)+1)), 0x3)) {
				// 00
				case 0:
					// do nothing
				break;

				// 01
				case 1:
					if (!ignore_last) {
						node->src_dst_ip.src_coords.z_coord += adjustment;
					}
				break;

				// 10
				case 2:
					node->src_dst_ip.src_coords.y_coord += adjustment;
				break;

				// 11
				case 3:
					node->src_dst_ip.src_coords.y_coord += adjustment;

					if (!ignore_last) {
						node->src_dst_ip.src_coords.z_coord += adjustment;
					}
				break;
			}
		}

		// loop to find the coordinates where our quadtree begins for our dst_address
		ignore_last = FALSE;
		loops = (node->src_dst_ip.dst_network_bits + 1) / 2;
		adjustment = COORD_SIZE;
		for (i = 0; i < loops; ++i) {
			adjustment /= 2.0;

			// if its our last loop and we have an odd num of network bits, ignore the last bit and double our y_width
			if ((i == (loops - 1)) && (node->src_dst_ip.dst_network_bits % 2 == 1)) {
				ignore_last = TRUE;
				dst_node_y_width *= 2.0;
			}

			// for a COORD_SIZE x COORD_SIZE grid
			switch (SHIFT(node->src_dst_ip.dst_addr.s_addr, ((((i/4)+1)*8) - 2*((i%4)+1)), 0x3)) {
				// 00
				case 0:
					// do nothing
				break;

				// 01
				case 1:
					if (!ignore_last) {
						node->src_dst_ip.dst_coords.z_coord += adjustment;
					}
				break;

				// 10
				case 2:
					node->src_dst_ip.dst_coords.y_coord += adjustment;
				break;

				// 11
				case 3:
					node->src_dst_ip.dst_coords.y_coord += adjustment;

					if (!ignore_last) {
						node->src_dst_ip.dst_coords.z_coord += adjustment;
					}
				break;
			}
		}

		GLfloat src_unit_size = CUBE_SIZE / (GLfloat) (viz->src_bound.max_x_coord - viz->src_bound.min_x_coord);
		GLfloat dst_unit_size = CUBE_SIZE / (GLfloat) (viz->dst_bound.max_x_coord - viz->dst_bound.min_x_coord);

		// adjust to the bounded box coords
		node->src_dst_ip.src_coords.y_coord -= viz->src_bound.min_x_coord;
		node->src_dst_ip.src_coords.z_coord -= viz->src_bound.min_y_coord;
		node->src_dst_ip.dst_coords.y_coord -= viz->dst_bound.min_x_coord;
		node->src_dst_ip.dst_coords.z_coord -= viz->dst_bound.min_y_coord;

		// translate src coords and dimensions onto the left side of the cube
		node->src_dst_ip.src_coords.y_coord = (GLfloat) (node->src_dst_ip.src_coords.y_coord * src_unit_size) - 1.0;
		node->src_dst_ip.src_coords.z_coord = (GLfloat) (node->src_dst_ip.src_coords.z_coord * src_unit_size) - 1.0;
		node->src_dst_ip.src_coords.x_width = (GLfloat) (src_node_x_width * src_unit_size);
		node->src_dst_ip.src_coords.y_width = (GLfloat) (src_node_y_width * src_unit_size);

		// translate dst coords and dimensions onto the right side of the cube
		node->src_dst_ip.dst_coords.y_coord = (GLfloat) (node->src_dst_ip.dst_coords.y_coord * dst_unit_size) - 1.0;
		node->src_dst_ip.dst_coords.z_coord = (GLfloat) (node->src_dst_ip.dst_coords.z_coord * dst_unit_size) - 1.0;
		node->src_dst_ip.dst_coords.x_width = (GLfloat) (dst_node_x_width * dst_unit_size);
		node->src_dst_ip.dst_coords.y_width = (GLfloat) (dst_node_y_width * dst_unit_size);

		// add the node to our GTree
		g_tree_insert(tree, node, NULL);
	}
}


gchar *handler_src_dst_ip_get_node_description(tree_node_t *node)
{	
	return (strdup("N/A"));
}


gchar *handler_src_dst_ip_get_node_title(void)
{
	return (g_strdup("Src Addr -> Dst Addr"));
}


gchar *handler_src_dst_ip_get_node_value(tree_node_t *node)
{
	gchar *src_label, *dst_label, *label;
	
	// create these separately due to how inet_ntoa functions
	src_label = g_strdup_printf("%s/%d", inet_ntoa(node->src_dst_ip.src_addr), node->src_dst_ip.src_network_bits);
	dst_label = g_strdup_printf("%s/%d", inet_ntoa(node->src_dst_ip.dst_addr), node->src_dst_ip.dst_network_bits);

	// create final label
	label = g_strdup_printf("%s -> %s", src_label, dst_label);

	g_free(src_label);
	g_free(dst_label);

	return (label);
}
