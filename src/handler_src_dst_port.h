#ifndef __HANDLER_SRC_DEST_PORT_H__
#define __HANDLER_SRC_DEST_PORT_H__

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
#include "socket.h"

gboolean handler_src_dst_port_check_visibility(tree_node_t *node);
gboolean handler_src_dst_port_check_2d_click(tree_node_t *node, GLfloat x_pos, GLfloat y_pos);
gint handler_src_dst_port_compare_nodes(tree_node_t *node_a_in, tree_node_t *node_b_in);
tree_node_t *handler_src_dst_port_lookup_node_from_string(gchar *node_str);
void handler_src_dst_port_init_interface(void);
void handler_src_dst_port_render_2d_scene(void);
void handler_src_dst_port_render_3d_scene(void);
gchar *handler_src_dst_port_get_node_title(void);
gchar *handler_src_dst_port_get_node_value(tree_node_t *node);
gchar *handler_src_dst_port_get_node_description(tree_node_t *node);
coord_t *handler_src_dst_port_get_node_coords(tree_node_t *node);
void handler_src_dst_port_input_process_nodes(GTree *tree, data_record_t *data, guint len);

#endif
