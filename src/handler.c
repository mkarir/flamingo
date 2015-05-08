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
#include "handler_ip.h"
#include "handler_as.h"
#include "handler_port.h"
#include "handler_src_dst_ip.h"
#include "handler_src_dst_as.h"
#include "handler_src_dst_port.h"

extern viz_t *viz;


void handler_set_ip_functions(void)
{
	viz->handler.check_visibility = handler_ip_check_visibility;
	viz->handler.check_2d_click = handler_ip_check_2d_click;
	viz->handler.compare_nodes = handler_ip_compare_nodes;
	viz->handler.lookup_node_from_string = handler_ip_lookup_node_from_string;
	viz->handler.get_node_title = handler_ip_get_node_title;
	viz->handler.get_node_value = handler_ip_get_node_value;
	viz->handler.get_node_description = handler_ip_get_node_description;
	viz->handler.get_node_coords = handler_ip_get_node_coords;
	viz->handler.init_interface = handler_ip_init_interface;
	viz->handler.render_2d_scene = handler_ip_render_2d_scene;
	viz->handler.render_3d_scene = handler_ip_render_3d_scene;
	viz->handler.input_process_nodes = handler_ip_input_process_nodes;
}


void handler_set_as_functions(void)
{
	viz->handler.check_visibility = handler_as_check_visibility;
	viz->handler.check_2d_click = handler_as_check_2d_click;
	viz->handler.compare_nodes = handler_as_compare_nodes;
	viz->handler.lookup_node_from_string = handler_as_lookup_node_from_string;
	viz->handler.get_node_title = handler_as_get_node_title;
	viz->handler.get_node_value = handler_as_get_node_value;
	viz->handler.get_node_description = handler_as_get_node_description;
	viz->handler.get_node_coords = handler_as_get_node_coords;
	viz->handler.init_interface = handler_as_init_interface;
	viz->handler.render_2d_scene = handler_as_render_2d_scene;
	viz->handler.render_3d_scene = handler_as_render_3d_scene;
	viz->handler.input_process_nodes = handler_as_input_process_nodes;
}


void handler_set_port_functions(void)
{
	viz->handler.check_visibility = handler_port_check_visibility;
	viz->handler.check_2d_click = handler_port_check_2d_click;
	viz->handler.compare_nodes = handler_port_compare_nodes;
	viz->handler.lookup_node_from_string = handler_port_lookup_node_from_string;
	viz->handler.get_node_title = handler_port_get_node_title;
	viz->handler.get_node_value = handler_port_get_node_value;
	viz->handler.get_node_description = handler_port_get_node_description;
	viz->handler.get_node_coords = handler_port_get_node_coords;
	viz->handler.init_interface = handler_port_init_interface;
	viz->handler.render_2d_scene = handler_port_render_2d_scene;
	viz->handler.render_3d_scene = handler_port_render_3d_scene;
	viz->handler.input_process_nodes = handler_port_input_process_nodes;
}


void handler_set_src_dst_ip_functions(void)
{
	viz->handler.check_visibility = handler_src_dst_ip_check_visibility;
	viz->handler.check_2d_click = handler_src_dst_ip_check_2d_click;
	viz->handler.compare_nodes = handler_src_dst_ip_compare_nodes;
	viz->handler.lookup_node_from_string = handler_src_dst_ip_lookup_node_from_string;
	viz->handler.get_node_title = handler_src_dst_ip_get_node_title;
	viz->handler.get_node_value = handler_src_dst_ip_get_node_value;
	viz->handler.get_node_description = handler_src_dst_ip_get_node_description;
	viz->handler.get_node_coords = handler_src_dst_ip_get_node_coords;
	viz->handler.init_interface = handler_src_dst_ip_init_interface;
	viz->handler.render_2d_scene = handler_src_dst_ip_render_2d_scene;
	viz->handler.render_3d_scene = handler_src_dst_ip_render_3d_scene;
	viz->handler.input_process_nodes = handler_src_dst_ip_input_process_nodes;
}


void handler_set_src_dst_as_functions(void)
{
	viz->handler.check_visibility = handler_src_dst_as_check_visibility;
	viz->handler.check_2d_click = handler_src_dst_as_check_2d_click;
	viz->handler.compare_nodes = handler_src_dst_as_compare_nodes;
	viz->handler.lookup_node_from_string = handler_src_dst_as_lookup_node_from_string;
	viz->handler.get_node_title = handler_src_dst_as_get_node_title;
	viz->handler.get_node_value = handler_src_dst_as_get_node_value;
	viz->handler.get_node_description = handler_src_dst_as_get_node_description;
	viz->handler.get_node_coords = handler_src_dst_as_get_node_coords;
	viz->handler.init_interface = handler_src_dst_as_init_interface;
	viz->handler.render_2d_scene = handler_src_dst_as_render_2d_scene;
	viz->handler.render_3d_scene = handler_src_dst_as_render_3d_scene;
	viz->handler.input_process_nodes = handler_src_dst_as_input_process_nodes;
}


void handler_set_src_dst_port_functions(void)
{
	viz->handler.check_visibility = handler_src_dst_port_check_visibility;
	viz->handler.check_2d_click = handler_src_dst_port_check_2d_click;
	viz->handler.compare_nodes = handler_src_dst_port_compare_nodes;
	viz->handler.lookup_node_from_string = handler_src_dst_port_lookup_node_from_string;
	viz->handler.get_node_title = handler_src_dst_port_get_node_title;
	viz->handler.get_node_value = handler_src_dst_port_get_node_value;
	viz->handler.get_node_description = handler_src_dst_port_get_node_description;
	viz->handler.get_node_coords = handler_src_dst_port_get_node_coords;
	viz->handler.init_interface = handler_src_dst_port_init_interface;
	viz->handler.render_2d_scene = handler_src_dst_port_render_2d_scene;
	viz->handler.render_3d_scene = handler_src_dst_port_render_3d_scene;
	viz->handler.input_process_nodes = handler_src_dst_port_input_process_nodes;
}
