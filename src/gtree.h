#ifndef _GTREE_H
#define _GTREE_H

#include <glib.h>
#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pond.h"
#include "server.h"

#define COLOR_SIZE		65535

typedef struct {
	guint srcip;
	guint srclen;
	guint dstip;
	guint dstlen;
	guint count;
} src_dst_ip_node_t;

typedef struct {
	guint srcas;
	guint dstas;
	guint count;
} src_dst_as_node_t;

typedef struct {
	guint srcip;
	guint srclen;
	guint dstip;
	guint dstlen;
	guint count;
	guint proto;
	gushort dstport;
	gushort srcport;
} src_dst_port_node_t;

typedef struct {
	guint addr;
	guint network_bits;
	gint count;
} ip_node_t;

typedef struct {
	guint as_num;
	gint count;
} as_node_t;

typedef struct {
	guint ip;
	guint len;
	guint count;
	gushort port;
} port_node_t;

typedef struct {
	guint srcip;
	guint dstip;
	gushort srcport;
	gushort dstport;
	guchar protocol;
} unique_flow_node_t;

typedef struct {
	guint ip;
	ip_node_t *longest_prefix;
} longest_prefix_t;

// node for master ip tree w/o the stats but with colors!
typedef struct {
	guint addr;
	guint network_bits;
	guint origin_as;
	guchar red;
	guchar green;
	guchar blue;
} ip_master_t;

// node for pallet to map colors for non-aggregated feeds
typedef struct {
	guchar red;
	guchar green;
	guchar blue;
}color_t;

typedef struct {
	guint data_type;
	guint num_records;
	client_t *client;
	guint curr_record_id;
	data_record_t *curr_record;
} process_data_t;


gint g_tree_compare_ip(gconstpointer a, gconstpointer b, gpointer user_data);
gint g_tree_compare_as(gconstpointer a, gconstpointer b, gpointer user_data);
gint g_tree_compare_port(gconstpointer a, gconstpointer b, gpointer user_data);
gint g_tree_compare_src_dst_port(gconstpointer a, gconstpointer b, gpointer user_data);
gint g_tree_compare_src_dst_ip(gconstpointer a, gconstpointer b, gpointer user_data);
gint g_tree_compare_src_dst_as(gconstpointer a, gconstpointer b, gpointer user_data);
gint g_tree_compare_unique_flow(gconstpointer a, gconstpointer b, gpointer user_data);
gint g_tree_search_longest_prefix(gconstpointer a, gconstpointer b);

#endif 
