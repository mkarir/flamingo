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
#include "glext3d.h"
#include "trackball.h"

#define FLAMINGO_ID	1
#define FLAMINGO_LOGO	"logo.tga"

// pan/rotate/zoom transformation vars
static GLfloat trans_x = 0.0;
static GLfloat trans_y = 0.0;
static GLfloat begin_x = 0.0;
static GLfloat begin_y = 0.0;
static GLfloat view_quat_diff[4] = { 0.0, 0.0, 0.0, 1.0 };
static GLfloat view_quat[4] = { 0.0, 0.0, 0.0, 1.0 };
static GLfloat view_scale = ZOOM_START_3D;

extern viz_t *viz;


void glext3d_input_view_parameters(FILE *in)
{
	fscanf(in, "%f\n", &trans_x);
	fscanf(in, "%f\n", &trans_y);
	fscanf(in, "%f\n", &begin_x);
	fscanf(in, "%f\n", &begin_y);
	fscanf(in, "%f\n", &view_quat_diff[0]);
	fscanf(in, "%f\n", &view_quat_diff[1]);
	fscanf(in, "%f\n", &view_quat_diff[2]);
	fscanf(in, "%f\n", &view_quat_diff[3]);
	fscanf(in, "%f\n", &view_quat[0]);
	fscanf(in, "%f\n", &view_quat[1]);
	fscanf(in, "%f\n", &view_quat[2]);
	fscanf(in, "%f\n", &view_quat[3]);
	fscanf(in, "%f\n", &view_scale);
}


void glext3d_output_view_parameters(FILE *out)
{
	fprintf(out, "%f\n", trans_x);
	fprintf(out, "%f\n", trans_y);
	fprintf(out, "%f\n", begin_x);
	fprintf(out, "%f\n", begin_y);
	fprintf(out, "%f\n", view_quat_diff[0]);
	fprintf(out, "%f\n", view_quat_diff[1]);
	fprintf(out, "%f\n", view_quat_diff[2]);
	fprintf(out, "%f\n", view_quat_diff[3]);
	fprintf(out, "%f\n", view_quat[0]);
	fprintf(out, "%f\n", view_quat[1]);
	fprintf(out, "%f\n", view_quat[2]);
	fprintf(out, "%f\n", view_quat[3]);
	fprintf(out, "%f\n", view_scale);
}


void glext3d_load_logo()
{
	FILE *s = NULL;
	guchar *rgba, temp, type[4], info[7];
	guint bread, i, imageWidth, imageHeight, imageBits, size;

	// look for the logo data file
	if (g_file_test(PRIMARY_PATH FLAMINGO_LOGO, G_FILE_TEST_EXISTS)) {
		s = fopen(PRIMARY_PATH "logo.tga", "r+bt");
	} else if (g_file_test(BACKUP_PATH FLAMINGO_LOGO, G_FILE_TEST_EXISTS)) {
		s = fopen(BACKUP_PATH "logo.tga", "r+bt");
	} else {
		g_error("Could not find the flamingo logo file");
	}

	fread(&type, sizeof(gchar), 3, s);
	fseek(s, 12, SEEK_SET);
	fread(&info, sizeof(gchar), 6, s);

	imageWidth = info[0] + info[1] * 256;
	imageHeight = info[2] + info[3] * 256;
	imageBits = info[4];

	size = imageWidth * imageHeight;

	rgba = g_new0(guchar, size * 4);

	bread = fread(rgba, sizeof(guchar), size * 4, s);

	if (bread != size * 4) {
		fclose(s);
		g_free(rgba);
		return;
	}

	for (i = 0; i < size * 4; i += 4) {
		temp = rgba[i];
		rgba[i] = rgba[i + 2];
		rgba[i + 2] = temp;
	}

	glBindTexture(GL_TEXTURE_2D, FLAMINGO_ID);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imageWidth, imageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);

	fclose(s);
	g_free(rgba);
}


void cb_enable_transparency_check_box_toggled(GtkToggleButton *toggle_button, gpointer user_data)
{
	GdkGLContext *glcontext = gtk_widget_get_gl_context(viz->glext3d);
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(viz->glext3d);

	if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext)) {
		return;
	}

	if (gtk_toggle_button_get_active(toggle_button)) {
		viz->transparent = TRUE;

		// set opengl parameters for transparency
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_DEPTH_TEST);
	} else {
		viz->transparent = FALSE;

		// disable transparency
		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
	}

	gdk_gl_drawable_gl_end(gldrawable);

	// redraw
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext2d));
	gtk_widget_queue_draw(GTK_WIDGET(viz->glext3d));
}


void cb_reset_3d_view_clicked(GtkButton *button, gpointer user_data)
{
	trans_x = 0.0;
	trans_y = 0.0;
	begin_x = 0.0;
	begin_y = 0.0;
	view_scale = ZOOM_START_3D;
	view_quat_diff[0] = 0.0;
	view_quat_diff[1] = 0.0;
	view_quat_diff[2] = 0.0;
	view_quat_diff[3] = 1.0;
	view_quat[0] = 0.0;
	view_quat[1] = 0.0;
	view_quat[2] = 0.0;
	view_quat[3] = 1.0;

	gtk_widget_queue_draw(GTK_WIDGET(viz->glext3d));
}


void glext3d_realize_init()
{
	PangoFont *font;
	PangoFontMetrics *font_metrics;
	PangoFontDescription *font_desc;

	glClearDepth(1.0);
	glDepthFunc(GL_LESS);

	glShadeModel(GL_FLAT);
	glEnable(GL_LIGHTING);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_NORMALIZE);
	glDisable(GL_DITHER);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	glHint(GL_LINE_SMOOTH_HINT, GL_FASTEST);

	// turn on the light!!!
	glLightfv(GL_LIGHT0, GL_POSITION, viz->light_position_3d);
	glLightfv(GL_LIGHT0, GL_AMBIENT, viz->light_ambient_3d);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, viz->light_diffuse_3d);
	glEnable(GL_LIGHT0);

	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);

	// load our flamingo logo
	glext3d_load_logo();
	
	glNewList(FLAMINGO_LOGO_CALL_LIST, GL_COMPILE);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, FLAMINGO_ID);

		glBegin(GL_QUADS);
		glColor3ub(255, 255, 255);
		glNormal3f(0.0, 0.0, -1.0);
		glTexCoord2f(0.0f, 0.0f);
		glVertex3f(-1.0f, -1.0f, -1.0f - Z_HEIGHT_ADJ);
		glTexCoord2f(1.0f, 0.0f);
		glVertex3f(1.0f, -1.0f, -1.0f - Z_HEIGHT_ADJ);
		glTexCoord2f(1.0f, 1.0f);
		glVertex3f(1.0f, 1.0f, -1.0f - Z_HEIGHT_ADJ);
		glTexCoord2f(0.0f, 1.0f);
		glVertex3f(-1.0f, 1.0f, -1.0f - Z_HEIGHT_ADJ);
		glEnd();

		glDisable(GL_TEXTURE_2D);
	glEndList();

	// define display lists for our static elements
	glNewList(GREY_3D_SQUARE_CALL_LIST, GL_COMPILE);
		glBegin(GL_QUAD_STRIP);
		glNormal3f(0.0, 0.0, 1.0);
		glColor3ub(100, 100, 100);
		glVertex3f(-1, -1, -1);
		glVertex3f(-1, 1, -1);
		glVertex3f(1, -1, -1);
		glVertex3f(1, 1, -1);
		glEnd();
	glEndList();

	glNewList(GREY_3D_TWO_SIDED_CALL_LIST, GL_COMPILE);
		glBegin(GL_QUAD_STRIP);
		glNormal3f(1.0, 0.0, 0.0);
		glColor4ub(100, 100, 100, 180);
		glVertex3f(-1, -1, -1);
		glVertex3f(-1, 1, -1);
		glVertex3f(-1, -1, 1);
		glVertex3f(-1, 1, 1);
		glEnd();

		glBegin(GL_QUAD_STRIP);
		glNormal3f(-1.0, 0.0, 0.0);
		glColor4ub(100, 100, 100, 180);
		glVertex3f(1, -1, -1);
		glVertex3f(1, 1, -1);
		glVertex3f(1, -1, 1);
		glVertex3f(1, 1, 1);
		glEnd();
	glEndList();

	glNewList(WHITE_3D_SQUARE_CALL_LIST, GL_COMPILE);
		glBegin(GL_QUAD_STRIP);
		glNormal3f(0.0, 0.0, 1.0);
		glColor3ub(255, 255, 255);
		glVertex3f(-1, -1, -1);
		glVertex3f(-1, 1, -1);
		glVertex3f(1, -1, -1);
		glVertex3f(1, 1, -1);
		glEnd();
	glEndList();

	glNewList(WHITE_3D_TWO_SIDED_CALL_LIST, GL_COMPILE);
		glBegin(GL_QUAD_STRIP);
		glNormal3f(1.0, 0.0, 0.0);
		glColor4ub(255, 255, 255, 180);
		glVertex3f(-1, -1, -1);
		glVertex3f(-1, 1, -1);
		glVertex3f(-1, -1, 1);
		glVertex3f(-1, 1, 1);
		glEnd();

		glBegin(GL_QUAD_STRIP);
		glNormal3f(-1.0, 0.0, 0.0);
		glColor4ub(100, 100, 100, 180);
		glVertex3f(1, -1, -1);
		glVertex3f(1, 1, -1);
		glVertex3f(1, -1, 1);
		glVertex3f(1, 1, 1);
		glEnd();
	glEndList();

	glNewList(WIRE_CUBE_CALL_LIST, GL_COMPILE);
		glDisable(GL_LIGHTING);

		glLineWidth(1.0);
		glColor3ub(255, 255, 255);
		gdk_gl_draw_cube(FALSE, 2);

		glEnable(GL_LIGHTING);
	glEndList();


	// generate display lists for our font
	const char *font_string = gtk_entry_get_text(GTK_ENTRY(viz->prefs_label_font));
	viz->font_list_3d = glGenLists(128);
	font_desc = pango_font_description_from_string(font_string);
	font = gdk_gl_font_use_pango_font(font_desc, 0, 128, viz->font_list_3d);
	if (font == NULL) {
		g_warning("cannot load font '%s', falling back to '%s'", font_string, DEFAULT_LABEL_FONT);

		font_desc = pango_font_description_from_string(DEFAULT_LABEL_FONT);
		font = gdk_gl_font_use_pango_font(font_desc, 0, 128, viz->font_list_3d);
	}

	// use pango to determine dimensions of font
	font_metrics = pango_font_get_metrics(font, NULL);
	viz->font_height_3d = pango_font_metrics_get_ascent(font_metrics) + pango_font_metrics_get_descent(font_metrics);
	viz->font_height_3d = PANGO_PIXELS(viz->font_height_3d);
	pango_font_description_free(font_desc);
	pango_font_metrics_unref(font_metrics);

	// define display lists for our as labels
	glNewList(LABELS_3D_AS_CALL_LIST, GL_COMPILE);
		glDisable(GL_LIGHTING);

		// output our labels
		glColor3f(1.0, 1.0, 1.0);

		// 0 label
		glRasterPos3f(-1.0, -1.0, -0.9);
		glListBase(viz->font_list_3d);
		glCallLists(strlen(" 0"), GL_UNSIGNED_BYTE, " 0");

		// 65535 label
		glRasterPos3f(1.0, 1.0, -0.9);
		glListBase(viz->font_list_3d);
		glCallLists(strlen(" 65535"), GL_UNSIGNED_BYTE, " 65535");

		glEnable(GL_LIGHTING);
	glEndList();

	// define display lists for our src_dst_as labels
	glNewList(LABELS_3D_SRC_DEST_AS_CALL_LIST, GL_COMPILE);
		glDisable(GL_LIGHTING);

		// output our labels
		glColor3f(1.0, 1.0, 1.0);

		// 0.0.0.0 label
		glRasterPos3f(-1.0, -1.0, -1.0);
		glListBase(viz->font_list_3d);
		glCallLists(strlen(" SRC 0"), GL_UNSIGNED_BYTE, " SRC 0");

		// 255.255.255.255 label
		glRasterPos3f(-1.0, 1.0, 1.0);
		glListBase(viz->font_list_3d);
		glCallLists(strlen(" SRC 65535"), GL_UNSIGNED_BYTE, " SRC 65535");

		// 0.0.0.0 label
		glRasterPos3f(1.0, -1.0, -1.0);
		glListBase(viz->font_list_3d);
		glCallLists(strlen(" DEST 0"), GL_UNSIGNED_BYTE, " DEST 0");

		// 255.255.255.255 label
		glRasterPos3f(1.0, 1.0, 1.0);
		glListBase(viz->font_list_3d);
		glCallLists(strlen(" DEST 65535"), GL_UNSIGNED_BYTE, " DEST 65535");

		glEnable(GL_LIGHTING);
	glEndList();

	// enable the use of glDrawArrays with vertices and normals
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
}


void cb_glext3d_realize(GtkWidget *widget, gpointer user_data)
{
	GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

	if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext)) {
		return;
	}

	glext3d_realize_init();

	gdk_gl_drawable_gl_end(gldrawable);
}


void glext3d_configure_init(GLfloat w, GLfloat h)
{
	GLfloat aspect;

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
}


gboolean cb_glext3d_configure_event(GtkWidget *widget, GdkEventConfigure *event, gpointer user_data)
{
	GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

	GLfloat w = widget->allocation.width;
	GLfloat h = widget->allocation.height;

	if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext)) {
		return FALSE;
	}

	glext3d_configure_init(w, h);

	gdk_gl_drawable_gl_end(gldrawable);

	return FALSE;
}


void glext3d_render_export(guint w, guint h)
{
	// set the correct context vars for our 3d export
	glext3d_realize_init();
	glext3d_configure_init(w, h);

	if (viz->transparent) {
		// set opengl parameters for transparency
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_DEPTH_TEST);
	} else {
		// disable transparency
		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
	}

	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	// view transformation to match the glext3d widget
	GLfloat m[4][4];
	glLoadIdentity();
	glTranslatef(trans_x, trans_y, -10.0);
	add_quats(view_quat_diff, view_quat, view_quat);
	build_rotmatrix(m, view_quat);
	glMultMatrixf(&m[0][0]);
	glScalef(view_scale, view_scale, view_scale);

	// render the nodes into our off-screen context
	viz->handler.render_3d_scene();

	// blocks until the hardware has cleared out its command queue
	glFinish();
}


gboolean cb_glext3d_expose_event(GtkWidget *widget, GdkEventExpose *event, gpointer user_data)
{
	GLfloat m[4][4];
	GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

	if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext)) {
		return FALSE;
	}

	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	// view transformation
	glLoadIdentity();
	glTranslatef(trans_x, trans_y, -10.0);
	add_quats(view_quat_diff, view_quat, view_quat);
	build_rotmatrix(m, view_quat);
	glMultMatrixf(&m[0][0]);
	glScalef(view_scale, view_scale, view_scale);

	// render 3d scene
	viz->handler.render_3d_scene();

	if (gdk_gl_drawable_is_double_buffered(gldrawable)) {
		gdk_gl_drawable_swap_buffers(gldrawable);
	} else {
		glFlush();
	}

	gdk_gl_drawable_gl_end(gldrawable);

	return TRUE;
}


gboolean cb_glext3d_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	// need to reset these on release to prevent continuing rotation when redrawing
	view_quat_diff[0] = 0.0;
	view_quat_diff[1] = 0.0;
	view_quat_diff[2] = 0.0;
	view_quat_diff[3] = 1.0;

	begin_x = event->x;
	begin_y = event->y;

	return FALSE;
}

gboolean cb_glext3d_button_press_event(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	// need to reset these on press to prevent continuing rotation when redrawing
	view_quat_diff[0] = 0.0;
	view_quat_diff[1] = 0.0;
	view_quat_diff[2] = 0.0;
	view_quat_diff[3] = 1.0;

	begin_x = event->x;
	begin_y = event->y;

	return FALSE;
}


gboolean cb_glext3d_motion_notify_event(GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
	GLfloat w = widget->allocation.width;
	GLfloat h = widget->allocation.height;
	GLfloat x = event->x;
	GLfloat y = event->y;

	// rotation
	if (event->state & GDK_BUTTON1_MASK) {
		trackball(view_quat_diff,
				(2.0 * begin_x - w) / w,
				(2.0 * begin_y - h) / h,
				(2.0 * x - w) / w,
				(2.0 * y - h) / h);

		gtk_widget_queue_draw(GTK_WIDGET(viz->glext3d));
	}

	// pan
	if (event->state & GDK_BUTTON3_MASK) {
		trans_x -= (begin_x - x) / 50.0f;
		trans_y -= (begin_y - y) / 50.0f;

		gtk_widget_queue_draw(GTK_WIDGET(viz->glext3d));
	}
	
	// zoom
	if (event->state & GDK_BUTTON2_MASK) {
		view_scale = view_scale * (1.0 + (y - begin_y) / h);
		
		if (view_scale > ZOOM_MAX_3D) {
			view_scale = ZOOM_MAX_3D;
		} else if (view_scale < ZOOM_MIN_3D) {
			view_scale = ZOOM_MIN_3D;
		}

		gtk_widget_queue_draw(GTK_WIDGET(viz->glext3d));
	}

	begin_x = x;
	begin_y = y;

	return TRUE;
}
