#ifndef __MAIN_H__
#define __MAIN_H__

#include <glib.h>
#include <config.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <glade/glade.h>

#include "prefs_interface.h"
#include "handler.h"
#include "socket.h"
#include "range.h"

#define PRIMARY_PATH "../data/"
#define BACKUP_PATH FLAMINGO_DIR "/"

#define DEFAULT_LABEL_FONT	"courier 8"
#define CUBE_SIZE		2.0
#define COORD_SIZE		512.0
#define STANDARD_UNIT_SIZE	(GLfloat) (CUBE_SIZE / COORD_SIZE)
#define Z_HEIGHT_ADJ		.001
#define Z_LABEL_ADJ		.005
#define SHIFT(var, shift, mask) (var >> shift) & mask

enum {
	FLAMINGO_LOGO_CALL_LIST = 1,
	GREY_2D_SQUARE_CALL_LIST,
	GREY_3D_SQUARE_CALL_LIST,
	GREY_3D_TWO_SIDED_CALL_LIST,
	WHITE_2D_SQUARE_CALL_LIST,
	WHITE_3D_SQUARE_CALL_LIST,
	WHITE_3D_TWO_SIDED_CALL_LIST,
	WIRE_CUBE_CALL_LIST,
	LABELS_2D_AS_CALL_LIST,
	LABELS_3D_AS_CALL_LIST,
	LABELS_3D_SRC_DEST_AS_CALL_LIST
};

enum {
	LIST_DISPLAY_COLOR_COLUMN = 0,
	LIST_VOLUME_COLUMN,
	LIST_ADDRESS_COLUMN,
	LIST_DESC_COLUMN,
	LIST_PRIVATE_COLOR_COLUMN,
	LIST_N_COLUMNS
};

enum {
	ALERT_TIME_COLUMN = 0,
	ALERT_STRING_COLUMN,
	ALERT_VALUE_COLUMN,
	ALERT_N_COLUMNS
};

typedef struct {
	// main gtk window elements
	GladeXML *xml;
	GtkWidget *main_window;
	GtkWidget *controls_window;
	GtkWidget *info_window;

	// toolbar buttons
	GtkWidget *connect_menu_button;
	GtkWidget *type_menu_button;
	GtkWidget *feed_name_menu_button;
	GtkWidget *update_period_menu_button;
	GtkWidget *aggregation_button;
	GtkWidget *preferences_button;
	GtkWidget *feed_name_menu;
	GtkWidget *update_period_menu;
	GtkWidget *aggregation_menu;

	// buttons in the controls window
	GtkWidget *pause_button;
	GtkWidget *resume_button;
	GtkWidget *stop_button;

	// connect_dialog widgets
	GtkWidget *connect_dialog;
	GtkWidget *connect_hostname_entry;
	GtkWidget *connect_password_entry;
	GtkWidget *connect_port_entry;
	
	// statusbar widgets and context ids
	guint primary_statusbar_id;
	GtkWidget *primary_statusbar;
	guint secondary_statusbar_id;
	GtkWidget *secondary_statusbar;

	// gtktree views
	GtkWidget *watch_list;
	GtkListStore *watch_store;
	GtkWidget *full_list;
	GtkListStore *full_store;
	GtkWidget *alert_list;
	GtkListStore *alert_store;

	// gtkgl related
	GtkWidget *glext2d;
	GtkWidget *glext3d;
	GdkGLConfig *glconfig;

	// ranges
	guint num_src_ranges;
	guint num_dst_ranges;
	ip_range_t *src_ranges;
	ip_range_t *dst_ranges;
	bound_t src_bound;
	bound_t dst_bound;
	bound_t src_dst_port_bound;

	// node containers
	GTree *nodes;

	// type menu widgets
	GtkWidget *type_src_ip;
	GtkWidget *type_dst_ip;
	GtkWidget *type_src_as;
	GtkWidget *type_dst_as;
	GtkWidget *type_src_port;
	GtkWidget *type_dst_port;
	GtkWidget *type_src_dst_ip;
	GtkWidget *type_src_dst_as;
	GtkWidget *type_src_dst_port;

	// pause/resume vars
	data_record_t *paused_records;
	msg_header_t paused_header;
	guint paused_len;
	gboolean paused;

	// various range scales (threshold, port ranges, etc)
	guint threshold_min;
	guint threshold_max;
	guint port1_min;
	guint port1_max;
	guint port2_min;
	guint port2_max;
	GtkWidget *threshold_min_range;
	GtkWidget *threshold_max_range;
	GtkWidget *port1_min_range;
	GtkWidget *port1_max_range;
	GtkWidget *port2_min_range;
	GtkWidget *port2_max_range;
	GtkWidget *threshold_min_label;
	GtkWidget *threshold_max_label;
	GtkWidget *port1_min_label;
	GtkWidget *port1_max_label;
	GtkWidget *port2_min_label;
	GtkWidget *port2_max_label;

	// line/slice scale ranges
	GtkWidget *line_scale_range;
	GtkWidget *line_scale_label;
	GtkWidget *slice_scale_range;
	GtkWidget *slice_scale_label;

	// boolean states for our check boxes
	GtkWidget *labels_check_box;
	GtkWidget *transparent_check_box;
	GtkWidget *lighting_check_box;
	gboolean draw_labels;
	gboolean transparent;
	gboolean disable_lighting;
	gboolean disable_max_threshold;

	// current client/server mode and hostname/port
	gboolean connected;
	struct in_addr server_addr;
	guint server_port;
	fmode_t mode;
	frange_t *mode_ranges;
	avail_feed_t *feeds;
	guint num_feeds;

	// alerts
	guint num_alerts;
	wlreq_t *alerts;

	// handlers for the current type of data being viewed
	handler_t handler;

	// giochannel vars for sockets
	guint in_id;
	guint hup_id;
	guint timeout_id;
	GIOChannel *channel;

	// pointer arrays for storing our as/port/proto descriptions
	GPtrArray *asnames;
	GPtrArray *port_numbers;
	GPtrArray *proto_numbers;

	// opengl lighting effects
	GLfloat light_ambient_3d[4];
	GLfloat light_diffuse_3d[4];
	GLfloat light_position_3d[4];
	GLfloat quads_normal_3d[72];

	// opengl font vars
	guint font_list_2d;
	guint font_height_2d;
	guint font_list_3d;
	guint font_height_3d;

	// playback vars and widgets
	gboolean playback_active;
	GtkWidget *playback_dialog;
	GtkWidget *playback_cancel_button;
	GtkWidget *playback_ok_button;
	GtkWidget *playback_feed_name;
	GtkWidget *playback_speed;
	GtkWidget *start_calendar;
	GtkWidget *start_hours;
	GtkWidget *start_minutes;
	GtkWidget *start_seconds;
	GtkWidget *end_calendar;
	GtkWidget *end_hours;
	GtkWidget *end_minutes;
	GtkWidget *end_seconds;

	// preferences keyfile
	GKeyFile *prefs;
	GtkWidget *preferences_dialog;
	GtkWidget *prefs_max_threshold_value;
	GtkWidget *prefs_default_host;
	GtkWidget *prefs_default_port;
	GtkWidget *prefs_default_password;
	GtkWidget *prefs_label_font;
	guint num_src_widgets;
	guint num_dst_widgets;
	GtkWidget *prefs_range_src_vbox;
	GtkWidget *prefs_range_dst_vbox;
	GQueue *prefs_src_range_widgets;
	GQueue *prefs_dst_range_widgets;
	GtkWidget *prefs_alert_vbox;
	GQueue *prefs_alert_widgets;
	guint num_alert_widgets;
} viz_t;

viz_t *viz;

gint gtree_compare(gconstpointer node_a_in, gconstpointer node_b_in, gpointer data);

#endif
