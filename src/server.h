#ifndef _SERVER_H_
#define _SERVER_H_

#include <glib.h>
#include <config.h>

#include "pond.h"
#include "protocol.h"
#include "myflow.h"

#define DEFAULT_LISTEN_PORT	4500
#define DEFAULT_MAX_CLIENTS	5
#define BACKLOG			5
#define CONNECT_TIMEOUT		3

gboolean init_server(void);
void send_feed_list(client_t *client);
gpointer client_thread(gpointer socket);
void client_refresh(client_t *client);
void send_data_update(client_t *client);
void handle_message(client_t *client, gchar *recv_buf, msg_header_t *header);
gboolean client_socket_write(client_t *client, gchar *buf, guint len);
gboolean client_io_in(GIOChannel *source, GIOCondition condition, gpointer client_in);
gboolean client_io_hup(GIOChannel *source, GIOCondition condition, gpointer client_in);

#endif
