#ifndef __INFO_INTERFACE_H__
#define __INFO_INTERFACE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <config.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <glade/glade.h>

void info_init();

void info_watch_list_insert(tree_node_t *node);
void info_full_list_insert(tree_node_t *node);
void info_alert_list_insert(wlalert_t *alert);

void info_full_list_update(void);
void info_watch_list_update(void);

gint info_list_sort_address(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data);
gint info_list_sort_freq(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data);

#endif
