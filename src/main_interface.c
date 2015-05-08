#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <glib.h>
#include <config.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <glade/glade.h>
#include <GL/gl.h>

#include "main.h"
#include "alerts.h"
#include "main_interface.h"
#include "socket.h"


void main_update_mode(void)
{
	// only update our mode if we're currently connected
	if (!viz->connected) {
		return;
	}
	
	// check aggregation availability
	avail_feed_t *feed = &viz->feeds[viz->mode.exporter];
	if (!feed->aggregation) {
		// always disable aggregation if the feed doesnt support it
		viz->mode.aggregated = FALSE;

		// disable aggregation button
		gtk_widget_set_sensitive(viz->aggregation_button, FALSE);
	} else {
		// enable aggregation button
		gtk_widget_set_sensitive(viz->aggregation_button, TRUE);
	}

	// if our new feed doesn't support AS data types, grey them out
	if (viz->feeds[viz->mode.exporter].as_availability) {
		gtk_widget_set_sensitive(viz->type_src_as, TRUE);
		gtk_widget_set_sensitive(viz->type_dst_as, TRUE);
		gtk_widget_set_sensitive(viz->type_src_dst_as, TRUE);
	} else {
		gtk_widget_set_sensitive(viz->type_src_as, FALSE);
		gtk_widget_set_sensitive(viz->type_dst_as, FALSE);
		gtk_widget_set_sensitive(viz->type_src_dst_as, FALSE);

		// if our current type is an AS data type, change it
		if (viz->mode.data_type == SRC_AS || viz->mode.data_type == DST_AS || viz->mode.data_type == SRC_DST_AS) {
			viz->mode.data_type = SRC_IP;
		}
	}

	gchar *msg;
	fmode_t mode;
	msg_header_t header;
	frange_t *range;

	guint range_size = (viz->mode.ranges_src + viz->mode.ranges_dst) * sizeof(frange_t);

	msg = g_new0(gchar, sizeof(msg_header_t) + sizeof(fmode_t) + range_size);

	header.type = g_htonl(MODE_UPDATE);
	header.size = g_htonl(sizeof(fmode_t) + range_size);

	mode.aggregated = g_htonl(viz->mode.aggregated);
	mode.exporter = g_htonl(viz->mode.exporter);
	mode.data_type = g_htonl(viz->mode.data_type);
	mode.update_period = g_htonl(viz->mode.update_period);
	mode.ranges_src = g_htonl(viz->mode.ranges_src);
	mode.ranges_dst = g_htonl(viz->mode.ranges_dst);

	memcpy(msg, &header, sizeof(msg_header_t));
	memcpy(msg + sizeof(msg_header_t), &mode, sizeof(fmode_t));
	memcpy(msg + sizeof(msg_header_t) + sizeof(fmode_t), viz->mode_ranges, range_size);

	range = (frange_t *) (msg + sizeof(msg_header_t) + sizeof(fmode_t));

	// send the server our new mode request
	if (!socket_write(msg, sizeof(msg_header_t) + sizeof(fmode_t) + range_size)) {
		// hang up if there was a problem sending
		socket_handle_hup(0, 0, 0);
	}

	g_free(msg);

	g_debug("sent mode update!");

	// update the label on our tool buttons
	gchar *update_period_str = g_strdup_printf("%d seconds", viz->mode.update_period);
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(viz->type_menu_button), data_type_str[viz->mode.data_type]);
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(viz->feed_name_menu_button), viz->feeds[viz->mode.exporter].name);
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(viz->update_period_menu_button), update_period_str);
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(viz->aggregation_button), (viz->mode.aggregated ? "Aggregated" : "Not Aggr."));
	g_free(update_period_str);

	// update our column header labels (eg. "Address" -> "AS Number")
	GtkTreeViewColumn *tree_column;
	gchar *title = viz->handler.get_node_title();
	tree_column = gtk_tree_view_get_column(GTK_TREE_VIEW(viz->watch_list), LIST_ADDRESS_COLUMN);
	gtk_tree_view_column_set_title(tree_column, title);
	tree_column = gtk_tree_view_get_column(GTK_TREE_VIEW(viz->full_list), LIST_ADDRESS_COLUMN);
	gtk_tree_view_column_set_title(tree_column, title);
	g_free(title);

	// update secondary status bar
	gchar *status_msg = g_strdup_printf("Mode Updated - %s / %s / %ds", viz->feeds[viz->mode.exporter].name, data_type_str[viz->mode.data_type], viz->mode.update_period);
	main_secondary_statusbar_update(status_msg);
	g_free(status_msg);

	// clear out our list boxes
	gtk_list_store_clear(viz->watch_store);
	gtk_list_store_clear(viz->full_store);

	// destroy all our nodes
	if (viz->nodes != NULL) {
		g_tree_destroy(viz->nodes);
	}
	viz->nodes = g_tree_new_full(gtree_compare, NULL, g_free, NULL);

	// redraw
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext2d));
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext3d));

	// initialize our interface elements for the particular data type
	viz->handler.init_interface();
}


void main_remove_feed_name_child(GtkWidget *child, gpointer data)
{
	gtk_container_remove(GTK_CONTAINER(viz->feed_name_menu), child);
}


void cb_main_feed_name_menu_activate(GtkMenuItem *menuitem, gpointer feed_id)
{
	// set our new feed name and send the update to the server
	viz->mode.exporter = GPOINTER_TO_INT(feed_id);
	main_update_mode();
}


void cb_main_playback_feed_activate(GtkMenuItem *menuitem, gpointer feed_id)
{
	// display our playback dialog
	gtk_widget_show(viz->playback_dialog);
}


void main_update_feed_menu()
{
	// enable the main interface window since we now have the feed list
	gtk_widget_set_sensitive(viz->main_window, TRUE);

	// clear out the current feed names
	gtk_container_foreach(GTK_CONTAINER(viz->feed_name_menu), main_remove_feed_name_child, NULL);

	// clear out our playback feed menu
	gchar *curr;
	gtk_combo_box_set_active(GTK_COMBO_BOX(viz->playback_feed_name), 0);
	while ((curr = gtk_combo_box_get_active_text(GTK_COMBO_BOX(viz->playback_feed_name)))) {
		g_free(curr);
		gtk_combo_box_remove_text(GTK_COMBO_BOX(viz->playback_feed_name), 0);
		gtk_combo_box_set_active(GTK_COMBO_BOX(viz->playback_feed_name), 0);
	}

	// add feed names to the menu
	guint i;
	GtkWidget *menu_item;
	for (i = 0; i < viz->num_feeds; ++i) {
		menu_item = gtk_menu_item_new_with_label(viz->feeds[i].name);

		// connect our activate signal for when the user selects the menu item
		g_signal_connect(menu_item, "activate", G_CALLBACK(cb_main_feed_name_menu_activate), GINT_TO_POINTER(i));

		// append the feed name menu item
		gtk_menu_shell_append(GTK_MENU_SHELL(viz->feed_name_menu), menu_item);
		gtk_widget_show(menu_item);

		// add feed to our playback feed menu also
		gtk_combo_box_append_text(GTK_COMBO_BOX(viz->playback_feed_name), viz->feeds[i].name);
	}

	// set the active/selected feed in our playback menu
	gtk_combo_box_set_active(GTK_COMBO_BOX(viz->playback_feed_name), 0);

	// add our playback feed menu option
	menu_item = gtk_menu_item_new_with_label("Playback Feed");
	g_signal_connect(menu_item, "activate", G_CALLBACK(cb_main_playback_feed_activate), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(viz->feed_name_menu), menu_item);
	gtk_widget_show(menu_item);

	// on connect, reset all button labels and mode vars to default (SRC_IP, 10, feed, etc)
	viz->playback_active = FALSE;
	viz->paused = FALSE;
	viz->mode.exporter = 0;
	viz->mode.data_type = SRC_IP;
	viz->mode.update_period = DEFAULT_UPDATE_INTERVAL;
	viz->mode.aggregated = TRUE;

	// send the default mode over to server
	handler_set_ip_functions();
	main_update_mode();

	// send the current alert filters over to the server
	alerts_send_update();

	// update primary status bar
	gchar *pri_status_msg = g_strdup_printf("Connected to %s:%d - %d available feed(s)", inet_ntoa(viz->server_addr), viz->server_port, viz->num_feeds);
	main_primary_statusbar_update(pri_status_msg);
	g_free(pri_status_msg);

	// make the connect button into a disconnect button
	GtkWidget *icon = gtk_image_new_from_icon_name("gtk-disconnect", GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_widget_show(icon);
	gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(viz->connect_menu_button), icon);
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(viz->connect_menu_button), "Disconnect");

	// enable appropriate interface buttons now that we're connected
	gtk_widget_set_sensitive(viz->connect_menu_button, TRUE);
	gtk_widget_set_sensitive(viz->feed_name_menu_button, TRUE);
	gtk_widget_set_sensitive(viz->type_menu_button, TRUE);
	gtk_widget_set_sensitive(viz->update_period_menu_button, TRUE);
	gtk_widget_set_sensitive(viz->pause_button, TRUE);
	gtk_widget_set_sensitive(viz->resume_button, FALSE);
	gtk_widget_set_sensitive(viz->stop_button, FALSE);
}


void cb_main_show_controls_button_clicked(GtkWidget *button, gpointer *data)
{
	gtk_window_present(GTK_WINDOW(viz->controls_window));
	gtk_window_present(GTK_WINDOW(viz->info_window));
}


void main_primary_statusbar_update(gchar *status_text)
{
	gtk_statusbar_pop(GTK_STATUSBAR(viz->primary_statusbar), viz->primary_statusbar_id);
	gtk_statusbar_push(GTK_STATUSBAR(viz->primary_statusbar), viz->primary_statusbar_id, status_text);
}


void main_secondary_statusbar_update(gchar *status_text)
{
	gtk_statusbar_pop(GTK_STATUSBAR(viz->secondary_statusbar), viz->secondary_statusbar_id);
	gtk_statusbar_push(GTK_STATUSBAR(viz->secondary_statusbar), viz->secondary_statusbar_id, status_text);
}


void cb_main_update_period_menu_activate(GtkMenuItem *menuitem, gpointer period)
{
	// set our new update period and send the update to the server
	viz->mode.update_period = GPOINTER_TO_INT(period);
	main_update_mode();
}


void cb_notaggregated_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	// change to not aggregated mode
	viz->mode.aggregated = FALSE;

	// send our mode update request to the server
	main_update_mode();	
}


void cb_aggregated_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	// change to aggregated mode
	viz->mode.aggregated = TRUE;

	// send our mode update request to the server
	main_update_mode();	
}


void cb_main_type_src_dst_as_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	// set the mode data type
	viz->mode.data_type = SRC_DST_AS;

	// set the handler functions
	handler_set_src_dst_as_functions();

	// send our mode update request to the server
	main_update_mode();
}


void cb_main_type_src_dst_port_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	// set the mode data type
	viz->mode.data_type = SRC_DST_PORT;

	// set the handler functions
	handler_set_src_dst_port_functions();

	// send our mode update request to the server
	main_update_mode();
}


void cb_main_type_src_dst_ip_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	// set the mode data type
	viz->mode.data_type = SRC_DST_IP;

	// set the handler functions
	handler_set_src_dst_ip_functions();

	// send our mode update request to the server
	main_update_mode();
}


void cb_main_type_src_ip_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	// set the mode data type (NetFlow Src IP)
	viz->mode.data_type = SRC_IP;

	// set the ip handler functions
	handler_set_ip_functions();

	// send our mode update request to the server
	main_update_mode();
}


void cb_main_type_dst_ip_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	// set the mode data type (NetFlow Dst IP)
	viz->mode.data_type = DST_IP;

	// set the ip handler functions
	handler_set_ip_functions();

	// send our mode update request to the server
	main_update_mode();
}


void cb_main_type_src_port_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	// set the mode data type (NetFlow Src Port)
	viz->mode.data_type = SRC_PORT;

	// set the port handler functions
	handler_set_port_functions();

	// send our mode update request to the server
	main_update_mode();
}


void cb_main_type_dst_port_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	// set the mode data type (NetFlow Dst Port)
	viz->mode.data_type = DST_PORT;

	// set the port handler functions
	handler_set_port_functions();

	// send our mode update request to the server
	main_update_mode();
}


void cb_main_type_src_as_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	// set the mode data type (NetFlow Src AS)
	viz->mode.data_type = SRC_AS;

	// set the as handler functions
	handler_set_as_functions();

	// send our mode update request to the server
	main_update_mode();
}


void cb_main_type_dst_as_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	// set the mode data type (NetFlow Dst AS)
	viz->mode.data_type = DST_AS;

	// set the as handler functions
	handler_set_as_functions();

	// send our mode update request to the server
	main_update_mode();
}


void cb_main_connect_menu_button_clicked(GtkWidget *button, gpointer *data)
{
	if (g_ascii_strcasecmp(gtk_tool_button_get_label(GTK_TOOL_BUTTON(viz->connect_menu_button)), "Disconnect") == 0) {
		// disconnect!
		socket_handle_hup(0, 0, 0);
	} else {
		// show the connection dialog
		gtk_widget_show(viz->connect_dialog);
	}
}


void cb_main_connect_dialog_button_clicked(GtkWidget *button, gpointer *data)
{
	guint port;
	gchar *status_text;
	const gchar *hostname, *password;
	msg_header_t header;
	gchar *msg;

	// retrieve the input from the entry boxes in connect_dialog
	hostname = gtk_entry_get_text(GTK_ENTRY(viz->connect_hostname_entry));
	password = gtk_entry_get_text(GTK_ENTRY(viz->connect_password_entry));
	port = g_ascii_strtod(gtk_entry_get_text(GTK_ENTRY(viz->connect_port_entry)), NULL);

	// disable the main interface window until we receive the feed list
	gtk_widget_set_sensitive(viz->main_window, FALSE);

	// attempt to connect to specified host/port
	if (socket_connect(hostname, port)) {
		status_text = g_strdup_printf("Waiting for feed list from %s:%d", hostname, port);
		g_debug("connected to %s:%d", hostname, port);

		viz->connected = TRUE;

		// send password to server
		header.type = g_htonl(CONNECT);
		header.extra = g_htonl(CONNECT_MAGIC);
		header.size = g_htonl(strlen(password));

		msg = g_new0(gchar, sizeof(msg_header_t) + strlen(password));
		memcpy(msg, &header, sizeof(msg_header_t));
		memcpy(msg + sizeof(msg_header_t), password, strlen(password));

		if (!socket_write(msg, sizeof(msg_header_t) + strlen(password))) {
			// hang up if there was a problem sending
			socket_handle_hup(0, 0, 0);
		}

		g_free(msg);
	} else {
		status_text = g_strdup_printf("Error connecting to %s:%d", hostname, port);
		g_debug("error connecting to %s:%d", hostname, port);

		// enable the main interface window since there was an error
		gtk_widget_set_sensitive(viz->main_window, TRUE);
	}

	// update primary statusbar with connection info
	main_primary_statusbar_update(status_text);
	g_free(status_text);

	// hide the connection dialog
	gtk_widget_hide(viz->connect_dialog);
}


gboolean cb_main_window_configure_event(GtkWidget *widget, GdkEventConfigure *event, gpointer user_data)
{
	// redraw called here to prevent leftover screen artifacts after resizing
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext2d));
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext3d));

	return FALSE;
}


void main_init()
{
	// get our main window
	viz->main_window = glade_xml_get_widget(viz->xml, "main_window");

	// get our gtkglext drawing areas
	viz->glext2d = glade_xml_get_widget(viz->xml, "glext2d");
	viz->glext3d = glade_xml_get_widget(viz->xml, "glext3d");

	// connect_dialog widgets
	viz->connect_menu_button = glade_xml_get_widget(viz->xml, "connect_menu_button");
	viz->connect_dialog = glade_xml_get_widget(viz->xml, "connect_dialog");
	viz->connect_hostname_entry = glade_xml_get_widget(viz->xml, "connect_hostname_entry");
	viz->connect_password_entry = glade_xml_get_widget(viz->xml, "connect_password_entry");
	viz->connect_port_entry = glade_xml_get_widget(viz->xml, "connect_port_entry");

	// type menu widgets
	viz->type_src_ip = glade_xml_get_widget(viz->xml, "type_src_ip");
	viz->type_dst_ip = glade_xml_get_widget(viz->xml, "type_dst_ip");
	viz->type_src_as = glade_xml_get_widget(viz->xml, "type_src_as");
	viz->type_dst_as = glade_xml_get_widget(viz->xml, "type_dst_as");
	viz->type_src_port = glade_xml_get_widget(viz->xml, "type_src_port");
	viz->type_dst_port = glade_xml_get_widget(viz->xml, "type_dst_port");
	viz->type_src_dst_ip = glade_xml_get_widget(viz->xml, "type_src_dst_ip");
	viz->type_src_dst_as = glade_xml_get_widget(viz->xml, "type_src_dst_as");
	viz->type_src_dst_port = glade_xml_get_widget(viz->xml, "type_src_dst_port");

	// hook our GtkMenuToolButton up to our type menu
	GtkWidget *type_menu = glade_xml_get_widget(viz->xml, "type_menu");
	viz->type_menu_button = glade_xml_get_widget(viz->xml, "type_menu_button");
	gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(viz->type_menu_button), type_menu);

	// hook our GtkMenuToolButton up to our aggregation menu
	viz->aggregation_menu = glade_xml_get_widget(viz->xml, "aggregation_menu");
	viz->aggregation_button = glade_xml_get_widget(viz->xml, "aggregation_button");
	gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(viz->aggregation_button), viz->aggregation_menu);

	// hook our GtkMenuToolButton up to our update period menu
	viz->update_period_menu = glade_xml_get_widget(viz->xml, "update_period_menu");
	viz->update_period_menu_button = glade_xml_get_widget(viz->xml, "update_period_menu_button");
	gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(viz->update_period_menu_button), viz->update_period_menu);

	// hook our GtkMenuToolButton up to our feed name menu
	viz->feed_name_menu = glade_xml_get_widget(viz->xml, "feed_name_menu");
	viz->feed_name_menu_button = glade_xml_get_widget(viz->xml, "feed_name_menu_button");
	gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(viz->feed_name_menu_button), viz->feed_name_menu);

	// preferences toolbar button
	viz->preferences_button = glade_xml_get_widget(viz->xml, "preferences_button");

	// add update periods to the menu button
	guint i;
	gchar *label;
	const guint update_periods[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 15, 20, 30, 45, 60 };

	for (i = 0; i < sizeof(update_periods) / sizeof(guint); ++i) {
		if (update_periods[i] == 1) {
			label = g_strdup_printf("1 second");
		} else {
			label = g_strdup_printf("%d seconds", update_periods[i]);
		}

		GtkWidget *menu_item = gtk_menu_item_new_with_label(label);
		g_signal_connect(menu_item, "activate", G_CALLBACK(cb_main_update_period_menu_activate), GINT_TO_POINTER(update_periods[i]));
		gtk_menu_shell_append(GTK_MENU_SHELL(viz->update_period_menu), menu_item);
		gtk_widget_show(menu_item);

		g_free(label);
	}

	
	// setup primary and secondary status bars
	viz->primary_statusbar = glade_xml_get_widget(viz->xml, "primary_statusbar");
	viz->secondary_statusbar = glade_xml_get_widget(viz->xml, "secondary_statusbar");
	viz->primary_statusbar_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(viz->primary_statusbar), "primary_statusbar");
	viz->secondary_statusbar_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(viz->secondary_statusbar), "secondary_statusbar");
	main_primary_statusbar_update("Not Connected");
	main_secondary_statusbar_update("Welcome to Flamingo!");
}
