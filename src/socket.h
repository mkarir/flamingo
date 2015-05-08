#ifndef __SOCKET_H__
#define __SOCKET_H__

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <glib.h>
#include <config.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <glade/glade.h>

#include "protocol.h"

// common struct for node coordinates
typedef struct {
	// coordinates of the start of the quadtree square
	GLfloat x_coord;
	GLfloat y_coord;
	GLfloat z_coord;

	// width/height of the bar
	GLfloat height;
	GLfloat x_width;
	GLfloat y_width;

	// RGB unsigned byte color for the bar
	GLubyte color[4];
} coord_t;


// node definition for the ip type

typedef struct {
	struct in_addr addr;
	guint network_bits;
	guint origin_as;
	coord_t coords;
} ip_node_t;


// node definition for the as type

typedef struct {
	guint as_num;
	coord_t coords;
} as_node_t;


// node definition for the port type

typedef struct {
	struct in_addr addr;
	guint network_bits;
	guint origin_as;
	guint port_num;
	coord_t coords;
} port_node_t;


// node definition for the src/dst ip type

typedef struct {
	struct in_addr src_addr;
	struct in_addr dst_addr;
	guint src_network_bits;
	guint dst_network_bits;
	guint src_origin_as;
	guint dst_origin_as;
	coord_t src_coords;
	coord_t dst_coords;
} src_dst_ip_node_t;


// node definition for the src/dst as type

typedef struct {
	guint src_as;
	guint dst_as;
	coord_t src_coords;
	coord_t dst_coords;
} src_dst_as_node_t;


// node definition for the src/dst port type

typedef struct {
	struct in_addr src_addr;
	struct in_addr dst_addr;
	guint src_network_bits;
	guint dst_network_bits;
	guint src_origin_as;
	guint dst_origin_as;
	guint src_port;
	guint dst_port;
	guint protocol;
	coord_t src_coords;
	coord_t dst_coords;
} src_dst_port_node_t;


// union for nodes to make variable names friendly

typedef struct {
	union {
		ip_node_t ip;
		as_node_t as;
		port_node_t port;
		src_dst_ip_node_t src_dst_ip;
		src_dst_as_node_t src_dst_as;
		src_dst_port_node_t src_dst_port;
	};

	// volume of node as received from server
	guint volume;
} tree_node_t;


guint socket_process_data_update(data_record_t *records, guint len, guint timestamp);
gboolean socket_handle_hup(GIOChannel *channel, GIOCondition cond, gpointer data);
gboolean socket_handle_in(GIOChannel *channel, GIOCondition cond, gpointer data);
gint socket_connect(const gchar *hostname, guint port);
gint socket_write(gchar *buf, guint len);

#endif
