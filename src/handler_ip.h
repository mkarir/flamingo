#ifndef __HANDLER_IP_H__
#define __HANDLER_IP_H__

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

gboolean handler_ip_check_visibility(tree_node_t *node);
gboolean handler_ip_check_2d_click(tree_node_t *node, GLfloat x_pos, GLfloat y_pos);
gint handler_ip_compare_nodes(tree_node_t *node_a_in, tree_node_t *node_b_in);
tree_node_t *handler_ip_lookup_node_from_string(gchar *node_str);
void handler_ip_init_interface(void);
void handler_ip_render_2d_scene(void);
void handler_ip_render_3d_scene(void);
gchar *handler_ip_get_node_title(void);
gchar *handler_ip_get_node_value(tree_node_t *node);
gchar *handler_ip_get_node_description(tree_node_t *node);
coord_t *handler_ip_get_node_coords(tree_node_t *node);
void handler_ip_input_process_nodes(GTree *tree, data_record_t *data, guint len);

#endif
