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
#include "glext2d.h"
#include "info_interface.h"

static GLfloat trans_x = 0.0;
static GLfloat trans_y = 0.0;
static GLfloat begin_x = 0.0;
static GLfloat begin_y = 0.0;
static GLfloat view_scale = ZOOM_START_2D;
static GLfloat x_pos_in_white_square = 0;
static GLfloat y_pos_in_white_square = 0;

extern viz_t *viz;


void glext2d_input_view_parameters(FILE *in)
{
	fscanf(in, "%f\n", &trans_x);
	fscanf(in, "%f\n", &trans_y);
	fscanf(in, "%f\n", &begin_x);
	fscanf(in, "%f\n", &begin_y);
	fscanf(in, "%f\n", &view_scale);
}


void glext2d_output_view_parameters(FILE *out)
{
	fprintf(out, "%f\n", trans_x);
	fprintf(out, "%f\n", trans_y);
	fprintf(out, "%f\n", begin_x);
	fprintf(out, "%f\n", begin_y);
	fprintf(out, "%f\n", view_scale);
}


void cb_reset_2d_view_clicked(GtkButton *button, gpointer user_data)
{
	trans_x = 0.0;
	trans_y = 0.0;
	begin_x = 0.0;
	begin_y = 0.0;
	view_scale = ZOOM_START_2D;

	gtk_widget_queue_draw(GTK_WIDGET(viz->glext2d));
}


void cb_glext2d_realize(GtkWidget *widget, gpointer user_data)
{
	PangoFont *font;
	PangoFontMetrics *font_metrics;
	PangoFontDescription *font_desc;

	GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

	if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext)) {
		return;
	}

	glShadeModel(GL_FLAT);
	glDisable(GL_DITHER);

	// generate display list for our square base
	glNewList(GREY_2D_SQUARE_CALL_LIST, GL_COMPILE);
		glBegin(GL_QUAD_STRIP);
		glColor3ub(100, 100, 100);
		glVertex2f(-1, -1);
		glVertex2f(-1, 1);
		glVertex2f(1, -1);
		glVertex2f(1, 1);
		glEnd();
	glEndList();

	glNewList(WHITE_2D_SQUARE_CALL_LIST, GL_COMPILE);
		glBegin(GL_QUAD_STRIP);
		glColor3ub(255, 255, 255);
		glVertex2f(-1, -1);
		glVertex2f(-1, 1);
		glVertex2f(1, -1);
		glVertex2f(1, 1);
		glEnd();
	glEndList();

	// generate display lists for our font
	const char *font_string = gtk_entry_get_text(GTK_ENTRY(viz->prefs_label_font));
	viz->font_list_2d = glGenLists(128);
	font_desc = pango_font_description_from_string(font_string);
	font = gdk_gl_font_use_pango_font(font_desc, 0, 128, viz->font_list_2d);
	if (font == NULL) {
		g_warning("cannot load font '%s', falling back to '%s'", font_string, DEFAULT_LABEL_FONT);

		font_desc = pango_font_description_from_string(DEFAULT_LABEL_FONT);
		font = gdk_gl_font_use_pango_font(font_desc, 0, 128, viz->font_list_3d);
	}

	// use pango to determine dimensions of font
	font_metrics = pango_font_get_metrics(font, NULL);
	viz->font_height_2d = pango_font_metrics_get_ascent(font_metrics) + pango_font_metrics_get_descent(font_metrics);
	viz->font_height_2d = PANGO_PIXELS(viz->font_height_2d);
	pango_font_description_free(font_desc);
	pango_font_metrics_unref(font_metrics);

	// define display lists for our as labels
	glNewList(LABELS_2D_AS_CALL_LIST, GL_COMPILE);
		// output our labels
		glColor3f(1.0, 1.0, 1.0);

		// 0 label
		glRasterPos2f(-1.0, -1.0);
		glListBase(viz->font_list_2d);
		glCallLists(strlen(" 0"), GL_UNSIGNED_BYTE, " 0");

		// 65535 label
		glRasterPos2f(1.0, 1.0);
		glListBase(viz->font_list_2d);
		glCallLists(strlen(" 65535"), GL_UNSIGNED_BYTE, " 65535");
	glEndList();

	// enable the use of glDrawArrays with vertices and normals
	glEnableClientState(GL_VERTEX_ARRAY);
	
	gdk_gl_drawable_gl_end(gldrawable);
}


gboolean cb_glext2d_configure_event(GtkWidget *widget, GdkEventConfigure *event, gpointer user_data)
{
	GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

	GLfloat w = widget->allocation.width;
	GLfloat h = widget->allocation.height;
	GLfloat aspect;

	if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext)) {
		return FALSE;
	}

	glViewport(0, 0, w, h);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	if (w > h) {
		aspect = w / h;
		glFrustum(-aspect, aspect, 1.0, -1.0, 5.0, 60.0);
	} else {
		aspect = h / w;
		glFrustum(-1.0, 1.0, aspect, -aspect, 5.0, 60.0);
	}
	
	glMatrixMode(GL_MODELVIEW);

	gdk_gl_drawable_gl_end(gldrawable);

	return FALSE;
}


gboolean cb_glext2d_expose_event(GtkWidget *widget, GdkEventExpose *event, gpointer user_data)
{
	GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

	if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext)) {
		return FALSE;
	}

	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	// view transformation
	glLoadIdentity();
	glTranslatef(trans_x, trans_y, -10.0);
	glScalef(view_scale, view_scale, view_scale);

	// render 2d scene
	viz->handler.render_2d_scene();

	if (gdk_gl_drawable_is_double_buffered(gldrawable)) {
		gdk_gl_drawable_swap_buffers(gldrawable);
	} else {
		glFlush();
	}

	gdk_gl_drawable_gl_end(gldrawable);

	return TRUE;
}


static inline gboolean gtree_foreach_check_match(gpointer node_in, gpointer null, gpointer iter_in)
{
	tree_node_t *node = node_in;

	// only check the node if it is between the thresholds
	if (viz->handler.check_visibility(node)) {
		// check if the click was within the nodes coords
		if (viz->handler.check_2d_click(node, x_pos_in_white_square, y_pos_in_white_square)) {
			// if a match is found, add it to the watch list
			info_watch_list_insert(node);
		}
	}

	return FALSE;
}


gboolean cb_glext2d_button_press_event(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	begin_x = event->x;
	begin_y = event->y;

	// left clicks will display info about the area clicked
	if (event->button == 1 && event->type == GDK_BUTTON_PRESS) {
		float x_pos_in_drawing_area = 0;
		float y_pos_in_drawing_area = 0;
	
		float height_width_ratio = (float) MAX(widget->allocation.width, widget->allocation.height)/MIN(widget->allocation.width, widget->allocation.height);
		float zoom_ratio = (float) view_scale / ZOOM_START_2D;
		float cube_size = .875 * zoom_ratio;
		float cube_size_scaled = (float) cube_size / height_width_ratio;
		float sidebar_size = (float) (1.0 - cube_size) / 2.0;
		float sidebar_size_scaled = (float) sidebar_size / height_width_ratio;
		
		float x_amount_moved = (float) trans_x / 4.0;
		float y_amount_moved = (float) trans_y / 4.0;
		float x_amount_moved_scaled = (float) x_amount_moved / height_width_ratio;
		float y_amount_moved_scaled = (float) y_amount_moved / height_width_ratio;

		float diff_in_width_height = (float) MAX(widget->allocation.width, widget->allocation.height) - (float) MIN(widget->allocation.width, widget->allocation.height);
		float extra_pixels_on_side = (float) diff_in_width_height / 2.0;
		float extra_pixels_ratio = (float) extra_pixels_on_side/MAX(widget->allocation.width, widget->allocation.height);

		if (widget->allocation.width >= widget->allocation.height) {
			// this line will make the range 0->1 always, no matter the width/height of the window
			x_pos_in_drawing_area = (float) (event->x) / ((float) widget->allocation.width - diff_in_width_height) / height_width_ratio;
			x_pos_in_white_square = (float) ((float) x_pos_in_drawing_area - (float) sidebar_size_scaled - (float) extra_pixels_ratio - (float) x_amount_moved_scaled) / (cube_size_scaled);
			
			y_pos_in_drawing_area = (float) (event->y) / ((float) widget->allocation.height);
			y_pos_in_white_square = (float) ((float) y_pos_in_drawing_area - (float) sidebar_size - (float) y_amount_moved) / (cube_size);
		} else {
			// this line will make the range 0->1 always, no matter the width/height of the window
			x_pos_in_drawing_area = (float) (event->x) / ((float) widget->allocation.width);
			x_pos_in_white_square = (float) ((float) x_pos_in_drawing_area - (float) sidebar_size - (float) x_amount_moved) / (cube_size);
			
			y_pos_in_drawing_area = (float) (event->y) / ((float) widget->allocation.height - diff_in_width_height) / height_width_ratio;
			y_pos_in_white_square = (float) ((float) y_pos_in_drawing_area - (float) sidebar_size_scaled - (float) extra_pixels_ratio - (float) y_amount_moved_scaled) / (cube_size_scaled);
		}

		// translate from 0->1 coordinates to -1->1
		x_pos_in_white_square = x_pos_in_white_square * 2 - 1;
		y_pos_in_white_square = y_pos_in_white_square * 2 - 1;

		// loop through nodes and look for a match
		GtkTreeIter iter;
		g_tree_foreach(viz->nodes, gtree_foreach_check_match, &iter);
	}

	return FALSE;
}


gboolean cb_glext2d_motion_notify_event(GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
	GLfloat h = widget->allocation.height;
	GLfloat x = event->x;
	GLfloat y = event->y;

	// pan
	if (event->state & GDK_BUTTON3_MASK) {
		trans_x -= (begin_x - x) / 50.0f;
		trans_y -= (begin_y - y) / 50.0f;

		gtk_widget_queue_draw(GTK_WIDGET(viz->glext2d));
	}

	// zoom
	if (event->state & GDK_BUTTON2_MASK) {
		view_scale = view_scale * (1.0 + (y - begin_y) / h);

		if (view_scale > ZOOM_MAX_2D) {
			view_scale = ZOOM_MAX_2D;
		} else if (view_scale < ZOOM_MIN_2D) {
			view_scale = ZOOM_MIN_2D;
		}

		gtk_widget_queue_draw(GTK_WIDGET(viz->glext2d));
	}

	begin_x = x;
	begin_y = y;

	return TRUE;
}
