#ifndef __MAIN_INTERFACE_H__
#define __MAIN_INTERFACE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <config.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <glade/glade.h>

void main_init(void);
void main_update_feed_menu(void);
void main_update_mode(void);
void main_primary_statusbar_update(gchar *status_text);
void main_secondary_statusbar_update(gchar *status_text);

#endif
