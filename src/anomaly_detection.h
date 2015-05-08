#ifndef _ANOMALY_DETECTION_H_
#define _ANOMALY_DETECTION_H_

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <glib.h>
#include <config.h>
#include <pcap.h>

#include "gtree.h"
#include "myflow.h"


typedef struct {
	// bpf program to based on user specified expression
	struct bpf_program program;

	// filter trigger type/val
	guint trigger_type;
	gfloat trigger_val;

	// current stats
	guint packet_count;
	GTree *indegree_flows;
	GTree *outdegree_flows;
} watch_list_filter_t;

typedef struct {
	client_t *client;
	wlalert_t *alerts;
	gint num_alerts;
} filter_alert_buf_t;


void watch_list_add(client_t *client, wlreq_t *new_request);
void watch_list_foreach_evaluate_filter(GQuark id, gpointer entry_data_in, gpointer rec_data_in);
void send_pending_alerts(client_t *client);

#endif


