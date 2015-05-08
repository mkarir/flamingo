#ifndef __ALERTS_H__
#define __ALERTS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <config.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <glade/glade.h>
#include <GL/gl.h>

void alert_text_changed(GtkEditable *editable, gpointer user_data);
gboolean alerts_add_all_from_widgets();
void alerts_send_update();

#endif
