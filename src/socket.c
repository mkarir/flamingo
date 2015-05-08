#include <math.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/un.h>
#include <unistd.h>
#include <setjmp.h>
#include <signal.h>
#include <time.h>
#include <glib.h>
#include <config.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <glade/glade.h>

#include "main.h"
#include "socket.h"
#include "main_interface.h"
#include "info_interface.h"
#include "controls_interface.h"
#include "playback_interface.h"

#define CONNECT_TIMEOUT	3
#define FEED_TIMEOUT 	3

extern viz_t *viz;

static sigjmp_buf jmpbuf;


guint socket_process_data_update(data_record_t *records, guint len, guint timestamp)
{
	guint time_len;
	gchar *status, *time_str;
	time_t time = (time_t) timestamp;

        // destroy all our nodes
        if (viz->nodes != NULL) {
                g_tree_destroy(viz->nodes);
        }
        viz->nodes = g_tree_new_full(gtree_compare, NULL, g_free, NULL);

	// process all the nodes
	viz->handler.input_process_nodes(viz->nodes, records, len);

	// get number of inserted nodes
	guint num_nodes = g_tree_nnodes(viz->nodes);

	time_str = g_strdup(g_strstrip(ctime(&time)));
	time_len = strlen(time_str);
	if (time_str[time_len] == '\n') {
		time_str[time_len-1] = '\0';
	}

	// update secondary status bar
	if (viz->playback_active) {
		status = g_strdup_printf("Playback Update (%s) - %s / %s / %d nodes", time_str, viz->feeds[viz->mode.exporter].name, data_type_str[viz->mode.data_type], num_nodes);
	} else {
		status = g_strdup_printf("Live Update (%s) - %s / %s / %d nodes", time_str, viz->feeds[viz->mode.exporter].name, data_type_str[viz->mode.data_type], num_nodes);
	}
	main_secondary_statusbar_update(status);
	g_free(status);
	g_free(time_str);

	// update our list boxes with the latest nodes
	info_watch_list_update();
	info_full_list_update();

	// schedule redraws for the two gtkgl widgets
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext2d));
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext3d));

	return num_nodes;
}


void socket_process_message(gchar *recv_buf, msg_header_t *header)
{
	guint len = header->size;
	gint numalerts, i;
	wlalert_t *alerts;
	
	switch (header->type) {
		case DATA_UPDATE:
			// allocate mem to store the raw incoming data records
			g_free(viz->paused_records);
			viz->paused_records = g_try_malloc(len);
			viz->paused_len = len;
			
			// if we were unable to allocate enough mem, kill connection with server
			if (viz->paused_records == NULL && len != 0) {
				g_warning("tried to allocate %d bytes of memory and failed, terminating connection", len);
				socket_handle_hup(0, 0, 0);
				return;
			}
			// make a copy of the incoming data for pausing functionality
			memcpy(viz->paused_records, recv_buf, len);
			memcpy(&viz->paused_header, header, sizeof(msg_header_t));

			if (!viz->paused) {
				// if we're not paused, update the tree as usual
				socket_process_data_update((data_record_t *) recv_buf, len, header->extra);
			}
		break;

		case PB_END:
			g_debug("received PB_END message");

			// server is notifying us that the playback has ended
			playback_end_action();
		break;

		case EXPORT_LIST:
			// remove our connection timeout source
			g_source_destroy(g_main_context_find_source_by_id(NULL, viz->timeout_id));

			// keep track of our feed data for display purposes
			g_free(viz->feeds);
			viz->feeds = NULL;
			viz->num_feeds = header->size / sizeof(avail_feed_t);
			viz->feeds = g_try_malloc(viz->num_feeds * sizeof(avail_feed_t));

			// if we were unable to allocate enough mem, kill connection with server
			if (viz->feeds == NULL) {
				g_warning("tried to allocate %d bytes of memory for feeds and failed, terminating connection", (guint) (viz->num_feeds * sizeof(avail_feed_t)));
				socket_handle_hup(0, 0, 0);
				return;
			}

			// copy the feed data
			memcpy(viz->feeds, recv_buf, viz->num_feeds * sizeof(avail_feed_t));

			// update our feed name menu
			main_update_feed_menu();
		break;

		case RESPONSE:
			g_debug("received playback response message");

			// process our playback response
			playback_process_response((pbreq_error_t *) recv_buf, header->size);
		break;

		case WL_ALERT:
			g_debug("received alert from server");
			
			alerts = (wlalert_t *) recv_buf;
			numalerts = len/sizeof(wlalert_t);

			// process incoming alerts			
			for(i=0; i<numalerts; i++)
				info_alert_list_insert(&alerts[i]);
			
		break;

		default:
			// hang up connection if we dont recognize the message type
			g_warning("undefined message type %d", header->type);
			socket_handle_hup(0, 0, 0);
		break;
	}
}


gboolean socket_handle_hup(GIOChannel *channel, GIOCondition cond, gpointer data)
{
	g_debug("socket_handle_hup() called");

	// if already disconnected, dont do anything
	if (!viz->connected) {
		return TRUE;
	}

	// define our hang up message
	msg_header_t hup_msg;
	memset(&hup_msg, 0, sizeof(hup_msg));
	hup_msg.type = g_htonl(TERMINATE);
	hup_msg.size = g_htonl(0);

	// send the hup_msg to the server!  goodbye!
	socket_write((gchar *) &hup_msg, sizeof(hup_msg));

	// remove our watches and unref each watch
	g_source_destroy(g_main_context_find_source_by_id(NULL, viz->in_id));
	g_source_destroy(g_main_context_find_source_by_id(NULL, viz->hup_id));

	// final unref which shutdowns and closes our giochannel socket
	g_io_channel_unref(viz->channel);

	viz->connected = FALSE;
	
	// make the disconnect button into a connect button
	GtkWidget *icon = gtk_image_new_from_icon_name("gtk-connect", GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_widget_show(icon);
	gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(viz->connect_menu_button), icon);
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(viz->connect_menu_button), "Connect");

	// update interface states
	main_primary_statusbar_update("Not Connected");
	main_secondary_statusbar_update("Welcome to Flamingo!");
	gtk_widget_set_sensitive(viz->connect_menu_button, TRUE);
	gtk_widget_set_sensitive(viz->feed_name_menu_button, FALSE);
	gtk_widget_set_sensitive(viz->type_menu_button, FALSE);
	gtk_widget_set_sensitive(viz->update_period_menu_button, FALSE);
	gtk_widget_set_sensitive(viz->aggregation_button, FALSE);
	gtk_widget_set_sensitive(viz->pause_button, FALSE);
	gtk_widget_set_sensitive(viz->resume_button, FALSE);
	gtk_widget_set_sensitive(viz->stop_button, FALSE);

	return TRUE;
}


gboolean socket_handle_in(GIOChannel *channel, GIOCondition cond, gpointer data)
{
	gssize len;
	gchar *recv_buf = NULL;
	msg_header_t header;
	GIOStatus status;

	// make sure the giochannel is readable
	if (!(g_io_channel_get_flags(viz->channel) & G_IO_FLAG_IS_READABLE)) {
		g_warning("channel is not readable");
		return FALSE;
	}

	// first read our message header off the wire
	g_io_channel_read_chars(viz->channel, (gchar *) &header, sizeof(msg_header_t), &len, NULL);

	// convert from net to host byte order
	header.size = g_ntohl(header.size);
	header.type = g_ntohl(header.type);
	header.extra = g_ntohl(header.extra);

	// allocate memory for the data to be received
	recv_buf = g_try_malloc(header.size);

	// if we were unable to allocate enough mem, kill connection with server
	if (recv_buf == NULL && header.size != 0) {
		g_warning("tried to allocate %d bytes of memory and failed, terminating connection", header.size);
		socket_handle_hup(0, 0, 0);
		return FALSE;
	}

	// receive the message payload
	status = g_io_channel_read_chars(viz->channel, recv_buf, header.size, &len, NULL);

	g_debug("read_chars() returned...read %d chars...header->size:%d  header->type:%d", (guint) len, header.size, header.type);

	switch (status) {
		case G_IO_STATUS_NORMAL:
			// handle the received data
			socket_process_message(recv_buf, &header);
			g_free(recv_buf);
			return TRUE;
		break;

		case G_IO_STATUS_ERROR:
			g_warning("socket io error");
			g_free(recv_buf);
			return TRUE;
		break;
		
		case G_IO_STATUS_EOF:
			// manually hang up on EOF status
			socket_handle_hup(0, 0, 0);
			g_free(recv_buf);
			return FALSE;
		break; 

		case G_IO_STATUS_AGAIN:
			g_free(recv_buf);
			return TRUE;
		break; 

		default:
			g_warning("undefined socket error");
			g_free(recv_buf);
			return TRUE;
		break;
	}

	return TRUE;
}


gboolean socket_feed_timeout(gpointer data)
{
	// enable the main interface window since we timed out
	gtk_widget_set_sensitive(viz->main_window, TRUE);

	g_debug("connection timed out");
	socket_handle_hup(0, 0, 0);

	return FALSE;
}


void socket_connect_timeout(gint sig)
{
	siglongjmp(jmpbuf, 1);
}


gboolean socket_connect(const gchar *hostname, guint port)
{
	gint ret;
	gint sock;
	struct sockaddr_in addr;
	struct hostent *hostinfo;
	GError *error = NULL;

	g_debug("attempting connection to %s:%d", hostname, port);

	// create our sexy socket
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		g_warning("error creating socket - %s", strerror(errno));
		return FALSE;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	hostinfo = gethostbyname(hostname);
	if (hostinfo == NULL) {
		g_warning("unknown host %s", hostname);
		close(sock);
		return FALSE;
	}

	addr.sin_addr = *(struct in_addr *) hostinfo->h_addr;

	// set our alarm signal for connect() timeout
	signal(SIGALRM, socket_connect_timeout);

	if (!sigsetjmp(jmpbuf, 1)) {
		// set our connect() timeout to CONNECT_TIMEOUT seconds
		alarm(CONNECT_TIMEOUT);

		// connect!
		ret = connect(sock, (struct sockaddr *) &addr, sizeof(addr));

		// reset the alarm
		alarm(0);
	} else {
		// handle the timeout
		g_warning("connect() timed out");
		close(sock);
		return FALSE;
	}

	// make sure connect() did not return an error
	if (ret < 0) {
		g_warning("error connecting to %s:%d - %s", hostname, port, strerror(errno));
		close(sock);
		return FALSE;
	}

	// create a new giochannel from our connected socket
	viz->channel = g_io_channel_unix_new(sock);

	// set binary encoding
	g_io_channel_set_encoding(viz->channel, NULL, &error);
	if (error) {
		g_error_free(error);
	}

	// auto-close our socket when its unreferenced to 0
	g_io_channel_set_close_on_unref(viz->channel, TRUE);

	// add watches for when data is available and on hangup
	viz->in_id = g_io_add_watch(viz->channel, G_IO_HUP, socket_handle_hup, NULL);
	viz->hup_id = g_io_add_watch(viz->channel, G_IO_IN, socket_handle_in, NULL);

	// assign our current server addr and port
	viz->server_addr = *(struct in_addr *) hostinfo->h_addr;
	viz->server_port = port;

	// timeout in FEED_TIMEOUT seconds if a feed list is not received
	viz->timeout_id = g_timeout_add(FEED_TIMEOUT * 1000, socket_feed_timeout, NULL);

	return TRUE;
}


gboolean socket_write(gchar *buf, guint len)
{
	g_debug("socket_write() called %p %d", buf, len);

	// if already disconnected, dont do anything
	if (!viz->connected) {
		return TRUE;
	}

	// make sure we're alive
	if (!viz->channel) {
		g_warning("trying to send line '%s' to socket that is not connected!", buf);
		return FALSE;
	}

	// make sure giochannel is currently writable
	if (!(g_io_channel_get_flags(viz->channel) & G_IO_FLAG_IS_WRITEABLE)) {
		g_warning("giochannel is not writeable");
		return FALSE;
	}

	// write that dandy data
	if (g_io_channel_write_chars(viz->channel, buf, len, NULL, NULL) == G_IO_STATUS_ERROR) { 
		g_warning("cannot send, g_io_channel_write_chars failed");
		return FALSE;
	}

	// flush the channel
	if (g_io_channel_flush(viz->channel, NULL) == G_IO_STATUS_ERROR) {
		g_warning("cannot send, g_io_channel_write_chars failed");
		return FALSE;
	}

	return TRUE;
}
