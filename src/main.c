#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <glib.h>
#include <config.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <glade/glade.h>
#include <GL/gl.h>

#include "main.h"
#include "handler.h"
#include "main_interface.h"
#include "info_interface.h"
#include "prefs_interface.h"
#include "controls_interface.h"
#include "playback_interface.h"
#include "socket.h"
#include "trackball.h"

#define ASNAMES_FILE "asnames.data"
#define PORTNUMBERS_FILE "port-numbers.data"
#define PROTONUMBERS_FILE "proto-numbers.data"
#define GLADE_FILE "interface.glade"


gint gtree_compare(gconstpointer node_a_in, gconstpointer node_b_in, gpointer data)
{
	tree_node_t *node_a = (tree_node_t *) node_a_in;
	tree_node_t *node_b = (tree_node_t *) node_b_in;

	return (viz->handler.compare_nodes(node_a, node_b));
}


void main_load_protonumbers(void)
{
	guint i, proto_num, counter = 0;
	gchar *line, **line_split, **desc_split;
	GIOChannel *protonum = NULL;

	// look for the proto-numbers.data file
	if (g_file_test(PRIMARY_PATH PROTONUMBERS_FILE, G_FILE_TEST_EXISTS)) {
		protonum = g_io_channel_new_file(PRIMARY_PATH PROTONUMBERS_FILE, "r", NULL);
	} else if (g_file_test(BACKUP_PATH PROTONUMBERS_FILE, G_FILE_TEST_EXISTS)) {
		protonum = g_io_channel_new_file(BACKUP_PATH PROTONUMBERS_FILE, "r", NULL);
	} else {
		g_error("Could not find the " PROTONUMBERS_FILE " file");
	}

	viz->proto_numbers = g_ptr_array_new();

	while (G_IO_STATUS_EOF != g_io_channel_read_line(protonum, &line, NULL, NULL, NULL)) {
		// split up the data
		line_split = g_strsplit(line, "     ", 2);

		// strip whitespace
		proto_num = atoi(g_strstrip(line_split[0]));
		line_split[1] = g_strstrip(line_split[1]);

		// split abreviation from the rest of the line
		desc_split = g_strsplit(line_split[1], " ", 2);
		desc_split[0] = g_strstrip(desc_split[0]);

		// add null ptrs for any missing proto numbers
		for (i = counter; i < proto_num; ++i) {
			g_ptr_array_add(viz->proto_numbers, NULL);
		}

		// insert description into corresponding protocol number
		g_ptr_array_add(viz->proto_numbers, g_strdup(desc_split[0]));

		counter = proto_num + 1;
		g_strfreev(desc_split);
		g_strfreev(line_split);
		g_free(line);
	}

	g_io_channel_unref(protonum);
}


void main_load_portnumbers(void)
{
	guint i, port_num, counter = 0;
	gchar *line, **line_split, **port_split;
	GIOChannel *portnum = NULL;

	// look for the port-numbers.data file
	if (g_file_test(PRIMARY_PATH PORTNUMBERS_FILE, G_FILE_TEST_EXISTS)) {
		portnum = g_io_channel_new_file(PRIMARY_PATH PORTNUMBERS_FILE, "r", NULL);
	} else if (g_file_test(BACKUP_PATH PORTNUMBERS_FILE, G_FILE_TEST_EXISTS)) {
		portnum = g_io_channel_new_file(BACKUP_PATH PORTNUMBERS_FILE, "r", NULL);
	} else {
		g_error("Could not find the " PORTNUMBERS_FILE " file");
	}

	viz->port_numbers = g_ptr_array_new();

	while (G_IO_STATUS_EOF != g_io_channel_read_line(portnum, &line, NULL, NULL, NULL)) {
		// if the first char is #, skip to next line
		if (line[0] == '#') {
			g_free(line);
			continue;
		}

		// split up port num and desc
		line_split = g_strsplit(line, "/tcp", 2);

		if (line_split[1] == NULL) {
			g_strfreev(line_split);
			g_free(line);
			continue;
		}
		
		// split and strip our desired data
		port_split = g_strsplit_set(line_split[0], " \t", 2);
		line_split[1] = g_strstrip(line_split[1]);
		port_split[0] = g_strstrip(port_split[0]);
		port_split[1] = g_strstrip(port_split[1]);

		port_num = atoi(g_strstrip(port_split[1]));
		
		// if we've already got this port number (duplicate entries)
		if (port_num < counter) {
			g_strfreev(port_split);
			g_strfreev(line_split);
			g_free(line);
			continue;
		}

		// add null ptrs for any missing port numbers
		for (i = counter; i < port_num; ++i) {
			g_ptr_array_add(viz->port_numbers, NULL);
		}

		// if our description is present, use it; otherwise, use the short name
		if (strlen(line_split[1]) > 0) {
			g_ptr_array_add(viz->port_numbers, g_strdup(line_split[1]));
		} else {
			g_ptr_array_add(viz->port_numbers, g_strdup(port_split[0]));
		}

		counter = port_num + 1;
		g_strfreev(port_split);
		g_strfreev(line_split);
		g_free(line);
	}

	g_io_channel_unref(portnum);
}


void main_load_asnames(void)
{
	guint i, len, asnum, counter = 0;
	gchar *line;
	gchar **split;
	GIOChannel *asnames = NULL;

	// look for the asnames.data file
	if (g_file_test(PRIMARY_PATH ASNAMES_FILE, G_FILE_TEST_EXISTS)) {
		asnames = g_io_channel_new_file(PRIMARY_PATH ASNAMES_FILE, "r", NULL);
	} else if (g_file_test(BACKUP_PATH ASNAMES_FILE, G_FILE_TEST_EXISTS)) {
		asnames = g_io_channel_new_file(BACKUP_PATH ASNAMES_FILE, "r", NULL);
	} else {
		g_error("Could not find the " ASNAMES_FILE " file");
	}

	viz->asnames = g_ptr_array_new();

	while (G_IO_STATUS_EOF != g_io_channel_read_line(asnames, &line, NULL, NULL, NULL)) {
		// split up the as name and number
		split = g_strsplit(line, " ", 2);

		// convert the as number to an integer
		asnum = atoi(split[0] + 2);

		// remove whitespace and the endline character
		split[1] = g_strstrip(split[1]);
		len = strlen(split[1]);
		if (split[1][len] == '\n') {
			split[1][len-1] = '\0';
		}

		// add null ptrs for any missing AS numbers
		for (i = counter; i < asnum; ++i) {
			g_ptr_array_add(viz->asnames, NULL);
		}

		g_ptr_array_add(viz->asnames, split[1]);

		counter = asnum + 1;
		g_free(line);
	}

	g_io_channel_unref(asnames);
}


void gtkgl_init(int argc, gchar *argv[])
{
	// init gtkglext
	gtk_gl_init(&argc, &argv);

	// see if we have double buffered support
	viz->glconfig = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB | GDK_GL_MODE_DEPTH | GDK_GL_MODE_DOUBLE);

	if (viz->glconfig == NULL) {
		g_debug("Cannot find the double-buffered visual");
		g_debug("Trying single-buffered visual");

		// attempt to use single buffered
		viz->glconfig = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB | GDK_GL_MODE_DEPTH);
		
		if (viz->glconfig == NULL) {
			g_error("No appropriate OpenGL-capable visual found");
		}
	}

	// make the boring drawing areas super special with opengl capabilities
	gtk_widget_set_gl_capability(viz->glext2d, viz->glconfig, NULL, TRUE, GDK_GL_RGBA_TYPE);
	gtk_widget_set_gl_capability(viz->glext3d, viz->glconfig, NULL, TRUE, GDK_GL_RGBA_TYPE);

	// set the default transparency, lighting, and label state
	viz->transparent = FALSE;
	viz->draw_labels = FALSE;
	viz->disable_lighting = FALSE;
	viz->disable_max_threshold = TRUE;

	// set opengl lighting parameters
	GLfloat light_ambient_3d_local[] = { 0.1f, 0.1f, 0.1f, 1.0f };
	GLfloat light_diffuse_3d_local[] = { 1.2f, 1.2f, 1.2f, 1.0f };
	GLfloat light_position_3d_local[] = { 1.0f, 1.0f, 0.0f, 1.0f };
	GLfloat quads_normal_3d_local[] = {
					0.0, 0.0, 1.0,
					0.0, 0.0, 1.0,
					0.0, 0.0, 1.0,
					0.0, 0.0, 1.0,

					1.0, 0.0, 0.0,
					1.0, 0.0, 0.0,
					1.0, 0.0, 0.0,
					1.0, 0.0, 0.0,

					-1.0, 0.0, 0.0,
					-1.0, 0.0, 0.0,
					-1.0, 0.0, 0.0,
					-1.0, 0.0, 0.0,

					0.0, -1.0, 0.0,
					0.0, -1.0, 0.0,
					0.0, -1.0, 0.0,
					0.0, -1.0, 0.0,

					0.0, 1.0, 0.0,
					0.0, 1.0, 0.0,
					0.0, 1.0, 0.0,
					0.0, 1.0, 0.0,

					0.0, 0.0, 1.0,
					0.0, 0.0, 1.0,
					0.0, 0.0, 1.0,
					0.0, 0.0, 1.0
	};
	memcpy(viz->light_ambient_3d, light_ambient_3d_local, sizeof(viz->light_ambient_3d));
	memcpy(viz->light_diffuse_3d, light_diffuse_3d_local, sizeof(viz->light_diffuse_3d));
	memcpy(viz->light_position_3d, light_position_3d_local, sizeof(viz->light_position_3d));
	memcpy(viz->quads_normal_3d, quads_normal_3d_local, sizeof(viz->quads_normal_3d));
}


void interface_init(void)
{
	// look for the glade interface file
	if (g_file_test(PRIMARY_PATH GLADE_FILE, G_FILE_TEST_EXISTS)) {
		viz->xml = glade_xml_new(PRIMARY_PATH GLADE_FILE, NULL, NULL);
	} else if (g_file_test(BACKUP_PATH GLADE_FILE, G_FILE_TEST_EXISTS)) {
		viz->xml = glade_xml_new(BACKUP_PATH GLADE_FILE, NULL, NULL);
	} else {
		g_error("Could not find the glade interface file");
	}

	// make sure our glade interface loaded properly
	if (!viz->xml) {
		g_error("Could not load the glade interface");
	}

	// connect the signals in the interface.
	glade_xml_signal_autoconnect(viz->xml);

	// initialize all the interfaces
	main_init();
	info_init();
	controls_init();
	playback_init();
	prefs_init();
}


gint main(gint argc, gchar *argv[])
{
	// ignore SIGPIPE
	signal(SIGPIPE, SIG_IGN);

	// allocate our main viz object
	viz = g_new0(viz_t, 1);

	// set default mode parameters
	viz->playback_active = FALSE;
	viz->paused = FALSE;
	viz->mode.exporter = 0;
	viz->mode.data_type = SRC_IP;
	viz->mode.update_period = DEFAULT_UPDATE_INTERVAL;
	viz->mode.ranges_src = 0;
	viz->mode.ranges_dst = 0;
	handler_set_ip_functions();

	// init our node containers
	viz->nodes = g_tree_new_full(gtree_compare, NULL, g_free, NULL);

	// load our AS names from our asnames.data
	g_debug("loading " ASNAMES_FILE);
	main_load_asnames();
	g_debug("done!");

	// load our known port numbers from port-numbers.data
	g_debug("loading " PORTNUMBERS_FILE);
	main_load_portnumbers();
	g_debug("done!");

	// load our protocol numbers from proto-numbers.data
	g_debug("loading " PROTONUMBERS_FILE);
	main_load_protonumbers();
	g_debug("done!");

	// init gtk
	gtk_init(&argc, &argv);

	// init glade and the gtk interfaces
	interface_init();

	// init gtkglext
	gtkgl_init(argc, argv);

	// show the main window
	gtk_widget_show(viz->main_window);

	// enter gtk's main event loop
	gtk_main();

	return 0;
}
