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
#include "main_interface.h"
#include "socket.h"

#define PB_TIMEOUT 3


gboolean playback_request_timeout(gpointer data)
{
	// enable the dialog since we timed out
	gtk_widget_set_sensitive(viz->playback_ok_button, TRUE);
	gtk_widget_set_sensitive(viz->playback_cancel_button, TRUE);

	// set the secondary status bar
	main_secondary_statusbar_update("Playback request timed out");

	g_debug("playback request timed out");

	return FALSE;
}


void playback_process_response(pbreq_error_t *response, guint len)
{
	gchar *status;

	guint feed_id = gtk_combo_box_get_active(GTK_COMBO_BOX(viz->playback_feed_name));

	// remove our timeout source
	g_source_destroy(g_main_context_find_source_by_id(NULL, viz->timeout_id));

	// enable dialog once we receive a response
	gtk_widget_set_sensitive(viz->playback_ok_button, TRUE);
	gtk_widget_set_sensitive(viz->playback_cancel_button, TRUE);

	if (len == 0) {
		// if len == 0, the playback request was successful!
		g_debug("playback request was successful");

		// set our current mode to the specified playback feed
		viz->mode.exporter = feed_id;

		// disable our feed menu and change the label
		gtk_widget_set_sensitive(viz->feed_name_menu_button, FALSE);
		gtk_tool_button_set_label(GTK_TOOL_BUTTON(viz->feed_name_menu_button), "Playback");	

		// enable our stop button in the controls window
		gtk_widget_set_sensitive(viz->stop_button, TRUE);

		gtk_widget_hide(viz->playback_dialog);

		viz->playback_active = TRUE;

		// redraw
		gtk_widget_queue_draw(GTK_WIDGET(viz->glext2d));
		gtk_widget_queue_draw(GTK_WIDGET(viz->glext3d));

		status = g_strdup("Playback request successful!");
	} else {
		response->extra = g_ntohl(response->extra);
		response->errorcode = g_ntohl(response->errorcode);

		// otherwise, check the error code
		switch(response->errorcode) {
			case RUNNING:
				status = g_strdup("Error: playback already running");
				g_debug("error when requesting playback: RUNNING");
			break;

			case NO_EXP:
				status = g_strdup("Error: playback feed not supported");
				g_debug("error when requesting playback: NO_EXP");
			break;

			case TOO_EARLY:
				status = g_strdup("Error: playback start time is too early");
				g_debug("error when requesting playback: TOO_EARLY");
			break;

			case TOO_LATE:
				status = g_strdup("Error: playback end time is too late");
				g_debug("error when requesting playback: TOO_LATE");
			break;

			default:
				status = g_strdup("Error: unknown playback response");
				g_debug("unknown playback response: %d", response->errorcode);
			break;
		}
	}

	// set the secondary status bar
	main_secondary_statusbar_update(status);

	g_free(status);
}


void playback_send_request(guint type)
{
	msg_header_t header;

	// construct our message header
	header.type = g_htonl(type);
	header.size = g_htonl(0);

	// send the server our playback request
	if (!socket_write((gchar *) &header, sizeof(msg_header_t))) {
		// hang up if there was a problem sending
		socket_handle_hup(0, 0, 0);
	}
}


void playback_end_action()
{
	// set pause/resume/stop button sensitivity
	gtk_widget_set_sensitive(viz->pause_button, TRUE);
	gtk_widget_set_sensitive(viz->resume_button, FALSE);
	gtk_widget_set_sensitive(viz->stop_button, FALSE);

	// exit playback mode
	viz->playback_active = FALSE;

	// reset our main toolbar buttons to the current mode parameters
	gtk_widget_set_sensitive(viz->feed_name_menu_button, TRUE);
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(viz->feed_name_menu_button), viz->feeds[viz->mode.exporter].name);

	main_secondary_statusbar_update("Playback mode terminated, returning to live updates");
}


void playback_resume_action()
{
	g_debug("playback resume request");
	playback_send_request(PB_PLAY);
}


void playback_pause_action()
{
	g_debug("playback pause request");
	playback_send_request(PB_PAUSE);
}


void playback_stop_action()
{
	g_debug("playback stop request");
	playback_send_request(PB_STOP);
}


void cb_playback_ok_button_clicked(GtkWidget *button, gpointer *data)
{
	pbreq_t request;
	msg_header_t header;
	gchar msg[sizeof(msg_header_t) + sizeof(pbreq_t)];
	time_t start_time, end_time;
	struct tm start_tm, end_tm;

	memset(&start_tm, 0, sizeof(struct tm));
	memset(&end_tm, 0, sizeof(struct tm));

	// calculate the start time
	gtk_calendar_get_date(GTK_CALENDAR(viz->start_calendar), &start_tm.tm_year, &start_tm.tm_mon, &start_tm.tm_mday);
	start_tm.tm_year -= 1900;
	start_tm.tm_hour = gtk_spin_button_get_value(GTK_SPIN_BUTTON(viz->start_hours));
	start_tm.tm_min = gtk_spin_button_get_value(GTK_SPIN_BUTTON(viz->start_minutes));
	start_tm.tm_sec = gtk_spin_button_get_value(GTK_SPIN_BUTTON(viz->start_seconds));
	start_time = mktime(&start_tm);

	// calculate the end time
	gtk_calendar_get_date(GTK_CALENDAR(viz->end_calendar), &end_tm.tm_year, &end_tm.tm_mon, &end_tm.tm_mday);
	end_tm.tm_year -= 1900;
	end_tm.tm_hour = gtk_spin_button_get_value(GTK_SPIN_BUTTON(viz->end_hours));
	end_tm.tm_min = gtk_spin_button_get_value(GTK_SPIN_BUTTON(viz->end_minutes));
	end_tm.tm_sec = gtk_spin_button_get_value(GTK_SPIN_BUTTON(viz->end_seconds));
	end_time = mktime(&end_tm);

	// construct our message header
	header.type = g_htonl(PB_REQUEST);
	header.size = g_htonl(sizeof(pbreq_t));

	// construct our playback request
	request.start_time = g_htonl(start_time);
	request.end_time = g_htonl(end_time);
	request.exporter = g_htonl(gtk_combo_box_get_active(GTK_COMBO_BOX(viz->playback_feed_name)));
	request.rate = g_htonl(gtk_spin_button_get_value(GTK_SPIN_BUTTON(viz->playback_speed)));

	// construct our complete message
	memcpy(msg, &header, sizeof(msg_header_t));
	memcpy(msg + sizeof(msg_header_t), &request, sizeof(pbreq_t));

	g_debug("playback request:  feed:%d start:%d end:%d rate:%f", gtk_combo_box_get_active(GTK_COMBO_BOX(viz->playback_feed_name)), (guint) start_time, (guint) end_time, gtk_spin_button_get_value(GTK_SPIN_BUTTON(viz->playback_speed)));

	// send the server our playback request
	if (!socket_write((gchar *) msg, sizeof(msg))) {
		// hang up if there was a problem sending
		socket_handle_hup(0, 0, 0);
	}

	// timeout in PB_TIMEOUT seconds if a feed list is not received
	viz->timeout_id = g_timeout_add(PB_TIMEOUT * 1000, playback_request_timeout, NULL);

	// disable buttons while waiting for a response from the server
	gtk_widget_set_sensitive(viz->playback_ok_button, FALSE);
	gtk_widget_set_sensitive(viz->playback_cancel_button, FALSE);
}


void playback_calendar_update(GtkWidget *calendar, gint add)
{
	static const gint month_length[2][13] =
	{
		{ 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
		{ 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
	};

	gint day, month, year;
	gint days_in_month;
	gboolean leap_year;

	gtk_calendar_get_date(GTK_CALENDAR(calendar), &year, &month, &day);

	leap_year = g_date_is_leap_year(year);
	days_in_month = month_length[leap_year][month+1];
	
	if (add != 0) {
		day += add;
		if (day < 1) {
			day = (month_length[leap_year][month]) + day;
			month--;
		} else if (day > days_in_month) {
			day -= days_in_month;
			month++;
		}

		if (month < 0) {
			year--;
			leap_year = g_date_is_leap_year(year);
			month = 11;
			day = month_length[leap_year][month+1];
		} else if (month > 11) {
			year++;
			leap_year = g_date_is_leap_year(year);
			month = 0;
			day = 1;
		}
	}

	gtk_calendar_select_month(GTK_CALENDAR(calendar), month, year);
	gtk_calendar_select_day(GTK_CALENDAR(calendar), day);
}


void cb_playback_time_value_changed(GtkSpinButton *widget, gpointer data)
{
	gint value = gtk_spin_button_get_value(widget);

	if (widget == GTK_SPIN_BUTTON(viz->start_seconds)) {
		if (value > 59) {
			gtk_spin_button_set_value(widget, value - 60);
			gtk_spin_button_spin(GTK_SPIN_BUTTON(viz->start_minutes), GTK_SPIN_STEP_FORWARD, 1);
		} else if (value < 0) {
			gtk_spin_button_set_value(widget, value + 60);
			gtk_spin_button_spin(GTK_SPIN_BUTTON(viz->start_minutes), GTK_SPIN_STEP_BACKWARD, 1);
		}
	} else if (widget == GTK_SPIN_BUTTON(viz->start_minutes)) {
		if (value > 59) {
			gtk_spin_button_set_value(widget, value - 60);
			gtk_spin_button_spin(GTK_SPIN_BUTTON(viz->start_hours), GTK_SPIN_STEP_FORWARD, 1);
		} else if (value < 0) {
			gtk_spin_button_set_value(widget, value + 60);
			gtk_spin_button_spin(GTK_SPIN_BUTTON(viz->start_hours), GTK_SPIN_STEP_BACKWARD, 1);
		}
	} else if (widget == GTK_SPIN_BUTTON(viz->start_hours)) {
		if (value > 23) {
			gtk_spin_button_set_value(widget, value - 24);
			playback_calendar_update(viz->start_calendar, 1);
		} else if (value < 0) {
			playback_calendar_update(viz->start_calendar, -1);
			gtk_spin_button_set_value(widget, value + 24);
		}
	} else if (widget == GTK_SPIN_BUTTON(viz->end_seconds)) {
		if (value > 59) {
			gtk_spin_button_set_value(widget, value - 60);
			gtk_spin_button_spin(GTK_SPIN_BUTTON(viz->end_minutes), GTK_SPIN_STEP_FORWARD, 1);
		} else if (value < 0) {
			gtk_spin_button_set_value(widget, value + 60);
			gtk_spin_button_spin(GTK_SPIN_BUTTON(viz->end_minutes), GTK_SPIN_STEP_BACKWARD, 1);
		}
	} else if (widget == GTK_SPIN_BUTTON(viz->end_minutes)) {
		if (value > 59) {
			gtk_spin_button_set_value(widget, value - 60);
			gtk_spin_button_spin(GTK_SPIN_BUTTON(viz->end_hours), GTK_SPIN_STEP_FORWARD, 1);
		} else if (value < 0) {
			gtk_spin_button_set_value(widget, value + 60);
			gtk_spin_button_spin(GTK_SPIN_BUTTON(viz->end_hours), GTK_SPIN_STEP_BACKWARD, 1);
		}
	} else if (widget == GTK_SPIN_BUTTON(viz->end_hours)) {
		if (value > 23) {
			gtk_spin_button_set_value(widget, value - 24);
			playback_calendar_update(viz->end_calendar, 1);
		} else if (value < 0) {
			playback_calendar_update(viz->end_calendar, -1);
			gtk_spin_button_set_value(widget, value + 24);
		}
	}

	// print 01 instead of just 1
	gchar *val = g_strdup_printf("%02d", value);
	gtk_entry_set_text(GTK_ENTRY(widget), val);
	g_free(val);
}


void playback_init()
{
	// playback interface setup
	viz->playback_dialog = glade_xml_get_widget(viz->xml, "playback_dialog");
	viz->playback_ok_button = glade_xml_get_widget(viz->xml, "playback_ok_button");
	viz->playback_cancel_button = glade_xml_get_widget(viz->xml, "playback_cancel_button");
	viz->playback_feed_name = glade_xml_get_widget(viz->xml, "playback_feed_name");
	viz->playback_speed = glade_xml_get_widget(viz->xml, "playback_speed");
	viz->start_calendar = glade_xml_get_widget(viz->xml, "start_calendar");
	viz->start_hours = glade_xml_get_widget(viz->xml, "start_hours");
	viz->start_minutes = glade_xml_get_widget(viz->xml, "start_minutes");
	viz->start_seconds = glade_xml_get_widget(viz->xml, "start_seconds");
	viz->end_calendar = glade_xml_get_widget(viz->xml, "end_calendar");
	viz->end_hours = glade_xml_get_widget(viz->xml, "end_hours");
	viz->end_minutes = glade_xml_get_widget(viz->xml, "end_minutes");
	viz->end_seconds = glade_xml_get_widget(viz->xml, "end_seconds");

	// initialize values to 00
	gchar *val = g_strdup_printf("%02d", 0);
	gtk_entry_set_text(GTK_ENTRY(viz->start_hours), val);
	gtk_entry_set_text(GTK_ENTRY(viz->start_minutes), val);
	gtk_entry_set_text(GTK_ENTRY(viz->start_seconds), val);
	gtk_entry_set_text(GTK_ENTRY(viz->end_hours), val);
	gtk_entry_set_text(GTK_ENTRY(viz->end_minutes), val);
	gtk_entry_set_text(GTK_ENTRY(viz->end_seconds), val);
	g_free(val);
}
