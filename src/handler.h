#ifndef __HANDLER_H__
#define __HANDLER_H__

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


typedef struct {
	gboolean (*check_visibility) (tree_node_t *);
	gboolean (*check_2d_click) (tree_node_t *node, GLfloat x_pos, GLfloat y_pos);
	gint (*compare_nodes) (tree_node_t *node_a_in, tree_node_t *node_b_in);
	tree_node_t * (*lookup_node_from_string) (gchar *node_str);
	gchar * (*get_node_title) (void);
	gchar * (*get_node_value) (tree_node_t *);
	gchar * (*get_node_description) (tree_node_t *);
	coord_t * (*get_node_coords) (tree_node_t *);
	void (*init_interface) (void);
	void (*render_2d_scene) (void);
	void (*render_3d_scene) (void);
	void (*input_process_nodes) (GTree *tree, data_record_t *data, guint len);
} handler_t;

void handler_set_ip_functions(void);
void handler_set_as_functions(void);
void handler_set_port_functions(void);
void handler_set_src_dst_ip_functions(void);
void handler_set_src_dst_as_functions(void);
void handler_set_src_dst_port_functions(void);

#endif
