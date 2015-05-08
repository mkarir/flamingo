#include <glib.h>
#include <config.h>

#include "pond.h"
#include "gtree.h"
#include "server.h"
#include "myflow.h"
#include "anomaly_detection.h"

extern pond_t *pond;

#define MAX_MSG_SIZE (sizeof(v5hdr_t) + MAX_REC_PER_MSG * sizeof(v5rec_t))

gboolean check_addr_in_range(guint ipaddr, guint iplen, frange_t *ranges, gint num_ranges)
{
	gint i;
	guint mask, ip_addr_masked, range_addr_masked, netbits;

	for (i = 0; i < num_ranges; ++i) {
		netbits = ranges[i].network_bits;
		// mask off bits of each addr using the specified network bits
		mask = pow(2, 32 - netbits) - 1;
		ip_addr_masked = ipaddr & ~mask;
		range_addr_masked = ranges[i].addr & ~mask;

		// if the two masked addrs are equal, the ip falls within the specified range
		if ( (ip_addr_masked == range_addr_masked) && (iplen >= netbits) ) {
			return TRUE;
		}
	}

	return FALSE;
}


void nf_record_process_src_port(client_t *client, v5rec_t *rec)
{
	gpointer port_tmp;
	port_node_t *port_node;
	port_node_t port_lookup;
	longest_prefix_t src;
	ip_node_t *ip_node;
	
	// check if this feed is aggregated
	if (client->mode.aggregated) {
		// check to see if the src ip is in our master tree
		src.ip = rec->srcip;
		src.longest_prefix = NULL;
		g_tree_search(pond->master_ip, g_tree_search_longest_prefix, &src);

		// make sure a match was found
		if (!src.longest_prefix) {
			return;
		}

		// found a match!
		ip_node = src.longest_prefix;


		// generate our lookup key
		guint mask = pow(2, 32 - ip_node->network_bits) - 1;
		port_lookup.ip = rec->srcip & ~mask;
		port_lookup.len = ip_node->network_bits;
		port_lookup.port = rec->srcport;
		port_lookup.count = rec->packets;
	} else {
		// generate our lookup key
		port_lookup.ip = rec->srcip;
		port_lookup.len = 32;
		port_lookup.port = rec->srcport;
		port_lookup.count = rec->packets;
	}
	
	// check if the src ip address is in one of the src ranges we are tracking
	if (!check_addr_in_range(rec->srcip, port_lookup.len, client->range_src, client->mode.ranges_src))
		return;
	
	
	// lookup to see if the entry is already in the tree
	if (g_tree_lookup_extended(client->node_tree, &port_lookup, &port_tmp, NULL)) {
		// if exists in tree, increments its stats
		port_node = (port_node_t *) port_tmp;
		port_node->count += rec->packets;
	} else {
		// otherwise, allocate and insert the new src_port entry
		port_node = g_new0(port_node_t, 1);
		memcpy(port_node, &port_lookup, sizeof(port_node_t));
		g_tree_insert(client->node_tree, port_node, NULL);
	}
}


void nf_record_process_dst_port(client_t *client, v5rec_t *rec)
{
	gpointer port_tmp;
	port_node_t *port_node;
	port_node_t port_lookup;
	longest_prefix_t dst;
	ip_node_t *ip_node;
	
	// check if this feed is aggregated
	if (client->mode.aggregated) {
		// check to see if the dst ip is in our master tree
		dst.ip = rec->dstip;
		dst.longest_prefix = NULL;
		g_tree_search(pond->master_ip, g_tree_search_longest_prefix, &dst);

		// make sure a match was found
		if (!dst.longest_prefix) {
			return;
		}

		// found a match!
		ip_node = dst.longest_prefix;


		// generate our lookup key
		guint mask = pow(2, 32 - ip_node->network_bits) - 1;
		port_lookup.ip = rec->dstip & ~mask;
		port_lookup.len = ip_node->network_bits;
		port_lookup.port = rec->dstport;
		port_lookup.count = rec->packets;
	} else {
		// generate our lookup key
		port_lookup.ip = rec->dstip;
		port_lookup.len = 32;
		port_lookup.port = rec->dstport;
		port_lookup.count = rec->packets;
	}
	
	// check if the dst ip address is in one of the dst ranges we are tracking
	if (!check_addr_in_range(rec->dstip, port_lookup.len, client->range_dst, client->mode.ranges_dst))
		return;
	
	// lookup to see if the entry is already in the tree
	if (g_tree_lookup_extended(client->node_tree, &port_lookup, &port_tmp, NULL)) {
		// if exists in tree, increments its stats
		port_node = (port_node_t *) port_tmp;
		port_node->count += rec->packets;
	} else {
		// otherwise, allocate and insert the new dst_port entry
		port_node = g_new0(port_node_t, 1);
		memcpy(port_node, &port_lookup, sizeof(port_node_t));
		g_tree_insert(client->node_tree, port_node, NULL);
	}
}


void nf_record_process_src_dst_ip(client_t *client, v5rec_t *rec)
{
	gpointer src_dst_ip_tmp;
	src_dst_ip_node_t *src_dst_ip_node;
	src_dst_ip_node_t src_dst_ip_lookup;
	longest_prefix_t src, dst;
	ip_node_t *src_ip_node, *dst_ip_node;
	
	// check if this feed is aggregated
	if(client->mode.aggregated){
	
		// check to see if the src ip is in our master tree
		src.ip = rec->srcip;
		src.longest_prefix = NULL;
		g_tree_search(pond->master_ip, g_tree_search_longest_prefix, &src);

		// make sure a match was found
		if (!src.longest_prefix) {
			return;
		}

		// found a match!
		src_ip_node = src.longest_prefix;


		// check to see if the dst ip is in our master tree
		dst.ip = rec->dstip;
		dst.longest_prefix = NULL;
		g_tree_search(pond->master_ip, g_tree_search_longest_prefix, &dst);

		// make sure a match was found
		if (!dst.longest_prefix) {
			return;
		}

		// found a match!
		dst_ip_node = dst.longest_prefix;


		// generate our lookup key
		guint src_mask = pow(2, 32 - src_ip_node->network_bits) - 1;
		guint dst_mask = pow(2, 32 - dst_ip_node->network_bits) - 1;
		src_dst_ip_lookup.srcip = rec->srcip & ~src_mask;
		src_dst_ip_lookup.srclen = src_ip_node->network_bits;
		src_dst_ip_lookup.dstip = rec->dstip & ~dst_mask;
		src_dst_ip_lookup.dstlen = dst_ip_node->network_bits;
		src_dst_ip_lookup.count = rec->packets;

	}else{
		// generate our lookup key
		src_dst_ip_lookup.srcip = rec->srcip;
		src_dst_ip_lookup.srclen = 32;
		src_dst_ip_lookup.dstip = rec->dstip;
		src_dst_ip_lookup.dstlen = 32;
		src_dst_ip_lookup.count = rec->packets;
	}
	
	// check if the src ip address is in one of the src ranges we are tracking
	if( !check_addr_in_range(rec->srcip, src_dst_ip_lookup.srclen, client->range_src, client->mode.ranges_src) )
		return;
		
	// check if the dst ip address is in one of the dst ranges we are tracking
	if( !check_addr_in_range(rec->dstip, src_dst_ip_lookup.dstlen, client->range_dst, client->mode.ranges_dst) )
		return;
	
	// lookup to see if the entry is already in the tree
	if (g_tree_lookup_extended(client->node_tree, &src_dst_ip_lookup, &src_dst_ip_tmp, NULL)) {
		// if exists in tree, increments its stats
		src_dst_ip_node = (src_dst_ip_node_t *) src_dst_ip_tmp;
		src_dst_ip_node->count += rec->packets;
	} else {
		// otherwise, allocate and insert the new src_dst_ip entry
		src_dst_ip_node = g_new0(src_dst_ip_node_t, 1);
		memcpy(src_dst_ip_node, &src_dst_ip_lookup, sizeof(src_dst_ip_node_t));
		g_tree_insert(client->node_tree, src_dst_ip_node, NULL);
	}
}


void nf_record_process_src_dst_as(client_t *client, v5rec_t *rec)
{
	gpointer src_dst_as_tmp;
	src_dst_as_node_t *src_dst_as_node;
	src_dst_as_node_t src_dst_as_lookup;

	// generate our lookup key
	src_dst_as_lookup.srcas = rec->srcas;
	src_dst_as_lookup.dstas = rec->dstas;
	src_dst_as_lookup.count = rec->packets;

	// lookup to see if the entry is already in the tree
	if (g_tree_lookup_extended(client->node_tree, &src_dst_as_lookup, &src_dst_as_tmp, NULL)) {
		// if exists in tree, increments its stats
		src_dst_as_node = (src_dst_as_node_t *) src_dst_as_tmp;
		src_dst_as_node->count += rec->packets;
	} else {
		// otherwise, allocate and insert the new src_dst_as entry
		src_dst_as_node = g_new0(src_dst_as_node_t, 1);
		memcpy(src_dst_as_node, &src_dst_as_lookup, sizeof(src_dst_as_node_t));
		g_tree_insert(client->node_tree, src_dst_as_node, NULL);
	}
}


void nf_record_process_src_dst_port(client_t *client, v5rec_t *rec)
{
	gpointer src_dst_port_tmp;
	src_dst_port_node_t *src_dst_port_node;
	src_dst_port_node_t src_dst_port_lookup;
	longest_prefix_t src, dst;
	ip_node_t *src_ip_node, *dst_ip_node;
	
	// check if this feed is aggregated
	if(client->mode.aggregated){
		// check to see if the src ip is in our master tree
		src.ip = rec->srcip;
		src.longest_prefix = NULL;
		g_tree_search(pond->master_ip, g_tree_search_longest_prefix, &src);

		// make sure a match was found
		if (!src.longest_prefix) {
			return;
		}

		// found a match!
		src_ip_node = src.longest_prefix;


		// check to see if the dst ip is in our master tree
		dst.ip = rec->dstip;
		dst.longest_prefix = NULL;
		g_tree_search(pond->master_ip, g_tree_search_longest_prefix, &dst);

		// make sure a match was found
		if (!dst.longest_prefix) {
			return;
		}

		// found a match!
		dst_ip_node = dst.longest_prefix;


		// generate our lookup key
		guint src_mask = pow(2, 32 - src_ip_node->network_bits) - 1;
		guint dst_mask = pow(2, 32 - dst_ip_node->network_bits) - 1;
		src_dst_port_lookup.srcip = rec->srcip & ~src_mask;
		src_dst_port_lookup.srclen = src_ip_node->network_bits;
		src_dst_port_lookup.srcport = rec->srcport;
		src_dst_port_lookup.dstip = rec->dstip & ~dst_mask;
		src_dst_port_lookup.dstlen = dst_ip_node->network_bits;
		src_dst_port_lookup.dstport = rec->dstport;
		src_dst_port_lookup.count = rec->packets;
		src_dst_port_lookup.proto = rec->protocol;
	}else{
		// generate our lookup key
		src_dst_port_lookup.srcip = rec->srcip;
		src_dst_port_lookup.srclen = 32;
		src_dst_port_lookup.srcport = rec->srcport;
		src_dst_port_lookup.dstip = rec->dstip;
		src_dst_port_lookup.dstlen = 32;
		src_dst_port_lookup.dstport = rec->dstport;
		src_dst_port_lookup.count = rec->packets;
		src_dst_port_lookup.proto = rec->protocol;
	}
	
	// check if the src ip address is in one of the src ranges we are tracking
	if( !check_addr_in_range(rec->srcip, src_dst_port_lookup.srclen, client->range_src, client->mode.ranges_src) )
		return;
		
	// check if the dst ip address is in one of the dst ranges we are tracking
	if( !check_addr_in_range(rec->dstip, src_dst_port_lookup.dstlen, client->range_dst, client->mode.ranges_dst) )
		return;
		
	// lookup to see if the entry is already in the tree
	if (g_tree_lookup_extended(client->node_tree, &src_dst_port_lookup, &src_dst_port_tmp, NULL)) {
		// if exists in tree, increments its stats
		src_dst_port_node = (src_dst_port_node_t *) src_dst_port_tmp;
		src_dst_port_node->count += rec->packets;
	} else {
		// otherwise, allocate and insert the new src_dst_port entry
		src_dst_port_node = g_new0(src_dst_port_node_t, 1);
		memcpy(src_dst_port_node, &src_dst_port_lookup, sizeof(src_dst_port_node_t));
		g_tree_insert(client->node_tree, src_dst_port_node, NULL);
	}
}


void nf_record_process_src_as(client_t *client, v5rec_t *rec)
{
	gpointer as_tmp;
	as_node_t *as_node;
	as_node_t as_lookup;

	// Source AS
	as_lookup.as_num = rec->srcas;

	// lookup as number
	if (g_tree_lookup_extended(client->node_tree, &as_lookup, &as_tmp, NULL)) {
		// if exists in tree, increments its stats
		as_node = (as_node_t *) as_tmp;
		as_node->count += rec->packets;
	} else {
		// otherwise, allocate and insert the new as number
		as_node = g_new0(as_node_t, 1);
		as_node->as_num = rec->srcas;
		as_node->count = rec->packets;
		g_tree_insert(client->node_tree, as_node, NULL);
	}
}


void nf_record_process_dst_as(client_t *client, v5rec_t *rec)
{
	gpointer as_tmp;
	as_node_t *as_node;
	as_node_t as_lookup;

	// Dst AS
	as_lookup.as_num = rec->dstas;

	// lookup as number
	if (g_tree_lookup_extended(client->node_tree, &as_lookup, &as_tmp, NULL)) {
		// if exists in tree, increments its stats
		as_node = (as_node_t *) as_tmp;
		as_node->count += rec->packets;
	} else {
		// otherwise, allocate and insert the new as number
		as_node = g_new0(as_node_t, 1);
		as_node->as_num = rec->dstas;
		as_node->count = rec->packets;
		g_tree_insert(client->node_tree, as_node, NULL);
	}
}


void nf_record_process_dst_ip(client_t *client, v5rec_t *rec)
{
	gpointer ip_tmp;
	ip_node_t *ip_node;
	ip_node_t ip_lookup;
	longest_prefix_t args;
	
	// check if this feed is aggregated
	if(client->mode.aggregated){
		// Dst IP
		args.ip = rec->dstip;
		args.longest_prefix = NULL;
		g_tree_search(pond->master_ip, g_tree_search_longest_prefix, &args);

		// make sure a match was found
		if (!args.longest_prefix) {
			return;
		}

		// found a match!
		ip_node = args.longest_prefix;

		// generate our lookup key
		guint mask = pow(2, 32 - ip_node->network_bits) - 1;
		ip_lookup.addr = rec->dstip & ~mask;
		ip_lookup.network_bits = ip_node->network_bits;
		ip_lookup.count = rec->packets;
	}else{
		// generate our lookup key
		ip_lookup.addr = rec->dstip;
		ip_lookup.network_bits = 32;
		ip_lookup.count = rec->packets;
	}
	
	// check if the dst ip address is in one of the ranges we are tracking
	if( !check_addr_in_range(rec->dstip, ip_lookup.network_bits, client->range_dst, client->mode.ranges_dst) )
		return;
		
	// lookup to see if the entry is already in the tree
	if (g_tree_lookup_extended(client->node_tree, &ip_lookup, &ip_tmp, NULL)) {
		// if exists in tree, increments its stats
		ip_node = (ip_node_t *) ip_tmp;
		ip_node->count += rec->packets;
	} else {
		// otherwise, allocate and insert the new ip entry
		ip_node = g_new0(ip_node_t, 1);
		memcpy(ip_node, &ip_lookup, sizeof(ip_node_t));
		g_tree_insert(client->node_tree, ip_node, NULL);
	}
}


void nf_record_process_src_ip(client_t *client, v5rec_t *rec)
{
	gpointer ip_tmp;
	ip_node_t *ip_node;
	ip_node_t ip_lookup;
	longest_prefix_t args;
	
	// check if this feed is aggregated
	if(client->mode.aggregated){
		// source IP
		args.ip = rec->srcip;
		args.longest_prefix = NULL;
		g_tree_search(pond->master_ip, g_tree_search_longest_prefix, &args);

		// make sure a match was found
		if (!args.longest_prefix) {
			return;
		}

		// found a match!
		ip_node = args.longest_prefix;
		
		// generate our lookup key
		guint mask = pow(2, 32 - ip_node->network_bits) - 1;
		ip_lookup.addr = rec->srcip & ~mask;
		ip_lookup.network_bits = ip_node->network_bits;
		ip_lookup.count = rec->packets;
	}else{
		// generate our lookup key
		ip_lookup.addr = rec->srcip;
		ip_lookup.network_bits = 32;
		ip_lookup.count = rec->packets;
	}
	
	// check if the src ip address is in one of the ranges we are tracking
	if( !check_addr_in_range(rec->srcip, ip_lookup.network_bits, client->range_src, client->mode.ranges_src) )
		return;
	
	// lookup to see if the entry is already in the tree
	if (g_tree_lookup_extended(client->node_tree, &ip_lookup, &ip_tmp, NULL)) {
		// if exists in tree, increments its stats
		ip_node = (ip_node_t *) ip_tmp;
		ip_node->count += rec->packets;
	} else {
		// otherwise, allocate and insert the new ip entry
		ip_node = g_new0(ip_node_t, 1);
		memcpy(ip_node, &ip_lookup, sizeof(ip_node_t));
		g_tree_insert(client->node_tree, ip_node, NULL);
	}
}


void nf_record_process(guint feed_num, guint time_stamp, v5rec_t *rec)
{
	guint i;
	client_t *client;

	// pond->clients reader lock
	g_static_rw_lock_reader_lock(&pond->clients_rwlock);

	// add netflow record to each client's tree
	for (i = 0; i < pond->clients->len; ++i) {
		client = g_ptr_array_index(pond->clients, i);

		// write lock for this specific client tree
		g_static_rw_lock_writer_lock(&client->rwlock);
		
					
		// make sure the client is live and actively using this data type 
		if (!client->live) {
			g_static_rw_lock_writer_unlock(&client->rwlock);
			continue;
		}
	

		client->time = time_stamp;

		// only process the record if its from the feed that the client is subscribed to
		if (client->mode.exporter == feed_num){

			// check the rec against registered filters
			g_datalist_foreach(&client->watch_list, watch_list_foreach_evaluate_filter, rec);

			// only process the data type that the client is subscribed to
			switch (client->mode.data_type) {
				case SRC_IP:
					nf_record_process_src_ip(client, rec);
				break;

				case DST_IP:
					nf_record_process_dst_ip(client, rec);
				break;

				case SRC_AS:
					nf_record_process_src_as(client, rec);
				break;

				case DST_AS:
					nf_record_process_dst_as(client, rec);
				break;

				case SRC_PORT:
					nf_record_process_src_port(client, rec);
				break;

				case DST_PORT:
					nf_record_process_dst_port(client, rec);
				break;

				case SRC_DST_IP:
					nf_record_process_src_dst_ip(client, rec);
				break;

				case SRC_DST_AS:
					nf_record_process_src_dst_as(client, rec);
				break;

				case SRC_DST_PORT:
					nf_record_process_src_dst_port(client, rec);
				break;
			}
		}

		// unlock write lock for client tree
		g_static_rw_lock_writer_unlock(&client->rwlock);
	}

	// unlock pond->clients reader lock
	g_static_rw_lock_reader_unlock(&pond->clients_rwlock);
}


gpointer nf_feed_thread(gpointer feed_in)
{
	guint i;
	nf_feed_t *feed = (nf_feed_t *) feed_in;

	struct sockaddr_in tmpaddr;
	gint ssize = sizeof(tmpaddr);
	gchar tmpbuf[MAX_MSG_SIZE];
	v5hdr_t *header;
	v5rec_t *rec;
	gint recv_bytes;
	guint time_stamp;
	
	watch_list_stash = g_private_new(NULL);
	g_private_set(watch_list_stash, NULL);
	
	while (TRUE) {
		recv_bytes = recvfrom(feed->sock, tmpbuf, MAX_MSG_SIZE, 0, (struct sockaddr *) &tmpaddr, &ssize);

		if (recv_bytes >= sizeof(v5hdr_t)) {
			header = (v5hdr_t *) tmpbuf;

			// check for netflow version 5
			if (g_ntohs(header->version) == 5) {
				// make sure the size of the records matches up with the number received
				if ((sizeof(v5hdr_t) + sizeof(v5rec_t) * g_ntohs(header->count)) == recv_bytes) {

					//already in network byte order
					time_stamp = g_ntohl(header->sysTime_sec);

					// loop through each record
					for (i = 0; i < g_ntohs(header->count); ++i) {
						if (g_random_int() % 100 > feed->samplerate) {
							continue;
						}

						rec = ((v5rec_t *) (tmpbuf + sizeof(v5hdr_t) + i * sizeof(v5rec_t)));

						// network to host byte conversions
						rec->srcip = g_ntohl(rec->srcip);
						rec->dstip = g_ntohl(rec->dstip);
						rec->srcport = g_ntohs(rec->srcport);
						rec->dstport = g_ntohs(rec->dstport);
						rec->srcas = g_ntohs(rec->srcas);
						rec->dstas = g_ntohs(rec->dstas);
						rec->packets = g_ntohl(rec->packets) * feed->scalefactor;

						// process the netflow record
						nf_record_process(feed->id, time_stamp, rec);
					}
				}
			} else {
				g_warning("unsupported netflow version");
			}
		}
	}

	return NULL;
}


gboolean nf_feed_bind(nf_feed_t *feed)
{
	struct sockaddr_in tmpaddr;

	// create udp socket
	if ((feed->sock = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
    		g_warning("socket could not be created");
    		return FALSE;
	}

	memset((gpointer) &tmpaddr, 0, sizeof(struct sockaddr_in));

	tmpaddr.sin_family = AF_INET;
	tmpaddr.sin_addr.s_addr = INADDR_ANY;
	tmpaddr.sin_port = g_htons(feed->port);

	// bind socket
	if (bind(feed->sock, (struct sockaddr *) &tmpaddr, sizeof(tmpaddr)) == -1){ 
		g_warning("could not bind to socket");
		close(feed->sock);
		return FALSE;
	}

	g_debug("feed %s bound successfully to socket", feed->name);

	return TRUE;
}


gchar* tomorrow_first_file(struct tm *ts, gchar* feed){
	gchar *path;
	
	//get the first file from the next day
	if((ts->tm_mon+1 == 12) && (ts->tm_mday == 31)){			//dec 31
		ts->tm_year++;
		ts->tm_mon = 0;
		ts->tm_mday	= 1;
	}else if( ((ts->tm_mon+1 == 1) || (ts->tm_mon+1 == 3) || (ts->tm_mon+1 == 5) || (ts->tm_mon+1 == 7) || (ts->tm_mon+1 == 8) || (ts->tm_mon+1 == 10)) && (ts->tm_mday == 31) ){		//31st day of long month
		ts->tm_mon++;
		ts->tm_mday	= 1;
	}else if( ((ts->tm_mon+1 == 4) || (ts->tm_mon+1 == 6) || (ts->tm_mon+1 == 9) || (ts->tm_mon+1 == 11)) && (ts->tm_mday == 30) ){													//30th day of short month
		ts->tm_mon++;
		ts->tm_mday	= 1;
	}else if( (ts->tm_mon+1 == 2) && (ts->tm_mday == 28) ){		//28th day of february
		ts->tm_mon++;
		ts->tm_mday	= 1;
	}else{														//midnight
		ts->tm_mday++;
	}
	
	path = g_malloc0(250);
	snprintf(path, 250, "%s%s/%04d/%4d-%02d/%04d-%02d-%02d/ft-v05.*", pond->archive_path, feed, ts->tm_year+1900, ts->tm_year+1900, ts->tm_mon+1, ts->tm_year+1900, ts->tm_mon+1, ts->tm_mday);
	
	return path;
}


gboolean get_initial_file(pbtracker_t *tracker)
{
	struct tm *ts;
	time_t ti;
	glob_t fnames;
	gchar *path, *filename;
	gint pathlen, i;
	
	ti = tracker->start;
	ts = localtime(&ti);
	 
	//construct search path
	path = g_malloc0(250);
	snprintf(path, 250, "%s%s/%04d/%4d-%02d/%04d-%02d-%02d/ft-v05.*", pond->archive_path, tracker->fname, ts->tm_year+1900, ts->tm_year+1900, ts->tm_mon+1, ts->tm_year+1900, ts->tm_mon+1, ts->tm_mday);
	//g_debug("Searching In Path: %s", path);

	//construct ideal file (ie tracker->start == time index of file)
	filename = g_malloc0(250);
	snprintf(filename, 250, "ft-v05.%04d-%02d-%02d.%02d%02d%02d-0600", ts->tm_year+1900, ts->tm_mon+1, ts->tm_mday, ts->tm_hour, ts->tm_min, ts->tm_sec+10);
	//g_debug("Searching for File: %s", filename);

	glob(path, 0, NULL, &fnames);

	//g_debug("%d files in folder", fnames.gl_pathc);

	pathlen = strlen(path) - 8;

	//folder does not exist or no files in folder
	if(fnames.gl_pathc == 0){
		g_debug("No data for this date.");
	
		g_free(path);
		g_free(filename);
	
		return FALSE;
	}

	//g_debug("first file: '%s'", &fnames.gl_pathv[0][pathlen]);

	//check that time is not before first file in folder
	if( strncmp(filename, &fnames.gl_pathv[0][pathlen], strlen(filename)) < 0){
		g_debug("ERROR:Time too Early - Not Caught By Initial Check!!!");	
	
		g_free(path);
		g_free(filename);
	
		return FALSE;
	}

	//search through the file in the folder
	for(i=0; i<fnames.gl_pathc; i++){
		if( strncmp(filename, &fnames.gl_pathv[i][pathlen], strlen(filename)) <= 0){
			tracker->file_index = i-1;
			tracker->ts = g_new0(struct tm, 1);
			memcpy(tracker->ts, ts, sizeof(struct tm));

			g_debug("Opening File: %s", &fnames.gl_pathv[i-1][pathlen]);
			tracker->file = fopen(fnames.gl_pathv[i-1], "rb");
							
			g_free(path);
			g_free(filename);				
		
			return TRUE;
		}
	}
	
	//check if time is in last file of the day
	if(ts->tm_min < 59){
		tracker->file_index = i-1;
		tracker->ts = g_new0(struct tm, 1);
		memcpy(tracker->ts, ts, sizeof(struct tm));

		g_debug("Opening File: %s", &fnames.gl_pathv[i-1][pathlen]);
		tracker->file = fopen(fnames.gl_pathv[i-1], "rb");
		
		g_free(path);
		g_free(filename);
		
		return TRUE;
	}

	g_free(path);

	path = tomorrow_first_file(ts, tracker->fname);

	glob(path, 0, NULL, &fnames);

	//folder does not exist or no files in folder
	if(fnames.gl_pathc == 0){
		g_debug("ERROR:Time too Late - Not Caught By Initial Check!!!");

		g_free(path);
		g_free(filename);

		return FALSE;
	}
	
	tracker->file_index = 0;
	tracker->ts = g_new0(struct tm, 1);
	memcpy(tracker->ts, ts, sizeof(struct tm));

	g_debug("Opening File: %s", &fnames.gl_pathv[0][pathlen]);
	tracker->file = fopen(fnames.gl_pathv[0], "rb");
	
	g_free(path);
	g_free(filename);
	
	return TRUE;
}


gboolean get_next_file(pbtracker_t *tracker){
	gchar *path;
	struct tm *ts = tracker->ts;
	glob_t fnames;
	gint pathlen;
	
	path = g_malloc0(250);
	snprintf(path, 250, "%s%s/%04d/%4d-%02d/%04d-%02d-%02d/ft-v05.*", pond->archive_path, tracker->fname, ts->tm_year+1900, ts->tm_year+1900, ts->tm_mon+1, ts->tm_year+1900, ts->tm_mon+1, ts->tm_mday);
	//g_debug("Searching In Path: %s", path);
	
	glob(path, 0, NULL, &fnames);

	pathlen = strlen(path) - 8;
	
	if(tracker->file_index+1 < fnames.gl_pathc){
		tracker->file_index = tracker->file_index+1;
		
		g_debug("Opening File: %s", &fnames.gl_pathv[tracker->file_index][pathlen] );
		tracker->file = fopen(fnames.gl_pathv[tracker->file_index], "rb");
	}else{
		g_free(path);
		path = tomorrow_first_file(tracker->ts, tracker->fname);
		
		glob(path, 0, NULL, &fnames);
		
		//check if there are files in the folder
		if(fnames.gl_pathc == 0){
			g_debug("ERROR:Time too Late - Not Caught By Initial Check!!!");
			g_free(path);

			return FALSE;
		}
		
		tracker->file_index = 0;
		
		g_debug("Opening File: %s", &fnames.gl_pathv[0][pathlen] );
		tracker->file = fopen(fnames.gl_pathv[0], "rb");
	}
	
	g_free(path);
	
	return TRUE;
}


void load_archive_data(client_t *client){
	pbtracker_t *tracker = client->tracker;
	gchar *rec;
	static struct fts3rec_offsets fo;
	static struct ftver ftv;
	v5rec_t recv5;
	gint nextbreak;
	
	//check if this is a new playback
	if(tracker->file == NULL){
		
		if( !get_initial_file(tracker) ){
			g_warning("Failed to Open Archive File");
			g_warning("EXITING PB_THRD");
			
			tracker->completed = TRUE;
			return;
		}

		//initialize flow-tools type file handler
		if (ftio_init(&tracker->handle, fileno(tracker->file), FT_IO_FLAG_READ) < 0) {
			fterr_errx(1, "ftio_init(): failed");
		}
		
		ftio_get_ver(&tracker->handle, &ftv);
		fts3rec_compute_offsets(&fo, &ftv);
		
		//if the current file index is before the start time fast-forward to the start time
		while ((rec = ftio_read(&tracker->handle)) && ((*((guint32*)(rec+fo.unix_secs))) < tracker->start))
			tracker->current = *((guint32*)(rec+fo.unix_secs));
	}
	
	nextbreak = tracker->current + (gint)ceil(( (gfloat)(tracker->rate*client->mode.update_period))/5.0)*5 - 5;
	
	while( (rec = ftio_read(&tracker->handle)) && (( tracker->current = *((guint32*)(rec+fo.unix_secs)) ) <= nextbreak)){
		//tracker->current = *((guint32*)(rec+fo.unix_secs));

		//create pseudo netflow packet to process
		recv5.srcip = *((guint32*)(rec+fo.srcaddr));
		recv5.dstip = *((guint32*)(rec+fo.dstaddr));
		recv5.srcport = *((guint16*)(rec+fo.srcport));
		recv5.dstport = *((guint16*)(rec+fo.dstport));
		recv5.srcas = *((u_int16*)(rec+fo.src_as));
		recv5.dstas = *((u_int16*)(rec+fo.dst_as));
		recv5.protocol = *((u_int8*)(rec+fo.prot));
		recv5.packets =  *((guint32*)(rec+fo.dPkts)) * tracker->scaler;
		
		//tag client with most recent playback time
		client->time = tracker->current;

		// check the rec against registered filters
		g_datalist_foreach(&client->watch_list, watch_list_foreach_evaluate_filter, rec);

		//process record into client data tree
		switch (client->mode.data_type) {
			case SRC_IP:
				nf_record_process_src_ip(client, &recv5);
			break;

			case DST_IP:
				nf_record_process_dst_ip(client, &recv5);
			break;

			case SRC_AS:
				nf_record_process_src_as(client, &recv5);
			break;

			case DST_AS:
				nf_record_process_dst_as(client, &recv5);
			break;

			case SRC_PORT:
				nf_record_process_src_port(client, &recv5);
			break;

			case DST_PORT:
				nf_record_process_dst_port(client, &recv5);
			break;

			case SRC_DST_PORT:
				nf_record_process_src_dst_port(client, &recv5);
			break;

			case SRC_DST_IP:
				nf_record_process_src_dst_ip(client, &recv5);
			break;

			case SRC_DST_AS:
				nf_record_process_src_dst_as(client, &recv5);
			break;
		}
	}
	
	if(!rec){
		//reached the end of a file - load next file
		g_debug("Closing File");
		fclose(tracker->file);
		tracker->file = NULL;
		
		if( get_next_file(tracker) ){
			//initialize flow-tools type file handler
			if (ftio_init(&tracker->handle, fileno(tracker->file), FT_IO_FLAG_READ) < 0) {
				fterr_errx(1, "ftio_init(): failed");
			}
			
			ftio_get_ver(&tracker->handle, &ftv);
			fts3rec_compute_offsets(&fo, &ftv);
			
			//if the current file index is before the start time fast-forward to the start time
			while ((rec = ftio_read(&tracker->handle)) && ((*((guint32*)(rec+fo.unix_secs))) < tracker->start))
				tracker->current = *((guint32*)(rec+fo.unix_secs));
		}else{
			tracker->completed = TRUE;
		}
		
	}else if(tracker->current > tracker->end){
		//reached the end of the playback period - end playback
		g_debug("Closing File");
		fclose(tracker->file);
		tracker->file = NULL;
		tracker->completed = TRUE;
	}

}
			

guint getEarliest(gchar *path)
{
	glob_t fnames;
	FILE *afile;
	gchar tmp[250], name[250];
	guint i, rv;
	struct tm ts;

	memset(tmp, 0, 250);
	snprintf(tmp, 250, "%s/*", path);
	glob(tmp, 0, NULL, &fnames);

	for(i=0, rv=0; (i<fnames.gl_pathc) && (rv == 0); i++){
		//g_debug("checking: %s", fnames.gl_pathv[i]);
		//if(fnames.gl_pathv[i][strlen(path)+1] == 'f'){
		if( strncmp( &(fnames.gl_pathv[i][strlen(path)+1]), "ft-v05.", 7) == 0){
			if((afile = fopen(fnames.gl_pathv[i], "rb")) != NULL){
				fclose(afile);

				//convert filename to unix sec and return
				strncpy(name, &fnames.gl_pathv[i][strlen(path)+1], 250);

				memset(tmp, 0, 250);
				memset(&ts, 0, sizeof(struct tm));
				
				ts.tm_mon = atoi( strncpy(tmp, &name[12], 2) ) - 1;		//month
				ts.tm_mday = atoi( strncpy(tmp, &name[15], 2) );		//day
				ts.tm_hour = atoi( strncpy(tmp, &name[18], 2) ) - 1;	//hour
				ts.tm_min = atoi( strncpy(tmp, &name[20], 2) );			//minute
				ts.tm_sec = atoi( strncpy(tmp, &name[22], 2) );			//second
				ts.tm_year = atoi( strncpy(tmp, &name[7], 4) ) - 1900;	//year
				
				return (gint)mktime(&ts); 
			}else{
				fclose(afile);
			}
		}else{
			if( (rv = getEarliest(fnames.gl_pathv[i])) > 0)
				return rv;
		}
	}
	
	return rv;
}


guint getLatest(gchar *path)
{
	glob_t fnames;
	FILE *afile;
	gchar tmp[250], name[250];
	gint i, rv;
	struct tm ts;
	
	memset(tmp, 0, 250);
	snprintf(tmp, 250, "%s/*", path);
	glob(tmp, 0, NULL, &fnames);

	for(i=fnames.gl_pathc-1, rv=0; (i>=0) && (rv == 0); i--){
		if(fnames.gl_pathv[i][strlen(path)+1] == 'f'){
		
			if((afile = fopen(fnames.gl_pathv[i], "rb")) != NULL){
				fclose(afile);

				//convert filename to unix sec and return
				strncpy(name, &fnames.gl_pathv[i][strlen(path)+1], 250);

				memset(tmp, 0, 250);
				memset(&ts, 0, sizeof(struct tm));
				
				
				
				ts.tm_mon = atoi( strncpy(tmp, &name[12], 2) ) - 1;		//month
				ts.tm_mday = atoi( strncpy(tmp, &name[15], 2) );		//day
				ts.tm_hour = atoi( strncpy(tmp, &name[18], 2) ) -1;		//hour
				ts.tm_min = atoi( strncpy(tmp, &name[20], 2) ) + 15;	//minute
				ts.tm_sec = atoi( strncpy(tmp, &name[22], 2) );			//second
				ts.tm_year = atoi( strncpy(tmp, &name[7], 4) ) - 1900;	//year
				ts.tm_isdst = -1;
				mktime(&ts);
				printf("\n\nDST=%d\n\n", ts.tm_isdst);
				

				return (gint)mktime(&ts); 
			}else{
				fclose(afile);
			}
		}else{
			if( (rv = getLatest(fnames.gl_pathv[i])) > 0)
				return rv;
		}
	}

	return rv;
}
