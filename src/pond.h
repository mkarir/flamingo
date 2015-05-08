#ifndef _POND_H_
#define _POND_H_

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <glob.h>
#include <time.h>
#include <glib.h>
#include <config.h>
#include <pthread.h>
#include <ftlib.h>
#include <math.h>

#include "protocol.h"

#define PRIMARY_PATH "../data/"
#define BACKUP_PATH FLAMINGO_DIR "/"

typedef struct {
	gboolean completed;
	gboolean paused;
	gchar *fname;
	gint current;
	gint start;
	gint end;
	gint rate;
	gint scaler;
	guint file_index;
	struct tm *ts;
	FILE *file;
	struct ftio handle;
} pbtracker_t;

typedef struct {
	guint time;
	gboolean live;
	guint timeout_id;
	GStaticRWLock rwlock;

	// data elements
	GTree *node_tree;

	// current mode parameters 
	fmode_t mode;
	frange_t *range_src;
	frange_t *range_dst;

	// tracking structure for playback
	pbtracker_t *tracker;
	
	// watch list for anomaly detection
	GData *watch_list;

	// glib related items
	GMainLoop *main_loop;
	GMainContext *context;
	GIOChannel *io;
	GSource *io_in_watch;
	GSource *io_hup_watch;
	GSource *update_timeout;
} client_t;

typedef struct {
	// parameters configured via server.conf
	gchar *password;
	gchar *archive_path;
	guint max_clients;
	guint listen_port;

	// server state vars
	gint server_sock;
	GPtrArray *clients;		// client_t
	GStaticRWLock clients_rwlock;

	// netflow feed structure
	guint num_feeds;
	GPtrArray *feeds;		// nf_feed_t

	// master data/color structures
	GTree *master_ip;			// ip_master_t color/longest pfx match
	GPtrArray *pallet;			// colors for ASes & non-aggregated feeds
} pond_t;

#endif
