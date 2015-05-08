#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glib.h>
#include <pcap.h>
#include <config.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <glade/glade.h>
#include <GL/gl.h>

#include "main.h"
#include "alerts.h"

float strtof(const char *nptr, char **endptr);


void alerts_send_update()
{
	gchar *msg;
	msg_header_t header;
	guint alert_size = viz->num_alerts * sizeof(wlreq_t);

	// if we're sending our new alerts, clear out current alert box since the id's are invalidated
	gtk_list_store_clear(viz->alert_store);

	// send all alerts to the server
	header.type = g_htonl(WL_UPDATE);
	header.size = g_htonl(alert_size);

	msg = g_new0(gchar, sizeof(msg_header_t) + alert_size);

	memcpy(msg, &header, sizeof(msg_header_t));
	memcpy(msg + sizeof(msg_header_t), viz->alerts, alert_size);

	if (!socket_write(msg, sizeof(msg_header_t) + alert_size)) {
		// hang up if there was a problem sending
		socket_handle_hup(0, 0, 0);
	}

	g_free(msg);
}


gboolean alert_check_valid(const gchar *alert_str, const gchar *trigger_val)
{
	struct bpf_program program;
	gfloat trigger_float = -1.0;

	trigger_float = strtof(trigger_val, NULL);

	// make sure strtof went ok
	if (trigger_float == 0 && strcmp("0", trigger_val) != 0) {
		return FALSE;
	}

	// make sure the trigger isnt blank
	if (strlen(trigger_val) == 0) {
		return FALSE;
	}

	// make sure the alert isnt blank
	if (strlen(alert_str) == 0) {
		return FALSE;
	}

	// make sure the trigger val is valid
	if (atof(trigger_val) < 0) {
		return FALSE;
	}
 
	// compile the filter into a bpf program
	if (pcap_compile_nopcap(65536, DLT_EN10MB, &program, (gchar *) alert_str, FALSE, 0) != 0) {
		return FALSE;
	}

	return TRUE;
}


void alert_text_changed(GtkEditable *editable, gpointer alert_data)
{
	GdkColor color;
	alert_widget_t *alert = (alert_widget_t *) alert_data;

	// get the text from the editable widget
	gchar *val = gtk_editable_get_chars(GTK_EDITABLE(alert->trigger_val), 0, -1);
	gchar *alert_str = gtk_editable_get_chars(GTK_EDITABLE(alert->alert), 0, -1);

	// if its an empty string, set bg to white
	if (strlen(val) == 0 && strlen(alert_str) == 0) {
		gdk_color_parse("white", &color);
	} else {
		if (alert_check_valid(alert_str, val)) {
			// if valid, set bg to green
			gdk_color_parse("#afffaf", &color);
		} else {
			// else, set bg to red
			gdk_color_parse("#ffafaf", &color);
		}
	}

	// change the bg color
	gtk_widget_modify_base(GTK_WIDGET(alert->alert), GTK_STATE_NORMAL, &color);
	gtk_widget_modify_base(GTK_WIDGET(alert->trigger_val), GTK_STATE_NORMAL, &color);

	g_free(alert_str);
	g_free(val);
}


gboolean alerts_add_all_from_widgets()
{
	guint i, old_num;
	wlreq_t *curr_alert, *old_alerts, *tmp_alerts;

	// record our old values to figure out whether there is a change in the alerts
	old_num = viz->num_alerts;
	old_alerts = g_new0(wlreq_t, viz->num_alerts);

	// record our old alerts but make sure that current alerts exist
	if (viz->alerts) {
		memcpy(old_alerts, viz->alerts, viz->num_alerts * sizeof(wlreq_t));
	}

	// make room for the max number of alerts possible (num of widgets)
	tmp_alerts = g_new0(wlreq_t, viz->num_alert_widgets);
	curr_alert = tmp_alerts;
	viz->num_alerts = 0;

	// count the number of valid alerts we have
	for (i = 0; i < viz->num_alert_widgets; ++i) {
		alert_widget_t *widget = g_queue_peek_nth(viz->prefs_alert_widgets, i);

		guint trigger_type = gtk_combo_box_get_active(GTK_COMBO_BOX(widget->trigger_type));
		const gchar *trigger_val = gtk_entry_get_text(GTK_ENTRY(widget->trigger_val));
		const gchar *alert_str = gtk_entry_get_text(GTK_ENTRY(widget->alert));

		// make sure this alert is actually enabled (checkbox state == TRUE)
		if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget->checkbox))) {
			continue;
		}

		// set our attributes
		curr_alert->filter_id = g_htonl(i);
		memset(curr_alert->filter, 0, MAX_FILTER_LENGTH);
		strncpy(curr_alert->filter, alert_str, MAX_FILTER_LENGTH - 1);
		curr_alert->trigger_type = g_htonl(trigger_type); 
		//curr_alert->trigger_val = g_htonl(atof(trigger_val));
		curr_alert->trigger_val = atof(trigger_val);

		// check if its a valid filter string
		if (!alert_check_valid(curr_alert->filter, trigger_val)) {
			continue;
		}

		g_debug("FOUND VALID ALERT!!!  id:%d  str:%s  type:%d  val:%f", i, curr_alert->filter, trigger_type, atof(trigger_val));

		// increment the number of alerts we need to send over
		viz->num_alerts++;
		curr_alert++;
	}

	// free/reallocate our current alerts
	g_free(viz->alerts);
	viz->alerts = g_new0(wlreq_t, viz->num_alerts);
	curr_alert = viz->alerts;

	// copy our tmp alerts to our main ones and free the tmp ones
	memcpy(viz->alerts, tmp_alerts, viz->num_alerts * sizeof(wlreq_t));
	g_free(tmp_alerts);

	// if there's a diff num of valid alerts, we definitely need an update
	if (old_num != viz->num_alerts) {
		g_free(old_alerts);
		return TRUE;
	}

	// if the content of the alerts is different, we need an update
	if (memcmp(old_alerts, viz->alerts, viz->num_alerts * sizeof(wlreq_t)) != 0) {
		g_free(old_alerts);
		return TRUE;
	}

	g_free(old_alerts);
	return FALSE;
}

