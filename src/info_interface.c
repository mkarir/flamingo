#include <stdio.h>
#include <time.h>
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
#include "info_interface.h"
#include "controls_interface.h"


void info_list_pre_insert(GtkWidget *list, gint *sort_column_id, GtkSortType *order)
{
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(list));
	GtkTreeSortable *sortable = GTK_TREE_SORTABLE(model);

	// get our current sorting column/method
	gtk_tree_sortable_get_sort_column_id(sortable, sort_column_id, order);

	// turn off sorting or else the list will be re-sorted after every insert
	gtk_tree_sortable_set_sort_column_id(sortable, GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID, GTK_SORT_ASCENDING);

	// increment the ref count, then detach the tree from the model
	g_object_ref(model);
	gtk_tree_view_set_model(GTK_TREE_VIEW(list), NULL);
}


void info_list_post_insert(GtkWidget *list, GtkListStore *model, gint *sort_column_id, GtkSortType *order)
{
	GtkTreeSortable *sortable = GTK_TREE_SORTABLE(model);

	// reattach the tree to the model, then decrement the ref count
	gtk_tree_view_set_model(GTK_TREE_VIEW(list), GTK_TREE_MODEL(model));
	g_object_unref(model);

	// restore the sorting state
	gtk_tree_sortable_set_sort_column_id(sortable, *sort_column_id, *order);
}


gint info_list_sort_address(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
	gint ret;
	gchar *node_str_a, *node_str_b;
	tree_node_t *node_a, *node_b;
	
	gtk_tree_model_get(model, a, LIST_ADDRESS_COLUMN, &node_str_a, -1);
	gtk_tree_model_get(model, b, LIST_ADDRESS_COLUMN, &node_str_b, -1);

	// retrieve the node associated with the node string
	node_a = viz->handler.lookup_node_from_string(node_str_a);
	node_b = viz->handler.lookup_node_from_string(node_str_b);

	// use our existing binary tree comparison function to compare our nodes
	ret = viz->handler.compare_nodes(node_a, node_b);

	g_free(node_a);
	g_free(node_b);
	g_free(node_str_a);
	g_free(node_str_b);

	return ret;
}


gint info_list_sort_freq(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
	guint freq_a, freq_b;

	gtk_tree_model_get(model, a, LIST_VOLUME_COLUMN, &freq_a, -1);
	gtk_tree_model_get(model, b, LIST_VOLUME_COLUMN, &freq_b, -1);

	// sort the volume column by integer comparison
	if (freq_a < freq_b) {
		return -1;
	} else if (freq_a > freq_b) {
		return 1;
	} else {
		return 0;
	}
}


void info_alert_list_insert(wlalert_t *alert)
{
	guint len;
	GtkTreeIter iter;

	alert->filter_id = g_ntohl(alert->filter_id);
	alert->time = g_ntohl(alert->time);
	alert->condition = g_ntohl(alert->condition);
	alert->network_condition = g_ntohl(alert->network_condition);

	g_debug("ALERT: %d %u %d %d", alert->filter_id, alert->time, alert->condition, alert->network_condition);

	alert_widget_t *alert_widget = g_queue_peek_nth(viz->prefs_alert_widgets, alert->filter_id);

	// make sure that the alert is still valid
	if (!alert_widget) {
		return;
	}

	gchar *timestamp = g_strdup_printf("%s", ctime((time_t *) &alert->time));
	gchar *string = g_strdup(gtk_entry_get_text(GTK_ENTRY(alert_widget->alert)));

	// strip the '\n' off the end of the timestamp
	timestamp = g_strstrip(timestamp);
	len = strlen(timestamp);
	if (timestamp[len] == '\n') {
		timestamp[len-1] = '\0';
	}

	gtk_list_store_append(viz->alert_store, &iter);
	gtk_list_store_set(viz->alert_store, &iter,
			ALERT_TIME_COLUMN, timestamp,
			ALERT_STRING_COLUMN, string,
			ALERT_VALUE_COLUMN, alert->network_condition,
			-1);

	g_free(string);
	g_free(timestamp);
}


void cb_remove_selected_alert_button_clicked(GtkWidget *button, gpointer *data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *select;

	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(viz->alert_list));

	if (gtk_tree_selection_get_selected(select, &model, &iter)) {
		// remove the selected alert
		gtk_list_store_remove(GTK_LIST_STORE(viz->alert_store), &iter);
	}
}


void cb_clear_alert_list_button_clicked(GtkWidget *button, gpointer *data)
{
	gtk_list_store_clear(viz->alert_store);
}


void cb_info_remove_selected_button_clicked(GtkWidget *button, gpointer *data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *select;

	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(viz->watch_list));

	if (gtk_tree_selection_get_selected(select, &model, &iter)) {
		// remove the selected node
		gtk_list_store_remove(GTK_LIST_STORE(viz->watch_store), &iter);
	}
}


void cb_info_clear_watch_list_clicked(GtkWidget *button, gpointer *data)
{
	gtk_list_store_clear(viz->watch_store);
}


void info_full_list_insert(tree_node_t *node)
{
	GtkTreeIter iter;

	coord_t *coords = viz->handler.get_node_coords(node);
	gchar *address = viz->handler.get_node_value(node);
	gchar *desc = viz->handler.get_node_description(node);
	gchar *color_str = g_strdup_printf("#%02x%02x%02x", coords->color[0], coords->color[1], coords->color[2]);

	// insert node into full list
	gtk_list_store_append(viz->full_store, &iter);
	gtk_list_store_set(viz->full_store, &iter,
			LIST_DISPLAY_COLOR_COLUMN, " ",
			LIST_VOLUME_COLUMN, node->volume,
			LIST_ADDRESS_COLUMN, address,
			LIST_DESC_COLUMN, desc,
			LIST_PRIVATE_COLOR_COLUMN, color_str,
			-1);

	g_free(address);
	g_free(desc);
	g_free(color_str);
}


void info_watch_list_insert(tree_node_t *node)
{
	GtkTreeIter iter;

	coord_t *coords = viz->handler.get_node_coords(node);
	gchar *address = viz->handler.get_node_value(node);
	gchar *desc = viz->handler.get_node_description(node);
	gchar *color_str = g_strdup_printf("#%02x%02x%02x", coords->color[0], coords->color[1], coords->color[2]);

	// only add the node if it is between the thresholds
	if (viz->handler.check_visibility(node)) {
		// insert node into watch list
		gtk_list_store_append(viz->watch_store, &iter);
		gtk_list_store_set(viz->watch_store, &iter,
				LIST_DISPLAY_COLOR_COLUMN, " ",
				LIST_VOLUME_COLUMN, node->volume,
				LIST_ADDRESS_COLUMN, address,
				LIST_DESC_COLUMN, desc,
				LIST_PRIVATE_COLOR_COLUMN, color_str,
				-1);
	}

	g_free(address);
	g_free(desc);
	g_free(color_str);
}


static inline gboolean gtree_foreach_info_watch_list_add(gpointer node_in, gpointer null, gpointer iter_in)
{
	tree_node_t *node = node_in;

	info_watch_list_insert(node);

	return FALSE;
}


void cb_info_add_all_visible_clicked(GtkWidget *button, gpointer *data)
{
	GtkTreeIter iter;
	GtkSortType order;
	gint sort_column_id;

	// disable sorting and disconnect the tree model from the view for a huge performance increase
	info_list_pre_insert(viz->watch_list, &sort_column_id, &order);

	// add all nodes visible at the current threshold to the watch list
	g_tree_foreach(viz->nodes, gtree_foreach_info_watch_list_add, &iter);

	// restore sorting and reconnect the tree model to the view
	info_list_post_insert(viz->watch_list, viz->watch_store, &sort_column_id, &order);
}


void cb_info_add_selected_to_watch_list_clicked(GtkWidget *button, gpointer *data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *select;
	tree_node_t *lookup;
	tree_node_t *node;
	gchar *node_str;
	gpointer tmp_node = NULL;

	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(viz->full_list));

	if (gtk_tree_selection_get_selected(select, &model, &iter)) {
		// get the string value of our node from the full list
		gtk_tree_model_get(model, &iter, LIST_ADDRESS_COLUMN, &node_str, -1);

		// retrieve our node given the node's string value
		lookup = viz->handler.lookup_node_from_string(node_str);

		// lookup and check if the address is in the current tree
		if (g_tree_lookup_extended(viz->nodes, lookup, &tmp_node, NULL)) {
			node = (tree_node_t *) tmp_node;

			// add the node to the watch list
			info_watch_list_insert(node);
		}

		g_free(node_str);
		g_free(lookup);
	}
}


void info_watch_list_update(void)
{
	// update our node "watch list" with the latest values
	gchar *node_str;
	gboolean valid;
	GtkTreeIter iter;
	tree_node_t *node;
	tree_node_t *lookup;
	gpointer tmp_node = NULL;

	// get first iter
	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(viz->watch_store), &iter);

	// loop through all rows of the list box
	while (valid) {
		// grab the address from the list box
		gtk_tree_model_get(GTK_TREE_MODEL(viz->watch_store), &iter, LIST_ADDRESS_COLUMN, &node_str, -1);

		// get our lookup node
		lookup = viz->handler.lookup_node_from_string(node_str);

		// lookup and check if the address is in the current tree
		if (g_tree_lookup_extended(viz->nodes, lookup, &tmp_node, NULL)) {
			node = (tree_node_t *) tmp_node;

			// if so, update its volume value
			gtk_list_store_set(GTK_LIST_STORE(viz->watch_store), &iter, LIST_VOLUME_COLUMN, node->volume, -1);
		} else {
			// if not, leave it in the watch list but set its freq to 0
			gtk_list_store_set(GTK_LIST_STORE(viz->watch_store), &iter, LIST_VOLUME_COLUMN, 0, -1);
		}

		// get the iter for the next row
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(viz->watch_store), &iter);
		g_free(node_str);
		g_free(lookup);
	}
}


static inline gboolean gtree_foreach_info_full_list_add(gpointer node_in, gpointer null, gpointer iter_in)
{
	tree_node_t *node = node_in;

	info_full_list_insert(node);

	return FALSE;
}


void info_full_list_update(void)
{
	GtkTreeIter iter;
	GtkSortType order;
	gint sort_column_id;

	// clear out the list
	gtk_list_store_clear(viz->full_store);

	// disable sorting and disconnect the tree model from the view for a huge performance increase
	info_list_pre_insert(viz->full_list, &sort_column_id, &order);

	// populate the list with the latest node entries
	g_tree_foreach(viz->nodes, gtree_foreach_info_full_list_add, &iter);

	// restore sorting and reconnect the tree model to the view
	info_list_post_insert(viz->full_list, viz->full_store, &sort_column_id, &order);
}


void info_init()
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeSortable *full_sortable;
	GtkTreeSortable *watch_sortable;

	// get our info window
	viz->info_window = glade_xml_get_widget(viz->xml, "info_window");

	// setup full list information box
	viz->full_list = glade_xml_get_widget(viz->xml, "full_list");
	viz->full_store = gtk_list_store_new(LIST_N_COLUMNS, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model(GTK_TREE_VIEW(viz->full_list), GTK_TREE_MODEL(viz->full_store));
	g_object_unref(G_OBJECT(viz->full_store));

	// add columns headers to full list
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Color", renderer, "text", LIST_DISPLAY_COLOR_COLUMN, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 40);
	gtk_tree_view_column_add_attribute(column, renderer, "background", LIST_PRIVATE_COLOR_COLUMN);
	gtk_tree_view_append_column(GTK_TREE_VIEW(viz->full_list), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Volume", renderer, "text", LIST_VOLUME_COLUMN, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 90);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, LIST_VOLUME_COLUMN);
	gtk_tree_view_append_column(GTK_TREE_VIEW(viz->full_list), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Address", renderer, "text", LIST_ADDRESS_COLUMN, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 90);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, LIST_ADDRESS_COLUMN);
	gtk_tree_view_append_column(GTK_TREE_VIEW(viz->full_list), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Description", renderer, "text", LIST_DESC_COLUMN, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(viz->full_list), column);

	// enable the full list to be sorted
	full_sortable = GTK_TREE_SORTABLE(viz->full_store);
	gtk_tree_sortable_set_sort_func(full_sortable, LIST_VOLUME_COLUMN, info_list_sort_freq, NULL, NULL);
	gtk_tree_sortable_set_sort_func(full_sortable, LIST_ADDRESS_COLUMN, info_list_sort_address, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id(full_sortable, LIST_VOLUME_COLUMN, GTK_SORT_DESCENDING);


	// setup node watch list box
	viz->watch_list = glade_xml_get_widget(viz->xml, "watch_list");
	viz->watch_store = gtk_list_store_new(LIST_N_COLUMNS, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model(GTK_TREE_VIEW(viz->watch_list), GTK_TREE_MODEL(viz->watch_store));
	g_object_unref(G_OBJECT(viz->watch_store));

	// add columns headers to watch list
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Color", renderer, "text", LIST_DISPLAY_COLOR_COLUMN, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 40);
	gtk_tree_view_column_add_attribute(column, renderer, "background", LIST_PRIVATE_COLOR_COLUMN);
	gtk_tree_view_append_column(GTK_TREE_VIEW(viz->watch_list), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Volume", renderer, "text", LIST_VOLUME_COLUMN, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 90);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, LIST_VOLUME_COLUMN);
	gtk_tree_view_append_column(GTK_TREE_VIEW(viz->watch_list), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Address", renderer, "text", LIST_ADDRESS_COLUMN, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 90);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column, LIST_ADDRESS_COLUMN);
	gtk_tree_view_append_column(GTK_TREE_VIEW(viz->watch_list), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Description", renderer, "text", LIST_DESC_COLUMN, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(viz->watch_list), column);

	// enable the watch list to be sorted
	watch_sortable = GTK_TREE_SORTABLE(viz->watch_store);
	gtk_tree_sortable_set_sort_func(watch_sortable, LIST_VOLUME_COLUMN, info_list_sort_freq, NULL, NULL);
	gtk_tree_sortable_set_sort_func(watch_sortable, LIST_ADDRESS_COLUMN, info_list_sort_address, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id(watch_sortable, LIST_VOLUME_COLUMN, GTK_SORT_DESCENDING);


	// setup alert list box
	viz->alert_list = glade_xml_get_widget(viz->xml, "alert_list");
	viz->alert_store = gtk_list_store_new(ALERT_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
	gtk_tree_view_set_model(GTK_TREE_VIEW(viz->alert_list), GTK_TREE_MODEL(viz->alert_store));
	g_object_unref(G_OBJECT(viz->alert_store));

	// add columns headers to watch list
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Timestamp", renderer, "text", ALERT_TIME_COLUMN, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 150);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(viz->alert_list), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Alert", renderer, "text", ALERT_STRING_COLUMN, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 200);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(viz->alert_list), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Value", renderer, "text", ALERT_VALUE_COLUMN, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 90);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(viz->alert_list), column);
}
