#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include <glib.h>
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// Message Types
#define CONNECT				1		// client requests connection and sends password 
#define TERMINATE				2		// Close the connection
#define EXPORT_LIST				3		// List of available feeds
#define MODE_UPDATE				4		// Client request change in data feed
#define DATA_UPDATE				5		// Sending data to client
#define RESPONSE				6		// Response to a previous message
#define PB_REQUEST				7		// request playback specified by pbreq_t payload
#define PB_PLAY				8		// resume playback
#define PB_PAUSE				9		// pause playback
#define PB_STOP				10		// implicitly resume live updates
#define PB_END					11		// end of playback reached (sent to client)
#define WL_UPDATE				12		// sends a list of the alerts to the server
#define WL_ALERT				13		// alert sent to client 

// Data Types
#define NO_XMIT				0		// send no data
#define SRC_IP					1		// source ip address
#define DST_IP					2		// dst ip address
#define SRC_AS					3		// source AS
#define DST_AS					4		// dst AS
#define SRC_PORT				5		// source port
#define DST_PORT				6		// dst port
#define SRC_DST_IP				7		// src dst ip
#define SRC_DST_AS				8		// src dst as
#define SRC_DST_PORT			9		// srcip,srcport/dstip,dstport

#define DATA_TYPE_MAX			9		// maximum int value of 'Data Types'

// PB Error Types
#define RUNNING				1
#define NO_EXP					2
#define TOO_EARLY				3
#define TOO_LATE				4
	
// alert trigger type
#define PKTS_PER_SEC			0		// pkts
#define INDEGREE_PER_SEC			1		// indegree
#define OUTDEGREE_PER_SEC		2		// outdegree

#define EXPORTER_NAME_LEN			12		// Len of NetFlow Feed Name (no '\0)
#define DEFAULT_UPDATE_INTERVAL		10		// number of seconds between updates
#define CONNECT_MAGIC				0xDEADBEEF	// magic number sent along with connect message
#define MAX_FILTER_LENGTH			256		// max string length of a filter expression

static const char *data_type_str[] = { 
	"",
	"Src IP",
	"Dst IP",
	"Src AS",
	"Dst AS",
	"Src Port",
	"Dst Port",
	"Src/Dst IP",
	"Src/Dst AS",
	"Src/Dst Port"
};


typedef struct {
	gint type;			// message type
	gint size;			// payload size
	gint extra;			// extra multi-purpose payload
} msg_header_t;

typedef struct {
	char name[EXPORTER_NAME_LEN];
	gboolean aggregation;
	gboolean as_availability;
} avail_feed_t;

typedef struct {
	gint exporter;
	gint data_type;
	guint update_period;
	gint ranges_src;
	gint ranges_dst;
	gboolean aggregated;
} fmode_t;

typedef struct {
	guint addr;
	guint network_bits;
} frange_t;

typedef struct {
	gint exporter;
	guint start_time;
	guint end_time;
	gint rate;
} pbreq_t;

typedef struct {
	gint errorcode;
	guint extra;
} pbreq_error_t;

typedef struct {
	gint filter_id;
	guint trigger_type;
	gfloat trigger_val;
	gchar filter[MAX_FILTER_LENGTH];
} wlreq_t;

typedef struct {
	gint filter_id;
	guint time;
	gint condition;
	gint network_condition;
} wlalert_t;

typedef struct {
	guint address;
	guint length;
	guint port;
	guint originas;
	guint dstaddr;
	guint dstlen;
	guint dstport;
	guint dstoriginas;
	guint count;
	guint proto;
	guchar red;
	guchar green;
	guchar blue;
	guchar pad;
	guchar red2;
	guchar green2;
	guchar blue2;
	guchar pad2;
} data_record_t;

#endif
