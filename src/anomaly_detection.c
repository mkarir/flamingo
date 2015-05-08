#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <glob.h>
#include <time.h>
#include <glib.h>
#include <config.h>

#include "anomaly_detection.h"
#include "server.h"
#include "pond.h"
#include "icmp.h"
#include "tcp.h"
#include "udp.h"
#include "ip.h"
#include "eth.h"

extern pond_t *pond;


void watch_list_filter_destroy(gpointer data_in)
{
	watch_list_filter_t *data = (watch_list_filter_t*)data_in;

	if (data->indegree_flows) {
		g_tree_destroy(data->indegree_flows);
	}

	if (data->outdegree_flows) {
		g_tree_destroy(data->outdegree_flows);
	}
}


void watch_list_add(client_t *client, wlreq_t *new_request)
{
	watch_list_filter_t *new_filter = g_new0(watch_list_filter_t, 1);

	// adjust index to be 1-based rather than 0-based as required by g_datalist
	new_request->filter_id++;

	// compile the filter into a bpf program
	if (pcap_compile_nopcap(65536, DLT_EN10MB, &new_filter->program, (gchar *) new_request->filter, TRUE, 0) != 0) {
		g_debug("invalid filter expression received by server");
		g_free(new_filter);
		return;
	}

	// check for a valid trigger value
	if (new_request->trigger_val < 0) {
		g_free(new_filter);
		return;
	}

	// check for a valid trigger type
	if (new_request->trigger_type < 0 || new_request->trigger_type > 2) {
		g_free(new_filter);
		return;
	}

	// set the trigger type and value
	new_filter->trigger_val = new_request->trigger_val;
	new_filter->trigger_type = new_request->trigger_type;

	new_filter->packet_count = 0;
	new_filter->indegree_flows = g_tree_new_full(g_tree_compare_unique_flow, NULL, g_free, NULL);
	new_filter->outdegree_flows = g_tree_new_full(g_tree_compare_unique_flow, NULL, g_free, NULL);

	g_debug("alert added - id:%d  trigger_type:%d  trigger_val:%f  string:%s", new_request->filter_id, new_request->trigger_type, new_request->trigger_val, new_request->filter);

	g_datalist_id_set_data_full(&client->watch_list, new_request->filter_id, new_filter, watch_list_filter_destroy);
}


gchar *reconstruct_packet(v5rec_t *rec, guint *size)
{
	gchar *packet = NULL;

	switch (rec->protocol) {
		case 1:
			*size = ETH_HDR_LEN + IP_HDR_LEN + ICMP_HDR_LEN;
			packet = g_malloc0(*size);

			eth_pack_hdr(packet, ETH_ADDR_BROADCAST, ETH_ADDR_BROADCAST, ETH_TYPE_IP);
			ip_pack_hdr(packet + ETH_HDR_LEN, 0, IP_HDR_LEN + ICMP_HDR_LEN, 0, 0, 0, rec->protocol, g_htonl(rec->srcip), g_htonl(rec->dstip));
			icmp_pack_hdr(packet + ETH_HDR_LEN + IP_HDR_LEN, 0, 0);
		break;

		case 6:
			*size = ETH_HDR_LEN + IP_HDR_LEN + TCP_HDR_LEN;
			packet = g_malloc0(*size);

			eth_pack_hdr(packet, ETH_ADDR_BROADCAST, ETH_ADDR_BROADCAST, ETH_TYPE_IP);
			ip_pack_hdr(packet + ETH_HDR_LEN, 0, IP_HDR_LEN + TCP_HDR_LEN, 0, 0, 0, rec->protocol, g_htonl(rec->srcip), g_htonl(rec->dstip));
			tcp_pack_hdr(packet + ETH_HDR_LEN + IP_HDR_LEN, rec->srcport, rec->dstport, 0, 0, rec->flags, 0, 0);
		break;

		case 17:
			*size = ETH_HDR_LEN + IP_HDR_LEN + UDP_HDR_LEN;
			packet = g_malloc0(*size);

			eth_pack_hdr(packet, ETH_ADDR_BROADCAST, ETH_ADDR_BROADCAST, ETH_TYPE_IP);
			ip_pack_hdr(packet + ETH_HDR_LEN, 0, IP_HDR_LEN + UDP_HDR_LEN, 0, 0, 0, rec->protocol, g_htonl(rec->srcip), g_htonl(rec->dstip));
			udp_pack_hdr(packet + ETH_HDR_LEN + IP_HDR_LEN, rec->srcport, rec->dstport, UDP_HDR_LEN);
		break;

		default:
			*size = 0;
			packet = NULL;
		break;
	}

	return packet;
}


void watch_list_foreach_evaluate_filter(GQuark id, gpointer entry_data_in, gpointer eval_data_in)
{
	gint ret;
	guint size = 0;
	gchar *packet;
	watch_list_filter_t *filter = (watch_list_filter_t *) entry_data_in;
	v5rec_t *rec = (v5rec_t *) eval_data_in;
	unique_flow_node_t *key;

	packet = reconstruct_packet(rec, &size);

	if (!packet || size == 0) {
		return;
	}

	ret = bpf_filter(filter->program.bf_insns, packet, size, size);

	g_free(packet);

	if (ret <= 0) {
		return;
	}

	switch (filter->trigger_type) {
		case PKTS_PER_SEC:
			filter->packet_count += rec->packets;
		break;

		case INDEGREE_PER_SEC:
			key = g_new0(unique_flow_node_t, 1);
			key->srcip = rec->srcip;
			key->dstip = rec->dstip;
			key->srcport = rec->srcport;
			key->dstport = rec->dstport;
			key->protocol = rec->protocol;
			
			g_tree_insert(filter->indegree_flows, key, NULL);
		break;

		case OUTDEGREE_PER_SEC:
			key = g_new0(unique_flow_node_t, 1);
			key->srcip = rec->srcip;
			key->dstip = rec->dstip;
			key->srcport = rec->srcport;
			key->dstport = rec->dstport;
			key->protocol = rec->protocol;

			g_tree_insert(filter->outdegree_flows, key, NULL);
		break;
	}
}


void watch_list_foreach_send_pending_alert(GQuark filter_id, gpointer filter_in, gpointer alert_buf_in)
{
	filter_alert_buf_t *alert_in = (filter_alert_buf_t *) alert_buf_in;
	watch_list_filter_t *filter = (watch_list_filter_t *) filter_in;
	client_t *client = alert_in->client;
	wlalert_t *alerts = alert_in->alerts;
	guint num_alerts = alert_in->num_alerts;
	wlalert_t *alerts_old;
	gint count = -1;

	g_debug("checking pending alerts for filter %d", filter_id);

	// calculate the threshold based on the update period
	gfloat thresh = filter->trigger_val * (gfloat) client->mode.update_period;

	// adjust the threshold if we're in playback
	if (!client->live && client->tracker) {
		thresh *= (gfloat) client->tracker->rate;
	}

	switch (filter->trigger_type) {
		case PKTS_PER_SEC:
			count = filter->packet_count;
		break;

		case INDEGREE_PER_SEC:
			count = g_tree_nnodes(filter->indegree_flows);
		break;

		case OUTDEGREE_PER_SEC:
			count = g_tree_nnodes(filter->outdegree_flows);
		break;
	}

	if (count > thresh) {
		alerts_old = alerts;
		alerts = g_new0(wlalert_t, num_alerts + 1);
		memcpy(alerts, alerts_old, num_alerts * sizeof(wlalert_t));
		g_free(alerts_old);

		alerts[num_alerts].filter_id = g_htonl(filter_id - 1);
		alerts[num_alerts].time = g_htonl(time(NULL));
		alerts[num_alerts].condition = g_htonl(filter->trigger_type);
		alerts[num_alerts].network_condition = g_htonl(count);
		alert_in->alerts = alerts;
		alert_in->num_alerts++;

		g_debug("added alert to buffer - type:%d  count: %d", filter->trigger_type, count);
	}

	// reset the filter's stats
	switch (filter->trigger_type) {
		case PKTS_PER_SEC:
			filter->packet_count = 0;
		break;

		case INDEGREE_PER_SEC:
			if (filter->indegree_flows) {
				g_tree_destroy(filter->indegree_flows);
			}
			filter->indegree_flows = g_tree_new_full(g_tree_compare_unique_flow, NULL, g_free, NULL);
		break;

		case OUTDEGREE_PER_SEC:
			if (filter->outdegree_flows) {
				g_tree_destroy(filter->outdegree_flows);
			}
			filter->outdegree_flows = g_tree_new_full(g_tree_compare_unique_flow, NULL, g_free, NULL);
		break;
	}
}


void send_pending_alerts(client_t *client)
{
	filter_alert_buf_t *alert_buf;
	msg_header_t header;
	gchar *buffer;
	guint size;
	
	alert_buf = g_new0(filter_alert_buf_t, 1);
	alert_buf->client = client;

	g_datalist_foreach(&client->watch_list, watch_list_foreach_send_pending_alert, alert_buf);

	if (alert_buf->num_alerts == 0) {
		return;
	}
		
	// send buffer of alerts 
	size = alert_buf->num_alerts * sizeof(wlalert_t);

	buffer = g_malloc0(sizeof(msg_header_t) + size);

	header.type = g_htonl(WL_ALERT);
	header.size = g_htonl(size);
	header.extra = g_htonl(0);

	memcpy(buffer, (void *) &header, sizeof(msg_header_t));
	memcpy(buffer + sizeof(msg_header_t), (void *) alert_buf->alerts, size);

	g_debug("sending %d alert(s) to client", alert_buf->num_alerts);
	
	client_socket_write(client, buffer, sizeof(msg_header_t) + size);
	
	g_free(alert_buf->alerts);
	g_free(alert_buf);
	g_free(buffer);
}
