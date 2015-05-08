#include <glib.h>
#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
 
#include "gtree.h"


gint g_tree_search_longest_prefix(gconstpointer node_in, gconstpointer args_in)
{
	guint mask, node_masked, ip_masked;
	ip_node_t *node = (ip_node_t *) node_in;
	longest_prefix_t *args = (longest_prefix_t *) args_in;

	mask = pow(2, 32 - node->network_bits) - 1;
	node_masked = node->addr & ~mask;

	if (args->ip < node_masked) {
		// if the ip is less, its definitely can't be a part of that network, search to the left
		return -1;
	} else if (args->ip > node_masked) {
		// if the ip is greater, its could be a part of that network
		ip_masked = args->ip & ~mask;

		// check if the ip is part of the network
		if (ip_masked == node_masked) {
			// if so, set our current longest prefix
			args->longest_prefix = node;
		}

		// return 1 to continue searching the greater keys (to the right)
		return 1;
	} else {
		// weird situation where they are equal?!?! not possible?!?
		g_debug("longest prefix evaluates to equal, strange?");
		return 0;
	}
}


gint g_tree_compare_ip(gconstpointer node_a_in, gconstpointer node_b_in, gpointer user_data)
{
	ip_node_t *node_a = (ip_node_t *) node_a_in;
	ip_node_t *node_b = (ip_node_t *) node_b_in;

	guint mask_a = pow(2, 32 - node_a->network_bits) - 1;
	guint mask_b = pow(2, 32 - node_b->network_bits) - 1;
	guint addr_a = node_a->addr & ~mask_a;
	guint addr_b = node_b->addr & ~mask_b;

	if (addr_a < addr_b) {
		return -1;
	} else if (addr_a > addr_b) {
		return 1;
	} else {
		return 0;
	}
}


gint g_tree_compare_as(gconstpointer node_a_in, gconstpointer node_b_in, gpointer user_data)
{
	as_node_t *node_a = (as_node_t *) node_a_in;
	as_node_t *node_b = (as_node_t *) node_b_in;

	if (node_a->as_num < node_b->as_num) {
		return -1;
	} else if (node_a->as_num > node_b->as_num) {
		return 1;
	} else {
		return 0;
	}
}


gint g_tree_compare_port(gconstpointer node_a_in, gconstpointer node_b_in, gpointer user_data)
{
	port_node_t *node_a = (port_node_t *) node_a_in;
	port_node_t *node_b = (port_node_t *) node_b_in;

	guint mask_a = pow(2, 32 - node_a->len) - 1;
	guint mask_b = pow(2, 32 - node_b->len) - 1;
	guint addr_a = node_a->ip & ~mask_a;
	guint addr_b = node_b->ip & ~mask_b;

	// check if the addresses are less than or greater than the other
	if (addr_a < addr_b) {
		return -1;
	} else if (addr_a > addr_b) {
		return 1;
	}

	// if we get to here the addresses must be equal, so compare the ports
	if (node_a->port < node_b->port) {
		return -1;
	} else if (node_a->port > node_b->port) {
		return 1;
	}

	// if we get to here, both are identical
	return 0;
}


gint g_tree_compare_src_dst_ip(gconstpointer node_a_in, gconstpointer node_b_in, gpointer user_data)
{
	guint mask_a, mask_b;
	src_dst_ip_node_t *node_a = (src_dst_ip_node_t *) node_a_in;
	src_dst_ip_node_t *node_b = (src_dst_ip_node_t *) node_b_in;

	mask_a = pow(2, 32 - node_a->srclen) - 1;
	mask_b = pow(2, 32 - node_b->srclen) - 1;
	guint src_addr_a = node_a->srcip & ~mask_a;
	guint src_addr_b = node_b->srcip & ~mask_b;

	// check if the src addresses are less than or greater than the other
	if (src_addr_a < src_addr_b) {
		return -1;
	} else if (src_addr_a > src_addr_b) {
		return 1;
	}

	mask_a = pow(2, 32 - node_a->dstlen) - 1;
	mask_b = pow(2, 32 - node_b->dstlen) - 1;
	guint dst_addr_a = node_a->dstip & ~mask_a;
	guint dst_addr_b = node_b->dstip & ~mask_b;

	// check if the dst addresses are less than or greater than the other
	if (dst_addr_a < dst_addr_b) {
		return -1;
	} else if (dst_addr_a > dst_addr_b) {
		return 1;
	}

	// if we get to here, both are identical
	return 0;
}


gint g_tree_compare_src_dst_as(gconstpointer node_a_in, gconstpointer node_b_in, gpointer user_data)
{
	src_dst_as_node_t *node_a = (src_dst_as_node_t *) node_a_in;
	src_dst_as_node_t *node_b = (src_dst_as_node_t *) node_b_in;

	// check if the src as are less than or greater than the other
	if (node_a->srcas < node_b->srcas) {
		return -1;
	} else if (node_a->srcas > node_b->srcas) {
		return 1;
	}

	// check if the dst as are less than or greater than the other
	if (node_a->dstas < node_b->dstas) {
		return -1;
	} else if (node_a->dstas > node_b->dstas) {
		return 1;
	}

	// if we get to here, both are identical
	return 0;
}


gint g_tree_compare_src_dst_port(gconstpointer node_a_in, gconstpointer node_b_in, gpointer user_data)
{
	guint mask_a, mask_b;
	src_dst_port_node_t *node_a = (src_dst_port_node_t *) node_a_in;
	src_dst_port_node_t *node_b = (src_dst_port_node_t *) node_b_in;

	mask_a = pow(2, 32 - node_a->srclen) - 1;
	mask_b = pow(2, 32 - node_b->srclen) - 1;
	guint src_addr_a = node_a->srcip & ~mask_a;
	guint src_addr_b = node_b->srcip & ~mask_b;

	// check if the src addresses are less than or greater than the other
	if (src_addr_a < src_addr_b) {
		return -1;
	} else if (src_addr_a > src_addr_b) {
		return 1;
	}

	mask_a = pow(2, 32 - node_a->dstlen) - 1;
	mask_b = pow(2, 32 - node_b->dstlen) - 1;
	guint dst_addr_a = node_a->dstip & ~mask_a;
	guint dst_addr_b = node_b->dstip & ~mask_b;

	// check if the dst addresses are less than or greater than the other
	if (dst_addr_a < dst_addr_b) {
		return -1;
	} else if (dst_addr_a > dst_addr_b) {
		return 1;
	}

	// if we get to here the addresses must be equal, so compare the source ports
	if (node_a->srcport < node_b->srcport) {
		return -1;
	} else if (node_a->srcport > node_b->srcport) {
		return 1;
	}

	// if we get to here the source ports must be equal, so compare the dst ports
	if (node_a->dstport < node_b->dstport) {
		return -1;
	} else if (node_a->dstport > node_b->dstport) {
		return 1;
	}

	// if we get to here, both are identical
	return 0;
}


gint g_tree_compare_unique_flow(gconstpointer node_a_in, gconstpointer node_b_in, gpointer user_data)
{
	unique_flow_node_t *node_a = (unique_flow_node_t *) node_a_in;
	unique_flow_node_t *node_b = (unique_flow_node_t *) node_b_in;

	// check if the src addresses are less than or greater than the other
	if (node_a->srcip < node_b->srcip) {
		return -1;
	} else if (node_a->srcip > node_b->srcip) {
		return 1;
	}

	// check if the dst addresses are less than or greater than the other
	if (node_a->dstip < node_b->dstip) {
		return -1;
	} else if (node_a->dstip > node_b->dstip) {
		return 1;
	}

	// if we get to here the addresses must be equal, so compare the source ports
	if (node_a->srcport < node_b->srcport) {
		return -1;
	} else if (node_a->srcport > node_b->srcport) {
		return 1;
	}

	// if we get to here the source ports must be equal, so compare the dst ports
	if (node_a->dstport < node_b->dstport) {
		return -1;
	} else if (node_a->dstport > node_b->dstport) {
		return 1;
	}
	
	// check protocol numbers
	if (node_a->protocol < node_b->protocol) {
		return -1;
	} else if (node_a->protocol > node_b->protocol) {
		return 1;
	}

	// if we get to here, both are identical
	return 0;
}
