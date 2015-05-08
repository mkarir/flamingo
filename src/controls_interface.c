#include <glib/gstdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <config.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <glade/glade.h>
#include <GL/gl.h>

#include "main.h"
#include "socket.h"
#include "glext3d.h"
#include "glext2d.h"
#include "info_interface.h"
#include "playback_interface.h"

#define PNG_MAX_DIMENSION 1600


void cb_controls_load_view_clicked(GtkButton *button, gpointer user_data)
{
	// create a GtkFileChooser to open a view
	GtkWidget *dialog = gtk_file_chooser_dialog_new("Open View",
					GTK_WINDOW(viz->controls_window),
					GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					NULL);

	GtkFileFilter *view_filter = gtk_file_filter_new();
	gtk_file_filter_set_name(view_filter, "view files");
	gtk_file_filter_add_pattern(view_filter, "*.view");
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), view_filter);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		FILE *in;
		gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

		// read in view data from file
		in = g_fopen(filename, "r");
		if (in) {
			glext3d_input_view_parameters(in);
			glext2d_input_view_parameters(in);
			fclose(in);

			gtk_widget_queue_draw(GTK_WIDGET(viz->glext3d));
			gtk_widget_queue_draw(GTK_WIDGET(viz->glext2d));		
		}

		g_free(filename);
	}

	// destroy open file dialog
	gtk_widget_destroy(dialog);
}


void cb_controls_save_view_clicked(GtkButton *button, gpointer user_data)
{
	// create a GtkFileChooser to select where to save the view
	GtkWidget *dialog = gtk_file_chooser_dialog_new("Save View",
					GTK_WINDOW(viz->controls_window),
					GTK_FILE_CHOOSER_ACTION_SAVE,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
					NULL);

	GtkFileFilter *view_filter = gtk_file_filter_new();
	gtk_file_filter_set_name(view_filter, "view files");
	gtk_file_filter_add_pattern(view_filter, "*.view");
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), view_filter);
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), ".view");

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		FILE *out;
		gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

		// write out view data to file
		out = g_fopen(filename, "w");
		if (out) {
			glext3d_output_view_parameters(out);
			glext2d_output_view_parameters(out);
			fclose(out);
		}

		g_free(filename);
	}

	// destroy save file dialog
	gtk_widget_destroy(dialog);
}


void cb_controls_export_3d_button_clicked(GtkWidget *button, gpointer *data)
{
	guint width, height;
	GLfloat ratio, widget_width, widget_height;

	// use the width/height ratio of 3d widget to get exact reproduction of 3d view
	widget_width = viz->glext3d->allocation.width;
	widget_height = viz->glext3d->allocation.height;

	if (widget_width > widget_height) {
		ratio = widget_width / widget_height;
		width = PNG_MAX_DIMENSION;
		height = PNG_MAX_DIMENSION / ratio;
	} else {
		ratio = widget_height / widget_width;
		height = PNG_MAX_DIMENSION;
		width = PNG_MAX_DIMENSION / ratio;
	}

	// set up our off-screen gl context
	GdkPixbuf *pixbuf;
	GdkGLConfig *glconfig = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB | GDK_GL_MODE_DEPTH | GDK_GL_MODE_SINGLE);
	GdkPixmap *pixmap = gdk_pixmap_new(NULL, width, height, 24);
	GdkGLDrawable *gldrawable = GDK_GL_DRAWABLE(gdk_pixmap_set_gl_capability(pixmap, glconfig, NULL));
	GdkGLContext *glcontext = gdk_gl_context_new(gldrawable, NULL, FALSE, GDK_GL_RGBA_TYPE);

	gdk_gl_drawable_gl_begin(gldrawable, glcontext);

	// render the 3d scene into the buffer
	glext3d_render_export(width, height);
	glFlush();

	gdk_gl_drawable_gl_end(gldrawable);


	// retrieve our GdkPixbuf from our off-screen drawable area
	pixbuf = gdk_pixbuf_get_from_drawable(NULL, GDK_DRAWABLE(pixmap), NULL, 0, 0, 0, 0, -1, -1);

	// create a GtkFileChooser to select where to save the file
	GtkWidget *dialog = gtk_file_chooser_dialog_new("Export PNG Image",
					GTK_WINDOW(viz->controls_window),
					GTK_FILE_CHOOSER_ACTION_SAVE,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
					NULL);

	// set file chooser to only see png files and suggest png as an extension
	GtkFileFilter *png_filter = gtk_file_filter_new();
	gtk_file_filter_set_name(png_filter, "png files");
	gtk_file_filter_add_pattern(png_filter, "*.png");
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), png_filter);
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), ".png");

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

		// write our off-screen buffer out to a lossless png file
		if (!gdk_pixbuf_save(pixbuf, filename, "png", NULL, NULL)) {
			g_warning("failed to write png image to %s", filename);
		}

		g_free(filename);
	}

	// destroy save file dialog
	gtk_widget_destroy(dialog);

	// unref/cleanup our off-screen context
	g_object_unref(G_OBJECT(pixbuf));
	gdk_gl_context_destroy(glcontext);
	gdk_pixmap_unset_gl_capability(pixmap);
	g_object_unref(G_OBJECT(pixmap));
}


void cb_controls_stop_button_clicked(GtkWidget *button, gpointer *data)
{
	// don't do anything if we're not in playback mode
	if (!viz->playback_active) {
		return;
	}

	// if in playback mode, send a message to server
	playback_stop_action();

	// end our playback session
	playback_end_action();
}


void cb_controls_pause_button_clicked(GtkWidget *button, gpointer *data)
{
	// playback mode
	if (viz->playback_active) {
		playback_pause_action();

		// interface elements to update when in playback mode
		gtk_widget_set_sensitive(viz->stop_button, FALSE);
		gtk_widget_set_sensitive(viz->pause_button, FALSE);
		gtk_widget_set_sensitive(viz->resume_button, TRUE);
		gtk_widget_set_sensitive(viz->connect_menu_button, FALSE);
		gtk_widget_set_sensitive(viz->feed_name_menu_button, FALSE);
		gtk_widget_set_sensitive(viz->type_menu_button, FALSE);
		gtk_widget_set_sensitive(viz->update_period_menu_button, FALSE);
		gtk_widget_set_sensitive(viz->aggregation_button, FALSE);
		gtk_widget_set_sensitive(viz->preferences_button, FALSE);
	} else {
		// disable pause and enable resume
		gtk_widget_set_sensitive(viz->pause_button, FALSE);
		gtk_widget_set_sensitive(viz->resume_button, TRUE);

		// disable menu buttons that shouldnt be active when paused
		gtk_widget_set_sensitive(viz->connect_menu_button, FALSE);
		gtk_widget_set_sensitive(viz->feed_name_menu_button, FALSE);
		gtk_widget_set_sensitive(viz->type_menu_button, FALSE);
		gtk_widget_set_sensitive(viz->update_period_menu_button, FALSE);
		gtk_widget_set_sensitive(viz->aggregation_button, FALSE);
		gtk_widget_set_sensitive(viz->preferences_button, FALSE);

		// set status var
		viz->paused = TRUE;

		// schedule a redraw
		gtk_widget_queue_draw(GTK_WIDGET(viz->glext2d));
		gtk_widget_queue_draw(GTK_WIDGET(viz->glext3d));
	}
}


void cb_controls_resume_button_clicked(GtkWidget *button, gpointer *data)
{
	// playback mode
	if (viz->playback_active) {
		// send resume message to server
		playback_resume_action();

		// interface elements to update when in playback mode
		gtk_widget_set_sensitive(viz->stop_button, TRUE);
		gtk_widget_set_sensitive(viz->pause_button, TRUE);
		gtk_widget_set_sensitive(viz->resume_button, FALSE);
		gtk_widget_set_sensitive(viz->connect_menu_button, TRUE);
		gtk_widget_set_sensitive(viz->feed_name_menu_button, FALSE);
		gtk_widget_set_sensitive(viz->type_menu_button, TRUE);
		gtk_widget_set_sensitive(viz->update_period_menu_button, TRUE);
		gtk_widget_set_sensitive(viz->aggregation_button, TRUE);
		gtk_widget_set_sensitive(viz->preferences_button, TRUE);
	} else {
		// interface elements to update when in live mode

		// disable resume and enable pause
		gtk_widget_set_sensitive(viz->pause_button, TRUE);
		gtk_widget_set_sensitive(viz->resume_button, FALSE);

		// enable menu buttons that should be active when not paused
		gtk_widget_set_sensitive(viz->connect_menu_button, TRUE);
		gtk_widget_set_sensitive(viz->feed_name_menu_button, TRUE);
		gtk_widget_set_sensitive(viz->type_menu_button, TRUE);
		gtk_widget_set_sensitive(viz->update_period_menu_button, TRUE);
		gtk_widget_set_sensitive(viz->aggregation_button, TRUE);
		gtk_widget_set_sensitive(viz->preferences_button, TRUE);

		// set status var
		viz->paused = FALSE;

		// process our stored "paused" data just as if it was a socket update
		socket_process_data_update(viz->paused_records, viz->paused_len, viz->paused_header.extra);
	}
}


void cb_controls_disable_lighting_check_box_toggled(GtkToggleButton *toggle_button, gpointer user_data)
{
	if (gtk_toggle_button_get_active(toggle_button)) {
		viz->disable_lighting = TRUE;
	} else {
		viz->disable_lighting = FALSE;
	}

	gtk_widget_queue_draw(GTK_WIDGET(viz->glext3d));
}


void cb_controls_enable_labels_check_box_toggled(GtkToggleButton *toggle_button, gpointer user_data)
{
	if (gtk_toggle_button_get_active(toggle_button)) {
		viz->draw_labels = TRUE;
	} else {
		viz->draw_labels = FALSE;
	}

	// redraw
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext2d));
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext3d));
}


void cb_controls_disable_max_threshold_check_box_toggled(GtkToggleButton *toggle_button, gpointer user_data)
{
	if (gtk_toggle_button_get_active(toggle_button)) {
		gtk_widget_set_sensitive(viz->threshold_max_range, FALSE);
		gtk_widget_set_sensitive(viz->threshold_max_label, FALSE);

		// set the current threshold to G_MAXUINT
		viz->threshold_max = G_MAXUINT;

		viz->disable_max_threshold = TRUE;
	} else {
		gtk_widget_set_sensitive(viz->threshold_max_range, TRUE);
		gtk_widget_set_sensitive(viz->threshold_max_label, TRUE);

		// restore the previous max threshold
		viz->threshold_max = gtk_range_get_value(GTK_RANGE(viz->threshold_max_range));

		viz->disable_max_threshold = FALSE;
	}

	// redraw
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext2d));
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext3d));
}


gboolean cb_controls_port2_min_value_changed(GtkRange *range, GtkScrollType scroll, gdouble value, gpointer user_data)
{
	viz->port2_min = gtk_range_get_value(GTK_RANGE(viz->port2_min_range));

	// schedule a redraw of the gl widgets as the range has changed
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext2d));
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext3d));

	return FALSE;
}


gboolean cb_controls_port2_max_value_changed(GtkRange *range, GtkScrollType scroll, gdouble value, gpointer user_data)
{
	viz->port2_max = gtk_range_get_value(GTK_RANGE(viz->port2_max_range));

	// schedule a redraw of the gl widgets as the port has changed
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext2d));
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext3d));

	return FALSE;
}


gboolean cb_controls_port1_min_value_changed(GtkRange *range, GtkScrollType scroll, gdouble value, gpointer user_data)
{
	viz->port1_min = gtk_range_get_value(GTK_RANGE(viz->port1_min_range));

	// schedule a redraw of the gl widgets as the range has changed
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext2d));
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext3d));

	return FALSE;
}


gboolean cb_controls_port1_max_value_changed(GtkRange *range, GtkScrollType scroll, gdouble value, gpointer user_data)
{
	viz->port1_max = gtk_range_get_value(GTK_RANGE(viz->port1_max_range));

	// schedule a redraw of the gl widgets as the port has changed
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext2d));
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext3d));

	return FALSE;
}


gboolean cb_controls_threshold_min_value_changed(GtkRange *range, GtkScrollType scroll, gdouble value, gpointer user_data)
{
	viz->threshold_min = gtk_range_get_value(GTK_RANGE(viz->threshold_min_range));

	// schedule a redraw of the gl widgets as the range has changed
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext2d));
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext3d));

	return FALSE;
}


gboolean cb_controls_threshold_max_value_changed(GtkRange *range, GtkScrollType scroll, gdouble value, gpointer user_data)
{
	viz->threshold_max = gtk_range_get_value(GTK_RANGE(viz->threshold_max_range));

	// schedule a redraw of the gl widgets as the range has changed
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext2d));
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext3d));

	return FALSE;
}


void controls_init()
{
	// get our controls window
	viz->controls_window = glade_xml_get_widget(viz->xml, "controls_window");

	viz->transparent_check_box = glade_xml_get_widget(viz->xml, "transparent_check_box");
	viz->labels_check_box = glade_xml_get_widget(viz->xml, "labels_check_box");
	viz->lighting_check_box = glade_xml_get_widget(viz->xml, "lighting_check_box");

	// buttons in the controls window
	viz->pause_button = glade_xml_get_widget(viz->xml, "pause_button");
	viz->resume_button = glade_xml_get_widget(viz->xml, "resume_button");
	viz->stop_button = glade_xml_get_widget(viz->xml, "stop_button");

	// line/slice ranges/labels
	viz->line_scale_range = glade_xml_get_widget(viz->xml, "line_scale_range");
	viz->line_scale_label = glade_xml_get_widget(viz->xml, "line_scale_label");
	viz->slice_scale_range = glade_xml_get_widget(viz->xml, "slice_scale_range");
	viz->slice_scale_label = glade_xml_get_widget(viz->xml, "slice_scale_label");

	// scale widgets
	viz->threshold_min_range = glade_xml_get_widget(viz->xml, "threshold_min_range");
	viz->threshold_max_range = glade_xml_get_widget(viz->xml, "threshold_max_range");
	viz->port1_min_range = glade_xml_get_widget(viz->xml, "port1_min_range");
	viz->port1_max_range = glade_xml_get_widget(viz->xml, "port1_max_range");
	viz->port2_min_range = glade_xml_get_widget(viz->xml, "port2_min_range");
	viz->port2_max_range = glade_xml_get_widget(viz->xml, "port2_max_range");

	// scale labels
	viz->threshold_min_label = glade_xml_get_widget(viz->xml, "threshold_min_label");
	viz->threshold_max_label = glade_xml_get_widget(viz->xml, "threshold_max_label");
	viz->port1_min_label = glade_xml_get_widget(viz->xml, "port1_min_label");
	viz->port1_max_label = glade_xml_get_widget(viz->xml, "port1_max_label");
	viz->port2_min_label = glade_xml_get_widget(viz->xml, "port2_min_label");
	viz->port2_max_label = glade_xml_get_widget(viz->xml, "port2_max_label");

	// initial scale values
	viz->threshold_min = gtk_range_get_value(GTK_RANGE(viz->threshold_min_range));
	viz->threshold_max = G_MAXUINT;
	viz->port1_min = gtk_range_get_value(GTK_RANGE(viz->port1_min_range));
	viz->port1_max = gtk_range_get_value(GTK_RANGE(viz->port1_max_range));
	viz->port2_min = gtk_range_get_value(GTK_RANGE(viz->port2_min_range));
	viz->port2_max = gtk_range_get_value(GTK_RANGE(viz->port2_max_range));

	viz->disable_max_threshold = TRUE;
}
