#ifndef _MYFLOW_H_
#define _MYFLOW_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "pond.h"

#define MAX_REC_PER_MSG		30	//Defined by Cisco as part of version 5 spec
#define NF_REC_LEN			160 //Length of NetFlow record as char string

typedef struct {
	guint id;
	gint sock;
	guint port;
	gchar name[EXPORTER_NAME_LEN];
	guint scalefactor;
	guint samplerate;
	gboolean aggregation;
	gboolean as_availability;
} nf_feed_t;

typedef struct v5hdr{
	gushort version;	//NetFlow export format version number 
	gushort count;		//Number of flows exported in this packet (1-30)
	guint upTime;		//Time in milliseconds since device was booted
	guint sysTime_sec;	//Current count of seconds since 0000 UTC 1970
	guint sysTime_usec;	//Residual nanoseconds since 0000 UTC 1970
	guint seqNum;		//Sequence counter of total flows seen
	gchar padding[4];			//zero	
}v5hdr_t;

typedef struct v5rec{
	guint srcip;		//Source IP address
	guint dstip;		//Dst IP address
	guint nexthop;		//IP address of next hop router		
	gushort srcif;		//SNMP index of input interface
	gushort dstif;		//SNMP index of output interface
	guint packets;		//Packets in the flow 
	guint bytes;		//Total number of bytes in the packets of the flow
	guint startTime;	//SysUptime at start of flow
	guint endTime;		//SysUptime at the time the last packet was received 
	gushort srcport;	//TCP/UDP source port number or equivalent 
	gushort dstport;	//TCP/UDP dst port number or equivalent
	guchar padding1;	//Unused (zero) bytes
	guchar flags;		//Cumulative OR of TCP flags
	guchar protocol;	//IP protocol type (for example, TCP = 6; UDP = 17)
	guchar tos;			//IP type of service (ToS)
	gushort srcas;		//AS number of the source, either origin or peer
	gushort dstas;		//AS number of the dst, either origin or peer
	guchar srcmask;		//Source address prefix mask bits
	guchar dsrmask;		//Dst address prefix mask bits 
	gchar padding2[2];			//Unused (zero) bytes
}v5rec_t;

// pass private data into yyparse
GPrivate* watch_list_stash;

gboolean nf_feed_bind(nf_feed_t *feed);
gpointer nf_feed_thread(gpointer feed_in);
void load_archive_data(client_t *client);
guint getEarliest(gchar *path);
guint getLatest(gchar *path);

#endif

