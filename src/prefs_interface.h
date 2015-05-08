#ifndef __PREFS_INTERFACE_H__
#define __PREFS_INTERFACE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <config.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <glade/glade.h>

void prefs_init(void);
gboolean prefs_write(void);

typedef struct {
        GtkWidget *hbox;
        GtkWidget *checkbox;
        GtkWidget *alert;
        GtkWidget *trigger_val;
        GtkWidget *trigger_type;
        GtkWidget *ident;
} alert_widget_t;

typedef struct {
        GtkWidget *hbox;
        GtkWidget *checkbox;
        GtkWidget *ip;
        GtkWidget *bits;
        GtkWidget *ident;
} range_widget_t;

#endif
