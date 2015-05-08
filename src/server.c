#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <glob.h>
#include <time.h>
#include <glib.h>
#include <config.h>

#include "pond.h"
#include "myflow.h"
#include "gtree.h"
#include "server.h"
#include "protocol.h"
#include "anomaly_detection.h"

extern pond_t *pond;


gboolean init_server()
{
	gint sockopt_on = 1;
	gint sockin, ssize = sizeof(struct sockaddr_in);
	struct sockaddr_in new_client;
	struct sockaddr_in server_addr;

  	if ((pond->server_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    		g_error("server socket() failed");
    		return FALSE;
	}

	// make socket reusable
	if (setsockopt(pond->server_sock, SOL_SOCKET, SO_REUSEADDR, &sockopt_on, sizeof(gint)) == -1) {
		g_error("server socket setsockopt() failed");
		return FALSE;
	}

	memset((gchar *) &server_addr, 0, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = g_htons(pond->listen_port);

	if (bind(pond->server_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1){ 
		g_error("server socket bind() failed");
		return FALSE;
	}

	if (listen(pond->server_sock, BACKLOG) == -1) {
		g_error("server socket listen() failed");
		return FALSE;
	}

	g_debug("server listening on %s:%d", inet_ntoa(server_addr.sin_addr), g_ntohs(server_addr.sin_port));


	// loop forever to accept connections from new clients
	while (TRUE) {
		if ((sockin = accept(pond->server_sock, (struct sockaddr *) &new_client, &ssize)) == -1) {
			g_error("server accept() failed");
	    	}

		// read lock for pond->clients
		g_static_rw_lock_reader_lock(&pond->clients_rwlock);

		// make sure that we're not exceeding our max number of clients
		if (pond->clients->len > pond->max_clients) {
			g_warning("max clients exceeded, rejecting connection");
			g_static_rw_lock_reader_unlock(&pond->clients_rwlock);
			close(sockin);
			continue;
		}

		// read unlock for pond->clients
		g_static_rw_lock_reader_unlock(&pond->clients_rwlock);

 		g_debug("request received from %s (port %d)", inet_ntoa(new_client.sin_addr), g_ntohs(new_client.sin_port));

		// spawn our client thread 
		if (!g_thread_create(client_thread, GINT_TO_POINTER(sockin), FALSE, NULL)) {
			g_warning("client thread creation error");
			close(sockin);
			continue;
		}
	}
}


gboolean client_socket_write(client_t *client, gchar *buf, guint len)
{
	g_debug("client_socket_write() called!");

	// make sure we're alive
	if (!client->io) {
		g_warning("trying to send data to a socket that is not connected!");
		return FALSE;
	}

	// make sure giochannel is currently writable
	if (!(g_io_channel_get_flags(client->io) & G_IO_FLAG_IS_WRITEABLE)) {
		g_warning("giochannel is not writeable");
		return FALSE;
	}

	// write that dandy data
	if (g_io_channel_write_chars(client->io, buf, len, NULL, NULL) == G_IO_STATUS_ERROR) {
		g_warning("cannot send, g_io_channel_write_chars failed");
		return FALSE;
	}

	// flush the channel
	if (g_io_channel_flush(client->io, NULL) == G_IO_STATUS_ERROR) {
		g_warning("cannot send, g_io_channel_flush failed");
		return FALSE;
	}

	return TRUE;
}


client_t *client_allocate(gint socket)
{
	client_t *client;
	nf_feed_t *feed;
	
	feed = g_ptr_array_index(pond->feeds, 0);
			
	// allocate a new client
	client = g_new0(client_t, 1);
	client->live = TRUE;

	// set default initial mode for the client
	client->mode.exporter = 0;
	client->mode.aggregated = feed->aggregation;
	client->mode.data_type = SRC_IP;
	client->mode.update_period = DEFAULT_UPDATE_INTERVAL;
	client->mode.ranges_src = 0;
	client->mode.ranges_dst = 0;
	client->range_src = NULL;
	client->range_dst = NULL;
	client->tracker = NULL;
	
	// initialize the list of filters
	g_datalist_init(&client->watch_list);
	
	// initialize our GIOChannel from our connected socket
	client->io = g_io_channel_unix_new(socket);

	// set binary encoding on our GIOChannel
	g_io_channel_set_encoding(client->io, NULL, NULL);

	// auto-close our socket when its unreferenced to 0
	g_io_channel_set_close_on_unref(client->io, TRUE);


	// initialize/populate all the client trees
	g_static_rw_lock_init(&client->rwlock);
	g_static_rw_lock_writer_lock(&client->rwlock);

	client->node_tree = g_tree_new_full(g_tree_compare_ip, NULL, g_free, NULL);

	g_static_rw_lock_writer_unlock(&client->rwlock);

	return client;
}


void client_deallocate(client_t *client)
{
	g_static_rw_lock_writer_lock(&client->rwlock);

	// destroy our data trees
	if (client->node_tree) {
		g_tree_destroy(client->node_tree);
	}
	
	//free the lists of ranges
	g_free(client->range_src);
	g_free(client->range_dst);
	
	//clean up the watch_list
	g_datalist_clear(&client->watch_list);

	g_static_rw_lock_writer_unlock(&client->rwlock);
	g_static_rw_lock_free(&client->rwlock);

	g_free(client);
}


void client_unregister(client_t *client)
{
	// lock clients rwlock
	g_static_rw_lock_writer_lock(&pond->clients_rwlock);

	if (!g_ptr_array_remove_fast(pond->clients, client)) {
		g_warning("error removing client from queue");
	}

	g_debug("client unregistered!!!");
	g_debug("currently %d client(s) registered", pond->clients->len);

	// unlock clients rwlock
	g_static_rw_lock_writer_unlock(&pond->clients_rwlock);
}


void client_register(client_t *client)
{
	// lock clients rwlock
	g_static_rw_lock_writer_lock(&pond->clients_rwlock);

	// add client to client list
	g_ptr_array_add(pond->clients, client);

	g_debug("client registered!!!");
	g_debug("currently %d client(s) registered", pond->clients->len);

	// unlock clients rwlock
	g_static_rw_lock_writer_unlock(&pond->clients_rwlock);
}


gboolean client_update_timeout(gpointer client_in)
{
	client_t *client = (client_t *) client_in;
	msg_header_t msg;

	g_debug("client_update_timeout() called!");

	//ignore timeout if the client is paused (must be in playback mode)
	if(!client->live && client->tracker->paused)
		return TRUE;
		
	//if in playback mode put archived data into client data tree
	g_static_rw_lock_writer_lock(&client->rwlock);
	if(!client->live)
		load_archive_data(client);
	g_static_rw_lock_writer_unlock(&client->rwlock);
	
	// send the update to the client
	send_data_update(client);

	// refresh the client's data stats
	client_refresh(client);
	
	// send any pending alerts to client
	send_pending_alerts(client);
	
	g_static_rw_lock_writer_lock(&client->rwlock);
	
	//check if server-side has finished playback
	if(!client->live && client->tracker->completed){
		g_free(client->tracker->fname);
		g_free(client->tracker);
		client->tracker = NULL;
		
		g_debug("resuming live updates and sending PB_END!");
		client->live = TRUE;

		//send PB_END message to client
		msg.type = g_htonl(PB_END);
		msg.size = g_htonl(0);
		client_socket_write(client, (gchar*)&msg, sizeof(msg_header_t));
	}
	
	g_static_rw_lock_writer_unlock(&client->rwlock);

	// returning true keeps the timeout going
	return TRUE;
}


gboolean client_io_hup(GIOChannel *source, GIOCondition condition, gpointer client_in)
{
	client_t *client = (client_t *) client_in;

	g_debug("client_io_hup() called!");

	// quit our main loop in our client thread on hangup event
	g_main_loop_quit(client->main_loop);

	return TRUE;
}


gboolean client_connect_timeout(gpointer data)
{
	g_debug("client connection timed out");

	client_io_hup(0, 0, 0);

	return FALSE;
}


gboolean client_io_in(GIOChannel *source, GIOCondition condition, gpointer client_in)
{
	client_t *client = (client_t *) client_in;

	g_debug("client_io_in() called!");

	gssize len;
	gchar *recv_buf = NULL;
	msg_header_t header;
	GIOStatus status;

	// make sure the giochannel is readable
	if (!(g_io_channel_get_flags(client->io) & G_IO_FLAG_IS_READABLE)) {
		g_warning("channel is not readable");
		return FALSE;
	}

	// first read our message header off the wire
	g_io_channel_read_chars(client->io, (gchar *) &header, sizeof(msg_header_t), &len, NULL);

	// convert from net to host byte order
	header.size = g_ntohl(header.size);
	header.type = g_ntohl(header.type);
	header.extra = g_ntohl(header.extra);

	// allocate memory for the data to be received
	recv_buf = g_try_malloc(header.size);

	// if we were unable to allocate enough mem, kill connection with client
	if (recv_buf == NULL && header.size != 0) {
		g_warning("tried to allocate %d bytes of memory and failed, terminating connection", header.size);
		client_io_hup(0, 0, client);
		return FALSE;
	}

	// receive the message payload
	status = g_io_channel_read_chars(client->io, recv_buf, header.size, &len, NULL);

	g_debug("read_chars() returned...read %d chars...header->size:%d  header->type:%d", (guint) len, header.size, header.type);

	switch (status) {
		case G_IO_STATUS_NORMAL:
			// handle the received data
			handle_message(client, recv_buf, &header);
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
			g_free(recv_buf);
			client_io_hup(0, 0, client);
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


gpointer client_thread(gpointer socket)
{
	// allocate a new client and initialize all its structures
	client_t *client = client_allocate(GPOINTER_TO_INT(socket));

	// register our client in the servers queue
	client_register(client);

	// create a new GMainContext/GMainLoop for our sources/timeouts within this thread
	client->context = g_main_context_new();
	client->main_loop = g_main_loop_new(client->context, FALSE);

	// add our timeout interval to send updates to the client, and attach it to our current context
	client->update_timeout = g_timeout_source_new(client->mode.update_period * 1000);
	g_source_set_callback(client->update_timeout, (GSourceFunc) client_update_timeout, client, NULL);
	g_source_attach(client->update_timeout, client->context);
	g_source_unref(client->update_timeout);

	// add our GIOChannel watch for when data is ready to be read
	client->io_in_watch = g_io_create_watch(client->io, G_IO_IN);
	g_source_set_callback(client->io_in_watch, (GSourceFunc) client_io_in, client, NULL);
	g_source_attach(client->io_in_watch, client->context);
	g_source_unref(client->io_in_watch);

	// add our GIOChannel watch for when the connection is hung up
	client->io_hup_watch = g_io_create_watch(client->io, G_IO_HUP);
	g_source_set_callback(client->io_hup_watch, (GSourceFunc) client_io_hup, client, NULL);
	g_source_attach(client->io_hup_watch, client->context);
	g_source_unref(client->io_hup_watch);

	// if the client doesn't send a proper connect message within the timeout, disconnect
	client->timeout_id = g_timeout_add(CONNECT_TIMEOUT * 1000, client_connect_timeout, NULL);

	// run our main loop and process events
	g_debug("main loop started!");
	g_main_loop_run(client->main_loop);
	g_debug("main loop exited!  cleaning up client structures");


	// remove our GIOChannel watches which also unref the channel each time
	g_source_destroy(client->io_hup_watch);
	g_source_destroy(client->io_in_watch);

	// remove our timeout interval
	g_source_destroy(client->update_timeout);

	// unref and close our giochannel socket
	g_io_channel_unref(client->io);

	// main loop has exited, unref main loop and context structures
	g_main_loop_unref(client->main_loop);
	g_main_context_unref(client->context);


	// unregister from the client queue
	client_unregister(client);

	// deallocate all the memory associated with our client structures
	client_deallocate(client);

	// exit our client thread, goodbye!
	return (NULL);
}


void client_refresh(client_t *client)
{
	g_static_rw_lock_writer_lock(&client->rwlock);

	if (client->node_tree) {
		g_tree_destroy(client->node_tree);
	}

	switch (client->mode.data_type) {
		case SRC_IP:
		case DST_IP:
			client->node_tree = g_tree_new_full(g_tree_compare_ip, NULL, g_free, NULL);
		break;

		case SRC_AS:
		case DST_AS:
			client->node_tree = g_tree_new_full(g_tree_compare_as, NULL, g_free, NULL);
		break;

		case SRC_PORT:
		case DST_PORT:
			client->node_tree = g_tree_new_full(g_tree_compare_port, NULL, g_free, NULL);
		break;

		case SRC_DST_PORT:
			client->node_tree = g_tree_new_full(g_tree_compare_src_dst_port, NULL, g_free, NULL);
		break;

		case SRC_DST_IP:
			client->node_tree = g_tree_new_full(g_tree_compare_src_dst_ip, NULL, g_free, NULL);
		break;

		case SRC_DST_AS:
			client->node_tree = g_tree_new_full(g_tree_compare_src_dst_as, NULL, g_free, NULL);
		break;

		default:
			g_error("unknown data type");
		break;
	}

	g_static_rw_lock_writer_unlock(&client->rwlock);
}


void send_feed_list(client_t *client)
{
	guint i;
	nf_feed_t *feed;
	gchar *feed_list;
	msg_header_t header;
	avail_feed_t feed_entry;
	guint size = pond->num_feeds * sizeof(avail_feed_t);
	guint total_size = size + sizeof(msg_header_t);

	// allocate buffer for all of the feed names
	feed_list = g_new0(gchar, total_size);

	header.type = g_htonl(EXPORT_LIST);
	header.size = g_htonl(size);

	memcpy(feed_list, &header, sizeof(msg_header_t));

	// copy all the feed names into the list
	for (i = 0; i < pond->num_feeds; ++i) {
		feed = g_ptr_array_index(pond->feeds, i);
		
		// create feed_entry
		memcpy(feed_entry.name, feed->name, EXPORTER_NAME_LEN);
		feed_entry.aggregation = g_htonl(feed->aggregation);
		feed_entry.as_availability = g_htonl(feed->as_availability);

		// copy into outgoing list of avail_feed_t
		memcpy(feed_list + sizeof(msg_header_t) + sizeof(avail_feed_t) * i, &feed_entry, sizeof(avail_feed_t));
	}

	g_debug("sending feed list to client, feeds: %d  size: %d", pond->num_feeds, total_size);

	// send the list to the client
	if (!client_socket_write(client, feed_list, total_size)) {
		g_warning("error sending feed list to client");
		g_free(feed_list);
		client_io_hup(0, 0, 0);
		return;
	}

	g_debug("feed list sent!");

	g_free(feed_list);
}


void playback_response_send(client_t *client, gint error, gint extra)
{
	gchar *response;
	pbreq_error_t pberr;
	msg_header_t header;

	response = g_new0(gchar, sizeof(msg_header_t) + sizeof(pbreq_error_t));

	header.type = g_htonl(RESPONSE);
	header.size = g_htonl(sizeof(pbreq_error_t));

	pberr.errorcode = g_htonl(error);
	pberr.extra = g_htonl(extra);

	memcpy(response, &header, sizeof(msg_header_t));
	memcpy(response + sizeof(msg_header_t), &pberr, sizeof(pbreq_error_t));

	client_socket_write(client, response, sizeof(msg_header_t) + sizeof(pbreq_error_t));

	g_free(response);
}


gboolean check_if_feed_avail(pbreq_t *pbreq)
{
	nf_feed_t *feed;
	gchar *path;
	glob_t fnames;
	
	feed = g_ptr_array_index(pond->feeds, pbreq->exporter);
	
	if(!feed)
		return FALSE;
	
	//compose path
	path = g_build_filename(pond->archive_path, feed->name, NULL);
	
	if(!path)
		return FALSE;
			
	glob(path, 0, NULL, &fnames);

	g_free(path);

	return fnames.gl_pathc;
}


void handle_message(client_t *client, gchar *recv_buf, msg_header_t *header)
{
	fmode_t *mode;
	pbreq_t *pbreq;
	nf_feed_t *feed;
	gchar *path;
	gchar *client_pw;
	msg_header_t msg;
	frange_t *ranges;
	wlreq_t * wlreq;
	gint numfilters, i;
	struct tm *ts;
	time_t ti;
	
	switch (header->type) {
		// update the exporter, data type, and update period
		case MODE_UPDATE:
			// translate our recv_buf into a fmode_t
			mode = (fmode_t *) recv_buf;
			mode->exporter = g_ntohl(mode->exporter);
			mode->data_type = g_ntohl(mode->data_type);
			mode->update_period = g_ntohl(mode->update_period);
			mode->ranges_src = g_ntohl(mode->ranges_src);
			mode->ranges_dst = g_ntohl(mode->ranges_dst);
			
			g_debug("feed=%d  type=%d  period=%d  aggregated=%d", mode->exporter, mode->data_type, mode->update_period, mode->aggregated);
			
			feed = g_ptr_array_index(pond->feeds, mode->exporter);

			if((mode->exporter >= 0) && (mode->exporter < pond->num_feeds) && (mode->data_type >= 0) && (mode->data_type <= DATA_TYPE_MAX) && (mode->update_period > 0) && !(mode->aggregated && !feed->aggregation)) {
				// refresh the client's data stats before updating the mode so that we refresh the right structures
				client_refresh(client);

				// lock client structure as we'll be modifying members
				g_static_rw_lock_writer_lock(&client->rwlock);

				if (client->live) {      //Don't allow changing of feed during playback
					client->mode.exporter = mode->exporter;
				} else {
					g_debug("exporter not changed, currently in playback mode");
				}

				client->mode.data_type = mode->data_type;
				client->mode.update_period = mode->update_period;
				client->mode.aggregated = mode->aggregated;
				
				//erase old src ranges
				g_free(client->range_src);
				
				//set new src ranges
				client->range_src = g_new0(frange_t, mode->ranges_src);
				client->mode.ranges_src = mode->ranges_src;
				ranges = (frange_t*)(recv_buf+sizeof(fmode_t));

				//copy new src ranges into client_t struct
				for(i=0; i < mode->ranges_src; i++){
					g_debug("received-range_src: %u/%u", g_ntohl(ranges[i].addr), g_ntohl(ranges[i].network_bits));
					client->range_src[i].addr = g_ntohl(ranges[i].addr);
					client->range_src[i].network_bits = g_ntohl(ranges[i].network_bits);
				}

				//erase old dst ranges
				g_free(client->range_dst);
				
				//set new dst ranges
				client->range_dst = g_new0(frange_t, mode->ranges_dst);
				client->mode.ranges_dst = mode->ranges_dst;
				ranges = (frange_t*)(recv_buf+sizeof(fmode_t)+sizeof(frange_t)*mode->ranges_src);

				//copy new dst ranges into client_t struct
				for(i=0; i < mode->ranges_dst; i++){
					g_debug("received-range_dst: %u/%u", g_ntohl(ranges[i].addr), g_ntohl(ranges[i].network_bits));
					client->range_dst[i].addr = g_ntohl(ranges[i].addr);
					client->range_dst[i].network_bits = g_ntohl(ranges[i].network_bits);
				}
				
				g_debug("client mode updated");

				// destroy then re-add our timeout based on the new update period
				g_source_destroy(client->update_timeout);
				client->update_timeout = g_timeout_source_new(client->mode.update_period * 1000);
				g_source_set_callback(client->update_timeout, (GSourceFunc) client_update_timeout, client, NULL);
				g_source_attach(client->update_timeout, client->context);
				g_source_unref(client->update_timeout);

				// unlock client structure
				g_static_rw_lock_writer_unlock(&client->rwlock);
			} else {
				g_warning("client mode not updated, invalid parameters");
			}
       	 	break;

		// control the playback of archived data
		case PB_REQUEST:	
			if (!client->live) {
				// send negative response for pb already running
				g_debug("already in playback mode");
				playback_response_send(client, RUNNING, 0);
				return;
			}

			pbreq = (pbreq_t*)recv_buf;
			pbreq->exporter = g_ntohl(pbreq->exporter);
			pbreq->start_time = g_ntohl(pbreq->start_time) - 3600;
			pbreq->end_time = g_ntohl(pbreq->end_time) - 3600;
			pbreq->rate = g_ntohl(pbreq->rate);

			
			ti = pbreq->start_time;
			ts = localtime(&ti);
			if(ts->tm_year+1900 == 2004)
				pbreq->start_time += 3600;
			
			ti = pbreq->end_time;
			ts = localtime(&ti);
			if(ts->tm_year+1900 == 2004)
				pbreq->end_time += 3600;
			
			
			g_debug("PB_REQUEST: Feed=%d Start=(%d) End=(%d) Rate=%d", pbreq->exporter, pbreq->start_time, pbreq->end_time, pbreq->rate);

			// make sure there are archives available for the requested feed
			if( !check_if_feed_avail(pbreq) ){
				//send negative response for feednum
				g_debug("Not a Valid PlayBack Feed");
				playback_response_send(client, NO_EXP, 0);
				return;
			}

			// retrieve our feed
			feed = g_ptr_array_index(pond->feeds, pbreq->exporter);

			//compose path
			path = g_build_filename(pond->archive_path, feed->name, NULL);
			
			g_debug("Earliest: %d", getEarliest(path));
			
			if(pbreq->start_time <= getEarliest(path)){
				//send negative response for start time
				g_debug("Specified Time Too Early");

				playback_response_send(client, TOO_EARLY, getEarliest(path));
				g_free(path);
				return; 
			}
			
			if(pbreq->end_time >= getLatest(path)){
				//send negative response for end time
				g_debug("Specified Time Too Late");

				playback_response_send(client, TOO_LATE, getLatest(path));
				g_free(path);
				return; 
			}
			
			g_free(path);

			if (pbreq->rate <= 0 ) {
				g_debug("Rate Below Zero, Adjusting to 1.");
				pbreq->rate = 1;
			}
			
			//clear out live data before beginning playback
			client_refresh(client);
			
			//switch to playback mode
			g_static_rw_lock_writer_lock(&client->rwlock);
			
			//set exporter - static throughout playback (atleast for now)
			client->mode.exporter = pbreq->exporter;
			
			//initialize playback tracking structure
			client->tracker = g_new0(pbtracker_t, 1);
			client->tracker->fname = g_malloc0( strlen(feed->name) );
			strncpy(client->tracker->fname, feed->name, strlen(feed->name));
			client->tracker->current = pbreq->start_time;
			client->tracker->start = pbreq->start_time;
			client->tracker->end = pbreq->end_time;
			client->tracker->rate = pbreq->rate;
			client->tracker->scaler = feed->scalefactor;
			
			g_debug("beginning playback");
			client->live = FALSE;
			client->time = client->tracker->start;
			
			g_static_rw_lock_writer_unlock(&client->rwlock);

			//send positive response
			msg.type = g_htonl(RESPONSE);
			msg.size = g_htonl(0);
			client_socket_write(client, (gchar *) &msg, sizeof(msg));
			
			//trigger the first playback update
			client_update_timeout(client);
		break;
			
		// playback play request
		case PB_PLAY:
			g_static_rw_lock_writer_lock(&client->rwlock);
			client->tracker->paused = FALSE;
			g_static_rw_lock_writer_unlock(&client->rwlock);
		break;

		// playback pause request	
		case PB_PAUSE:
			g_static_rw_lock_writer_lock(&client->rwlock);
			client->tracker->paused = TRUE;
			g_static_rw_lock_writer_unlock(&client->rwlock);
		break;

		// playback stop request	
		case PB_STOP:
			//switch to live mode
			g_static_rw_lock_writer_lock(&client->rwlock);
			
			if(client->tracker->file != NULL){
				g_debug("Closing File\n");
				fclose(client->tracker->file);
			}
			
			g_free(client->tracker->fname);
			g_free(client->tracker);
			client->tracker = NULL;
			
			g_debug("resuming live updates!");
			client->live = TRUE;
			
			g_static_rw_lock_writer_unlock(&client->rwlock);
		break;

		// add a new filter to watch list
		case WL_UPDATE:
			if(header->size%sizeof(wlreq_t) != 0)
				break;

			// remove old filters
			g_datalist_clear(&client->watch_list);
			
			wlreq = (wlreq_t*)recv_buf;
			numfilters = header->size/sizeof(wlreq_t);

			// add new filters
			for(i=0; i < numfilters; i++){
				wlreq[i].filter_id = g_ntohl(wlreq[i].filter_id);
				wlreq[i].trigger_type = g_ntohl(wlreq[i].trigger_type);
				//wlreq[i].trigger_val = g_ntohl(wlreq[i].trigger_val);
				watch_list_add(client, &wlreq[i]);
			}

		break;

		case CONNECT:
			client_pw = g_try_malloc(header->size + 1);

			// if we were unable to allocate enough mem, kill connection with client
			if (client_pw == NULL) {
				g_warning("tried to allocate %d bytes of memory and failed, terminating connection", header->size + 1);
				client_io_hup(0, 0, client);
				return;
			}

			memset(client_pw, 0, header->size + 1);
			memcpy(client_pw, recv_buf, header->size);

			// remove our connection timeout source
			g_source_destroy(g_main_context_find_source_by_id(NULL, client->timeout_id));

			// make sure our magic number matches
			if (header->extra != CONNECT_MAGIC) {
				g_free(client_pw);
				g_debug("client rejected because of invalid magic number");
				client_io_hup(0, 0, client);
				return;
			}

			// check our server password with the client's if a server password exists
			if (pond->password && strcmp(pond->password, client_pw) != 0) {
				g_free(client_pw);
				g_debug("client rejected because of invalid password");
				client_io_hup(0, 0, client);
				return;
			}

			g_debug("client connection accepted");

			// send the feed list to the client
			send_feed_list(client);

			g_free(client_pw);
		break;

		// close the connection
		case TERMINATE:
			g_debug("client requested connection termination");
			client_io_hup(0, 0, client);
		break;

		// unrecognized message type
		default:
			g_warning("unknown message type %d size=%d", header->type, header->size);
			client_io_hup(0, 0, client);
		break;
	}

}


void assign_colors(data_record_t *curr_record, ip_node_t *node1, ip_node_t *node2, client_t *client){
	nf_feed_t *feed;
	ip_master_t *color;
	ip_master_t color_lookup;
	gpointer color_tmp = NULL;
	
	guint color_index;
	color_t *pallet_color;
	
	feed = g_ptr_array_index(pond->feeds, client->mode.exporter);
	
	
	if(client->mode.aggregated){
	
		if(node1){
			// look up the color node
			color_lookup.addr = node1->addr;
			color_lookup.network_bits = node1->network_bits;
			if (!g_tree_lookup_extended(pond->master_ip, &color_lookup, &color_tmp, NULL)) {
				g_warning("color not found in tree!  uhoh!");
			}

			// assign color/origin_as values
			color = (ip_master_t *) color_tmp;
			curr_record->originas = g_htonl(color->origin_as);
			curr_record->red = color->red;
			curr_record->green = color->green;
			curr_record->blue = color->blue;
		}
		
		if(node2){
			// look up the color node
			color_lookup.addr = node2->addr;
			color_lookup.network_bits = node2->network_bits;
			if (!g_tree_lookup_extended(pond->master_ip, &color_lookup, &color_tmp, NULL)) {
				g_warning("color not found in tree!  uhoh!");
			}

			// assign color/origin_as values
			color = (ip_master_t *) color_tmp;
			curr_record->dstoriginas = g_htonl(color->origin_as);
			curr_record->red2 = color->red;
			curr_record->green2 = color->green;
			curr_record->blue2 = color->blue;
		}
		
	}else{
		
		if(node1){
			// get an index into the pallet
			color_index = node1->addr & COLOR_SIZE;
			pallet_color = g_ptr_array_index(pond->pallet, color_index);
			
			if(!pallet_color){
				g_warning("color pallet broken (FIX ME): index=%d", color_index);
			}else{
				// assign color/dummy_origin_as values
				curr_record->originas = g_htonl(0);
				curr_record->red = pallet_color->red;
				curr_record->green = pallet_color->green;
				curr_record->blue = pallet_color->blue;
			}
		}
		
		if(node2){
			// get an index into the pallet
			color_index = node2->addr & COLOR_SIZE;
			pallet_color = g_ptr_array_index(pond->pallet, color_index);
			
			if(!pallet_color){
				g_warning("color pallet broken (FIX ME): index=%d", color_index);
			}else{
				// assign color/dummy_origin_as values
				curr_record->dstoriginas = g_htonl(0);
				curr_record->red2 = pallet_color->red;
				curr_record->green2 = pallet_color->green;
				curr_record->blue2 = pallet_color->blue;
			}
		}
		
	}

}


gboolean g_tree_foreach_process_ip(gpointer node_in, gpointer value, gpointer process_data_in)
{
	ip_node_t *node = (ip_node_t *) node_in;
	process_data_t *process_data = (process_data_t *) process_data_in;
	data_record_t *curr_record = process_data->curr_record;
	
	
	// assign node values
	curr_record->address = g_htonl(node->addr);
	curr_record->length = g_htonl(node->network_bits);
	curr_record->count = g_htonl(node->count);

	assign_colors(curr_record, node, NULL, process_data->client);

	process_data->curr_record++;
	process_data->curr_record_id++;

	return FALSE;
}


gboolean g_tree_foreach_process_as(gpointer node_in, gpointer value, gpointer process_data_in)
{
	color_t *color;
	as_node_t *node = (as_node_t *) node_in;
	process_data_t *process_data = (process_data_t *) process_data_in;
	data_record_t *curr_record = process_data->curr_record;

	// assign node values
	curr_record->address = g_htonl(node->as_num);
	curr_record->count = g_htonl(node->count);

	// look up the as color node
	if (node->as_num >=0 && node->as_num < 65536) {
		color = g_ptr_array_index(pond->pallet, node->as_num);
	} else {
		g_warning("as color out of range!  uhoh!");
		color = g_ptr_array_index(pond->pallet, 0);
	}

	// assign color/origin_as values
	curr_record->red = color->red;
	curr_record->green = color->green;
	curr_record->blue = color->blue;

	process_data->curr_record++;
	process_data->curr_record_id++;

	return FALSE;
}


gboolean g_tree_foreach_process_port(gpointer node_in, gpointer value, gpointer process_data_in)
{
	port_node_t *node = (port_node_t *) node_in;
	process_data_t *process_data = (process_data_t *) process_data_in;
	data_record_t *curr_record = process_data->curr_record;
	ip_node_t color_node;
	
	curr_record->address = g_htonl(node->ip);
	curr_record->length = g_htonl(node->len);
	curr_record->port = g_htonl(node->port);
	curr_record->count = g_htonl(node->count);

	// create a tmp node to lookup the colors
	color_node.addr = node->ip;
	color_node.network_bits = node->len;
	
	assign_colors(curr_record, &color_node, NULL, process_data->client);

	process_data->curr_record++;
	process_data->curr_record_id++;

	return FALSE;
}


gboolean g_tree_foreach_process_src_dst_ip(gpointer node_in, gpointer value, gpointer process_data_in)
{
	src_dst_ip_node_t *node = (src_dst_ip_node_t *) node_in;
	process_data_t *process_data = (process_data_t *) process_data_in;
	data_record_t *curr_record = process_data->curr_record;
	ip_node_t color_node1, color_node2;
	
	curr_record->address = g_htonl(node->srcip);
	curr_record->length = g_htonl(node->srclen);
	curr_record->dstaddr = g_htonl(node->dstip);
	curr_record->dstlen = g_htonl(node->dstlen);
	curr_record->count = g_htonl(node->count);

	// create a tmp nodes to lookup the colors
	color_node1.addr = node->srcip;
	color_node1.network_bits = node->srclen;
	color_node2.addr = node->dstip;
	color_node2.network_bits = node->dstlen;
	
	assign_colors(curr_record, &color_node1, &color_node2, process_data->client);
	
	process_data->curr_record++;
	process_data->curr_record_id++;

	return FALSE;
}


gboolean g_tree_foreach_process_src_dst_as(gpointer node_in, gpointer value, gpointer process_data_in)
{
	color_t *color;
	src_dst_as_node_t *node = (src_dst_as_node_t *) node_in;
	process_data_t *process_data = (process_data_t *) process_data_in;
	data_record_t *curr_record = process_data->curr_record;

	curr_record->address = g_htonl(node->srcas);
	curr_record->dstaddr = g_htonl(node->dstas);
	curr_record->count = g_htonl(node->count);

	// look up the src as color node
	if (node->srcas >=0 && node->srcas < 65536) {
		color = g_ptr_array_index(pond->pallet, node->srcas);
	} else {
		g_warning("as color out of range!  uhoh!");
		color = g_ptr_array_index(pond->pallet, 0);
	}

	// assign source color values
	curr_record->red = color->red;
	curr_record->green = color->green;
	curr_record->blue = color->blue;

	// look up the dst color
	if (node->dstas >=0 && node->dstas < 65536) {
		color = g_ptr_array_index(pond->pallet, node->dstas);
	} else {
		g_warning("as color out of range!  uhoh!");
		color = g_ptr_array_index(pond->pallet, 0);
	}

	// assign dst color values
	curr_record->red2 = color->red;
	curr_record->green2 = color->green;
	curr_record->blue2 = color->blue;

	process_data->curr_record++;
	process_data->curr_record_id++;

	return FALSE;
}


gboolean g_tree_foreach_process_src_dst_port(gpointer node_in, gpointer value, gpointer process_data_in)
{
	src_dst_port_node_t *node = (src_dst_port_node_t *) node_in;
	process_data_t *process_data = (process_data_t *) process_data_in;
	data_record_t *curr_record = process_data->curr_record;
	ip_node_t color_node1, color_node2;
	
	curr_record->address = g_htonl(node->srcip);
	curr_record->length = g_htonl(node->srclen);
	curr_record->port = g_htonl(node->srcport);

	curr_record->dstaddr = g_htonl(node->dstip);
	curr_record->dstlen = g_htonl(node->dstlen);
	curr_record->dstport = g_htonl(node->dstport);

	curr_record->count = g_htonl(node->count);
	curr_record->proto = g_htonl(node->proto);
	
	// create a tmp nodes to lookup the colors
	color_node1.addr = node->srcip;
	color_node1.network_bits = node->srclen;
	color_node2.addr = node->dstip;
	color_node2.network_bits = node->dstlen;
	
	assign_colors(curr_record, &color_node1, &color_node2, process_data->client);

	process_data->curr_record++;
	process_data->curr_record_id++;

	return FALSE;
}


void send_data_update(client_t *client)
{
	gchar *send_buf;
	msg_header_t header;
	data_record_t *records = NULL;
	process_data_t process_data;

	// lock tree read rwlock
	g_static_rw_lock_reader_lock(&client->rwlock);

	// make sure we're within bounds of the character array
	if (client->mode.data_type > 0 && client->mode.data_type <= DATA_TYPE_MAX) {
		g_debug("sending %s data", data_type_str[client->mode.data_type]);
	}

	process_data.num_records = 0;
	process_data.curr_record_id = 0;
	process_data.data_type = client->mode.data_type;
	process_data.client = client;

	// calculate the number of records (aka the # of nodes in the tree)
	process_data.num_records = g_tree_nnodes(client->node_tree);

	// allocate space for the records
	records = g_new0(data_record_t, process_data.num_records);
	process_data.curr_record = records;

	switch (client->mode.data_type) {
		case SRC_IP:
		case DST_IP:
			g_tree_foreach(client->node_tree, g_tree_foreach_process_ip, &process_data);
		break;

		case SRC_AS:
		case DST_AS:
			g_tree_foreach(client->node_tree, g_tree_foreach_process_as, &process_data);
		break;

		case SRC_PORT:
		case DST_PORT:
			g_tree_foreach(client->node_tree, g_tree_foreach_process_port, &process_data);
		break;

		case SRC_DST_IP:
			g_tree_foreach(client->node_tree, g_tree_foreach_process_src_dst_ip, &process_data);
		break;

		case SRC_DST_AS:
			g_tree_foreach(client->node_tree, g_tree_foreach_process_src_dst_as, &process_data);
		break;

		case SRC_DST_PORT:
			g_tree_foreach(client->node_tree, g_tree_foreach_process_src_dst_port, &process_data);
		break;

		default:
			g_error("unknown/unsupported data type in send_data_update");
		break;
	}

	// unlock tree read rwlock
	g_static_rw_lock_reader_unlock(&client->rwlock);


	// construct the data update buffer
	send_buf = g_new0(gchar, sizeof(msg_header_t) + process_data.num_records * sizeof(data_record_t));

	header.type = g_htonl(DATA_UPDATE);
	header.size = g_htonl(process_data.num_records * sizeof(data_record_t));
	header.extra = g_htonl(client->time);
	
	//g_debug("sending time_stamp=%d(%d)\n", client->time, header.extra);

	memcpy(send_buf, &header, sizeof(msg_header_t));
	memcpy(send_buf + sizeof(msg_header_t), records, process_data.num_records * sizeof(data_record_t));

	// send the data update
	if (!client_socket_write(client, send_buf, sizeof(msg_header_t) + process_data.num_records * sizeof(data_record_t))) {
		g_warning("client send() failed");
	} else {
		g_debug("successfully sent %d bytes (%d records)", (guint) (sizeof(msg_header_t) + process_data.num_records * sizeof(data_record_t)), process_data.num_records);
	}

	g_free(records);
	g_free(send_buf);
}
