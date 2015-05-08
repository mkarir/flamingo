#include <glib/gstdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
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
#include "range.h"
#include "main_interface.h"
#include "socket.h"
#include "glext2d.h"
#include "glext3d.h"

#define CONFIG_FILE 	"client.conf"


alert_widget_t *prefs_add_alert(gboolean checked, gchar *alert_data, gchar *trigger_val, guint trigger_type, gchar *ident)
{
	alert_widget_t *alert;

	alert = g_new0(alert_widget_t, 1);

	// create and show our widgets
	alert->hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(viz->prefs_alert_vbox), alert->hbox, FALSE, FALSE, 0);
	gtk_widget_show(alert->hbox);

	alert->checkbox = gtk_check_button_new();
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alert->checkbox), checked);
	gtk_box_pack_start(GTK_BOX(alert->hbox), alert->checkbox, FALSE, FALSE, 0);
	gtk_widget_show(alert->checkbox);

	alert->alert = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(alert->alert), alert_data);
	gtk_entry_set_width_chars(GTK_ENTRY(alert->alert), 40);
	gtk_entry_set_max_length(GTK_ENTRY(alert->alert), MAX_FILTER_LENGTH - 1);
	gtk_box_pack_start(GTK_BOX(alert->hbox), alert->alert, FALSE, FALSE, 0);
	g_signal_connect(alert->alert, "changed", G_CALLBACK(alert_text_changed), alert);
	gtk_widget_show(alert->alert);

	alert->trigger_val = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(alert->trigger_val), trigger_val);
	gtk_entry_set_width_chars(GTK_ENTRY(alert->trigger_val), 4);
	gtk_box_pack_start(GTK_BOX(alert->hbox), alert->trigger_val, FALSE, FALSE, 0);
	g_signal_connect(alert->trigger_val, "changed", G_CALLBACK(alert_text_changed), alert);
	gtk_widget_show(alert->trigger_val);

	alert->trigger_type = gtk_combo_box_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(alert->trigger_type), "pkts/sec");
	gtk_combo_box_append_text(GTK_COMBO_BOX(alert->trigger_type), "indeg/sec");
	gtk_combo_box_append_text(GTK_COMBO_BOX(alert->trigger_type), "outdeg/sec");
	gtk_combo_box_set_active(GTK_COMBO_BOX(alert->trigger_type), trigger_type);
	gtk_box_pack_start(GTK_BOX(alert->hbox), alert->trigger_type, FALSE, FALSE, 0);
	gtk_widget_show(alert->trigger_type);
	
	alert->ident = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(alert->ident), ident);
	gtk_entry_set_width_chars(GTK_ENTRY(alert->ident), 8);
	gtk_box_pack_start(GTK_BOX(alert->hbox), alert->ident, FALSE, FALSE, 0);
	gtk_widget_show(alert->ident);

	g_signal_emit_by_name(alert->alert, "changed");

	// add to the end of the alert queue
	g_queue_push_tail(viz->prefs_alert_widgets, alert);

	return (alert);
}


void prefs_remove_alert()
{
	alert_widget_t *alert;

	alert = g_queue_pop_tail(viz->prefs_alert_widgets);

	gtk_widget_destroy(alert->alert);
	gtk_widget_destroy(alert->checkbox);
	gtk_widget_destroy(alert->ident);
	gtk_widget_destroy(alert->hbox);

	g_free(alert);
}


void cb_prefs_add_alert_clicked(GtkWidget *button, gpointer *data)
{
	viz->num_alert_widgets++;

	prefs_add_alert(FALSE, "", "", 0, "");
}


void cb_prefs_remove_alert_clicked(GtkWidget *button, gpointer *data)
{
	// make sure there's widgets left to remove
	if (viz->num_alert_widgets <= 0) {
		return;
	}

	viz->num_alert_widgets--;

	prefs_remove_alert();	
}


range_widget_t *prefs_add_range(GQueue *queue, GtkWidget *vbox, gboolean checked, gchar *ip, gchar *bits, gchar *ident)
{
	range_widget_t *range;

	range = g_new0(range_widget_t, 1);

	// create and show our widgets
	range->hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), range->hbox, FALSE, FALSE, 0);
	gtk_widget_show(range->hbox);

	range->checkbox = gtk_check_button_new();
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(range->checkbox), checked);
	gtk_box_pack_start(GTK_BOX(range->hbox), range->checkbox, FALSE, FALSE, 0);
	gtk_widget_show(range->checkbox);

	range->ip = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(range->ip), ip);
	gtk_entry_set_width_chars(GTK_ENTRY(range->ip), 15);
	gtk_box_pack_start(GTK_BOX(range->hbox), range->ip, FALSE, FALSE, 0);
	g_signal_connect(range->ip, "changed", G_CALLBACK(range_text_changed), range);
	gtk_widget_show(range->ip);

	range->bits = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(range->bits), bits);
	gtk_entry_set_width_chars(GTK_ENTRY(range->bits), 2);
	gtk_box_pack_start(GTK_BOX(range->hbox), range->bits, FALSE, FALSE, 0);
	g_signal_connect(range->bits, "changed", G_CALLBACK(range_text_changed), range);
	gtk_widget_show(range->bits);

	g_signal_emit_by_name(range->bits, "changed");

	range->ident = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(range->ident), ident);
	gtk_entry_set_width_chars(GTK_ENTRY(range->ident), 10);
	gtk_box_pack_start(GTK_BOX(range->hbox), range->ident, FALSE, FALSE, 0);
	gtk_widget_show(range->ident);

	// add to the end of the range queue
	g_queue_push_tail(queue, range);

	return (range);
}


void prefs_remove_range(GQueue *queue)
{
	range_widget_t *range;

	range = g_queue_pop_tail(queue);

	gtk_widget_destroy(range->ip);
	gtk_widget_destroy(range->bits);
	gtk_widget_destroy(range->checkbox);
	gtk_widget_destroy(range->ident);
	gtk_widget_destroy(range->hbox);

	g_free(range);
}


void cb_prefs_add_src_range_clicked(GtkWidget *button, gpointer *data)
{
	viz->num_src_widgets++;

	prefs_add_range(viz->prefs_src_range_widgets, viz->prefs_range_src_vbox, FALSE, "", "", "");
}


void cb_prefs_remove_src_range_clicked(GtkWidget *button, gpointer *data)
{
	// make sure there's widgets left to remove
	if (viz->num_src_widgets <= 0) {
		return;
	}

	viz->num_src_widgets--;

	prefs_remove_range(viz->prefs_src_range_widgets);	
}


void cb_prefs_add_dst_range_clicked(GtkWidget *button, gpointer *data)
{
	viz->num_dst_widgets++;

	prefs_add_range(viz->prefs_dst_range_widgets, viz->prefs_range_dst_vbox, FALSE, "", "", "");
}


void cb_prefs_remove_dst_range_clicked(GtkWidget *button, gpointer *data)
{
	// make sure there's widgets left to remove
	if (viz->num_dst_widgets <= 0) {
		return;
	}

	viz->num_dst_widgets--;

	prefs_remove_range(viz->prefs_dst_range_widgets);
}


void prefs_hide_dialog()
{
	gtk_widget_hide(viz->preferences_dialog);

	// redraw
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext2d));
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext3d));
}


void prefs_update_keyfile_from_widgets()
{
	guint i;

	// update our keyfile with all our current prefs
	g_key_file_set_integer(viz->prefs, "Preferences", "Max_Threshold_Value", gtk_spin_button_get_value(GTK_SPIN_BUTTON(viz->prefs_max_threshold_value)));
	g_key_file_set_string(viz->prefs, "Preferences", "Default_Server_Host", gtk_entry_get_text(GTK_ENTRY(viz->prefs_default_host)));
	g_key_file_set_integer(viz->prefs, "Preferences", "Default_Server_Port", atoi(gtk_entry_get_text(GTK_ENTRY(viz->prefs_default_port))));
	g_key_file_set_string(viz->prefs, "Preferences", "Default_Server_Password", gtk_entry_get_text(GTK_ENTRY(viz->prefs_default_password)));
	g_key_file_set_string(viz->prefs, "Preferences", "OpenGL_Label_Font", gtk_entry_get_text(GTK_ENTRY(viz->prefs_label_font)));

	// output alerts
	g_key_file_set_integer(viz->prefs, "Preferences", "Alert_Count", viz->num_alert_widgets);	

	for (i = 0; i < viz->num_alert_widgets; ++i) {
		gchar *alert_key = g_strdup_printf("Alert_Data_%d", i);
		gchar *ident_key = g_strdup_printf("Alert_Name_%d", i);
		gchar *trigger_val_key = g_strdup_printf("Alert_Val_%d", i);
		gchar *trigger_type_key = g_strdup_printf("Alert_Type_%d", i);
		gchar *checkbox_key = g_strdup_printf("Alert_Enabled_%d", i);

		alert_widget_t *alert = g_queue_peek_nth(viz->prefs_alert_widgets, i);

		g_key_file_set_boolean(viz->prefs, "Preferences", checkbox_key, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(alert->checkbox)));
		g_key_file_set_integer(viz->prefs, "Preferences", trigger_type_key, gtk_combo_box_get_active(GTK_COMBO_BOX(alert->trigger_type)));
		g_key_file_set_string(viz->prefs, "Preferences", trigger_val_key, gtk_entry_get_text(GTK_ENTRY(alert->trigger_val)));
		g_key_file_set_string(viz->prefs, "Preferences", ident_key, gtk_entry_get_text(GTK_ENTRY(alert->ident)));
		g_key_file_set_string(viz->prefs, "Preferences", alert_key, gtk_entry_get_text(GTK_ENTRY(alert->alert)));

		g_free(alert_key);
		g_free(ident_key);
		g_free(trigger_val_key);
		g_free(trigger_type_key);
		g_free(checkbox_key);
	}

	// output src ranges
	g_key_file_set_integer(viz->prefs, "Preferences", "Src_Range_Count", viz->num_src_widgets);	

	for (i = 0; i < viz->num_src_widgets; ++i) {
		gchar *addr_key = g_strdup_printf("Src_Range_Addr_%d", i);
		gchar *bits_key = g_strdup_printf("Src_Range_Network_Bits_%d", i);
		gchar *ident_key = g_strdup_printf("Src_Range_Name_%d", i);
		gchar *checkbox_key = g_strdup_printf("Src_Range_Enabled_%d", i);

		range_widget_t *range = g_queue_peek_nth(viz->prefs_src_range_widgets, i);

		g_key_file_set_boolean(viz->prefs, "Preferences", checkbox_key, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(range->checkbox)));
		g_key_file_set_string(viz->prefs, "Preferences", ident_key, gtk_entry_get_text(GTK_ENTRY(range->ident)));
		g_key_file_set_string(viz->prefs, "Preferences", addr_key, gtk_entry_get_text(GTK_ENTRY(range->ip)));
		g_key_file_set_string(viz->prefs, "Preferences", bits_key, gtk_entry_get_text(GTK_ENTRY(range->bits)));

		g_free(addr_key);
		g_free(bits_key);
		g_free(ident_key);
		g_free(checkbox_key);
	}

	// output dst ranges
	g_key_file_set_integer(viz->prefs, "Preferences", "Dst_Range_Count", viz->num_dst_widgets);

	for (i = 0; i < viz->num_dst_widgets; ++i) {
		gchar *addr_key = g_strdup_printf("Dst_Range_Addr_%d", i);
		gchar *bits_key = g_strdup_printf("Dst_Range_Network_Bits_%d", i);
		gchar *ident_key = g_strdup_printf("Dst_Range_Name_%d", i);
		gchar *checkbox_key = g_strdup_printf("Dst_Range_Enabled_%d", i);

		range_widget_t *range = g_queue_peek_nth(viz->prefs_dst_range_widgets, i);

		g_key_file_set_boolean(viz->prefs, "Preferences", checkbox_key, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(range->checkbox)));
		g_key_file_set_string(viz->prefs, "Preferences", ident_key, gtk_entry_get_text(GTK_ENTRY(range->ident)));
		g_key_file_set_string(viz->prefs, "Preferences", addr_key, gtk_entry_get_text(GTK_ENTRY(range->ip)));
		g_key_file_set_string(viz->prefs, "Preferences", bits_key, gtk_entry_get_text(GTK_ENTRY(range->bits)));

		g_free(addr_key);
		g_free(bits_key);
		g_free(ident_key);
		g_free(checkbox_key);
	}
}


void prefs_update_widgets_from_keyfile(GKeyFile *prefs)
{
	guint i;
	gchar *tmp_str;
	GError *error = NULL;
	guint max_threshold_value, default_port;

	// retrieve the max threshold maximum value
	max_threshold_value = g_key_file_get_integer(prefs, "Preferences", "Max_Threshold_Value", &error);

	if (!error) {
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(viz->prefs_max_threshold_value), max_threshold_value);
	} else {
		g_error_free(error);
		error = NULL;
	}

	// retrieve the default server port number
	default_port = g_key_file_get_integer(prefs, "Preferences", "Default_Server_Port", &error);

	if (!error) {
		gchar *port = g_strdup_printf("%d", default_port);
		gtk_entry_set_text(GTK_ENTRY(viz->prefs_default_port), port);
		g_free(port);
	} else {
		g_error_free(error);
		error = NULL;
	}

	// retrieve the default server hostname
	if ((tmp_str = g_key_file_get_string(prefs, "Preferences", "Default_Server_Host", NULL))) {
		gtk_entry_set_text(GTK_ENTRY(viz->prefs_default_host), tmp_str);
		g_free(tmp_str);
	}

	// retrieve the default server password
	if ((tmp_str = g_key_file_get_string(prefs, "Preferences", "Default_Server_Password", NULL))) {
		gtk_entry_set_text(GTK_ENTRY(viz->prefs_default_password), tmp_str);
		g_free(tmp_str);
	}

	// retrieve the opengl label font
	if ((tmp_str = g_key_file_get_string(prefs, "Preferences", "OpenGL_Label_Font", NULL))) {
		gtk_entry_set_text(GTK_ENTRY(viz->prefs_label_font), tmp_str);
		g_free(tmp_str);
	} else {
		gtk_entry_set_text(GTK_ENTRY(viz->prefs_label_font), DEFAULT_LABEL_FONT);
	}


	// clean up the old widgets
	for (i = 0; i < viz->num_alert_widgets; ++i) {
		prefs_remove_alert();
	}

	// get number of alert widgets that we have
	viz->num_alert_widgets = g_key_file_get_integer(prefs, "Preferences", "Alert_Count", &error);

	if (!error) {
		// no error
	} else {
		viz->num_alert_widgets = 0;
		g_error_free(error);
		error = NULL;
	}

	// retrieve our alert widgets
	for (i = 0; i < viz->num_alert_widgets; ++i) {
		guint trigger_type = -1;
		gboolean enabled = FALSE;
		gchar *ident = NULL, *alert = NULL, *trigger_val = NULL;

		gchar *alert_key = g_strdup_printf("Alert_Data_%d", i);
		gchar *ident_key = g_strdup_printf("Alert_Name_%d", i);
		gchar *trigger_val_key = g_strdup_printf("Alert_Val_%d", i);
		gchar *trigger_type_key = g_strdup_printf("Alert_Type_%d", i);
		gchar *checkbox_key = g_strdup_printf("Alert_Enabled_%d", i);

		// read in from keyfile
		ident = g_key_file_get_string(prefs, "Preferences", ident_key, NULL);
		alert = g_key_file_get_string(prefs, "Preferences", alert_key, NULL);
		trigger_val = g_key_file_get_string(prefs, "Preferences", trigger_val_key, NULL);

		trigger_type = g_key_file_get_integer(prefs, "Preferences", trigger_type_key, &error);

		if (error) {
			// if there's an error retrieving the trigger type, disable that alert by default
			enabled = FALSE;
			g_error_free(error);
			error = NULL;
		}

		enabled = g_key_file_get_boolean(prefs, "Preferences", checkbox_key, &error);

		if (error) {
			// if there's an error retrieving the boolean, disable that alert by default
			enabled = FALSE;
			g_error_free(error);
			error = NULL;
		}

		// make sure our parameters were read in successfully before adding the alert
		if (ident && alert && trigger_val && trigger_type >= 0 && trigger_type <= 2) {
			prefs_add_alert(enabled, alert, trigger_val, trigger_type, ident);
		}

		g_free(alert_key);
		g_free(ident_key);
		g_free(trigger_val_key);
		g_free(trigger_type_key);
		g_free(checkbox_key);

		g_free(ident);
		g_free(alert);
	}


	// clean up the old widgets
	for (i = 0; i < viz->num_src_widgets; ++i) {
		prefs_remove_range(viz->prefs_src_range_widgets);
	}

	// get number of src widgets that we have
	viz->num_src_widgets = g_key_file_get_integer(prefs, "Preferences", "Src_Range_Count", &error);

	if (!error) {
		// no error
	} else {
		viz->num_src_widgets = 0;
		g_error_free(error);
		error = NULL;
	}

	// retrieve our src widgets to monitor
	for (i = 0; i < viz->num_src_widgets; ++i) {
		gboolean enabled = FALSE;
		gchar *ident = NULL, *ip = NULL, *bits = NULL;

		gchar *addr_key = g_strdup_printf("Src_Range_Addr_%d", i);
		gchar *bits_key = g_strdup_printf("Src_Range_Network_Bits_%d", i);
		gchar *ident_key = g_strdup_printf("Src_Range_Name_%d", i);
		gchar *checkbox_key = g_strdup_printf("Src_Range_Enabled_%d", i);

		// read in from keyfile
		ident = g_key_file_get_string(prefs, "Preferences", ident_key, NULL);
		ip = g_key_file_get_string(prefs, "Preferences", addr_key, NULL);
		bits = g_key_file_get_string(prefs, "Preferences", bits_key, NULL);

		enabled = g_key_file_get_boolean(prefs, "Preferences", checkbox_key, &error);

		if (error) {
			// if there's an error retrieving the boolean, disable that range by default
			enabled = FALSE;
			g_error_free(error);
			error = NULL;
		}

		// make sure our parameters were read in successfully before adding the range
		if (ident && ip && bits) {
			prefs_add_range(viz->prefs_src_range_widgets, viz->prefs_range_src_vbox, enabled, ip, bits, ident);
		}

		g_free(addr_key);
		g_free(bits_key);
		g_free(ident_key);
		g_free(checkbox_key);

		g_free(ident);
		g_free(ip);
		g_free(bits);
	}


	// clean up the old widgets
	for (i = 0; i < viz->num_dst_widgets; ++i) {
		prefs_remove_range(viz->prefs_dst_range_widgets);
	}

	// get number of dst widgets that we have from the keyfile
	viz->num_dst_widgets = g_key_file_get_integer(prefs, "Preferences", "Dst_Range_Count", &error);

	if (!error) {
		// no error
	} else {
		viz->num_dst_widgets = 0;
		g_error_free(error);
		error = NULL;
	}

	// retrieve our dst ip ranges to monitor
	for (i = 0; i < viz->num_dst_widgets; ++i) {
		gboolean enabled = FALSE;
		gchar *ident = NULL, *ip = NULL, *bits = NULL;

		gchar *addr_key = g_strdup_printf("Dst_Range_Addr_%d", i);
		gchar *bits_key = g_strdup_printf("Dst_Range_Network_Bits_%d", i);
		gchar *ident_key = g_strdup_printf("Dst_Range_Name_%d", i);
		gchar *checkbox_key = g_strdup_printf("Dst_Range_Enabled_%d", i);

		// read in from keyfile
		ident = g_key_file_get_string(prefs, "Preferences", ident_key, NULL);
		ip = g_key_file_get_string(prefs, "Preferences", addr_key, NULL);
		bits = g_key_file_get_string(prefs, "Preferences", bits_key, NULL);

		enabled = g_key_file_get_boolean(prefs, "Preferences", checkbox_key, &error);

		if (error) {
			// if there's an error retrieving the boolean, disable that range by default
			enabled = FALSE;
			g_error_free(error);
			error = NULL;
		}

		// make sure our parameters were read in successfully before adding the range
		if (ident && ip && bits) {
			prefs_add_range(viz->prefs_dst_range_widgets, viz->prefs_range_dst_vbox, enabled, ip, bits, ident);
		}

		g_free(addr_key);
		g_free(bits_key);
		g_free(ident_key);
		g_free(checkbox_key);

		g_free(ident);
		g_free(ip);
		g_free(bits);
	}
}


gboolean prefs_write()
{
	// write out our current config to the users home dir
	FILE *out;
	gboolean updated = FALSE;
	gchar *prefs_old, *prefs_new;
	gchar *config_dir = g_build_filename(g_get_home_dir(), ".flamingo", NULL);
	gchar *config_file = g_build_filename(config_dir, CONFIG_FILE, NULL);

	g_debug("writing out config file");

	// create our .flamingo directory if it doesn't exist
	if (!g_file_test(config_dir, G_FILE_TEST_IS_DIR)) {
		if (g_mkdir(config_dir, 0755) < 0) {
			g_warning("could not create $HOME/.flamingo directory");
		}
	}

	// take a snapshot of our keyfile before updating it
	prefs_old = g_key_file_to_data(viz->prefs, NULL, NULL);

	// update our keyfile with all our current prefs
	prefs_update_keyfile_from_widgets();

	// convert our updated keyfile to a string
	prefs_new = g_key_file_to_data(viz->prefs, NULL, NULL);

	// return true if our preferences actually changed
	if (g_ascii_strcasecmp(prefs_new, prefs_old) != 0) {
		updated = TRUE;
	}

	// write the keyfile string out to disk
	out = g_fopen(config_file, "w");
	if (out) {
		fputs(prefs_new, out);
		fclose(out);
	}

	g_free(prefs_new);
	g_free(prefs_old);
	g_free(config_dir);
	g_free(config_file);

	return (updated);
}


void cb_prefs_preferences_button_clicked(GtkWidget *button, gpointer *data)
{
	gtk_widget_show(viz->preferences_dialog);
}


void cb_prefs_cancel_button_clicked(GtkWidget *button, gpointer *data)
{
	// discard anything thats changed in the widgets and reload the right data from the keyfile
	prefs_update_widgets_from_keyfile(viz->prefs);

	prefs_hide_dialog();
}


void prefs_activate()
{
	// update max threshold min/max
	gtk_range_set_range(GTK_RANGE(viz->threshold_min_range), 0, gtk_spin_button_get_value(GTK_SPIN_BUTTON(viz->prefs_max_threshold_value)));
	gtk_range_set_range(GTK_RANGE(viz->threshold_max_range), 0, gtk_spin_button_get_value(GTK_SPIN_BUTTON(viz->prefs_max_threshold_value)));

	// if the max threshold is currently disabled, make sure to set the max value 
	if (viz->disable_max_threshold) {
		viz->threshold_max = G_MAXUINT;
	}

	// set our connect dialog to default host/port/password
	gtk_entry_set_text(GTK_ENTRY(viz->connect_hostname_entry), gtk_entry_get_text(GTK_ENTRY(viz->prefs_default_host)));
	gtk_entry_set_text(GTK_ENTRY(viz->connect_port_entry), gtk_entry_get_text(GTK_ENTRY(viz->prefs_default_port)));
	gtk_entry_set_text(GTK_ENTRY(viz->connect_password_entry), gtk_entry_get_text(GTK_ENTRY(viz->prefs_default_password)));

	// add all our ranges
	if (range_add_all_from_widgets()) {
		g_debug("ranges changed - sending mode update");

		// only send a mode update when our ranges change
		main_update_mode();
	}

	// add all our alerts
	if (alerts_add_all_from_widgets()) {
		// only send the alerts if they have changed
		alerts_send_update();
	}
}


void cb_prefs_ok_button_clicked(GtkWidget *button, gpointer *data)
{
	// only process prefs if there was a change
	if (prefs_write()) {
		// activate the new preferences
		prefs_activate();
	}

	prefs_hide_dialog();
}


void prefs_init()
{
	GKeyFile *tmp_prefs;
	gchar *conf_path, *pwd_path, *home_path, *etc_path, *pwd;

	// setup prefs interface
	viz->preferences_dialog = glade_xml_get_widget(viz->xml, "preferences_dialog");

	// general widgets
	viz->prefs_max_threshold_value = glade_xml_get_widget(viz->xml, "prefs_max_threshold_value");
	viz->prefs_default_host = glade_xml_get_widget(viz->xml, "prefs_default_host");
	viz->prefs_default_port = glade_xml_get_widget(viz->xml, "prefs_default_port");
	viz->prefs_default_password = glade_xml_get_widget(viz->xml, "prefs_default_password");
	viz->prefs_label_font = glade_xml_get_widget(viz->xml, "prefs_label_font");

	// range widgets
	viz->prefs_range_src_vbox = glade_xml_get_widget(viz->xml, "prefs_src_range_vbox");
	viz->prefs_range_dst_vbox = glade_xml_get_widget(viz->xml, "prefs_dst_range_vbox");
	viz->prefs_src_range_widgets = g_queue_new();
	viz->prefs_dst_range_widgets = g_queue_new();

	// alert widgets
	viz->prefs_alert_vbox = glade_xml_get_widget(viz->xml, "prefs_alert_vbox");
	viz->prefs_alert_widgets = g_queue_new();

	// build all possible paths to our config file
	pwd = g_get_current_dir();
	pwd_path = g_build_filename(pwd, CONFIG_FILE, NULL);
	home_path = g_build_filename(g_get_home_dir(), ".flamingo", CONFIG_FILE, NULL);
	etc_path = g_build_filename(G_DIR_SEPARATOR_S, "etc", "flamingo", CONFIG_FILE, NULL);

	// initialize our GKeyFile prefs
	viz->prefs = g_key_file_new();
	tmp_prefs = g_key_file_new();

	g_debug("searching paths: %s:%s:%s", pwd_path, home_path, etc_path);

	// search for a config file to use (search order: pwd, $HOME/.flamingo/ /etc/flamingo/)
	if (g_file_test(pwd_path, G_FILE_TEST_EXISTS)) {
		conf_path = pwd_path;
	} else if (g_file_test(home_path, G_FILE_TEST_EXISTS)) {
		conf_path = home_path;
	} else if (g_file_test(etc_path, G_FILE_TEST_EXISTS)) {
		conf_path = etc_path;
	} else {
		g_debug("no " CONFIG_FILE " found in the search paths");
		goto cleanup;
	}

	g_debug("config file found: %s", conf_path);

	// attempt to load our config file
	if (!g_key_file_load_from_file(tmp_prefs, conf_path, G_KEY_FILE_KEEP_COMMENTS, NULL)) {
		g_warning("error loading config from %s", conf_path);
		goto cleanup;
	}

	// load our prefs into our preference widgets
	prefs_update_widgets_from_keyfile(tmp_prefs);

	cleanup:
		// write out our prefs to disk
		prefs_write();

		// activate the new preferences
		prefs_activate();

		// free up all our allocated structures
		g_free(pwd);
		g_free(pwd_path);
		g_free(home_path);
		g_free(etc_path);
		g_key_file_free(tmp_prefs);
}
