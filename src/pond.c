#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <glob.h>
#include <time.h>
#include <glib.h>
#include <config.h>

#include "pond.h"
#include "myflow.h"
#include "protocol.h"
#include "server.h"
#include "gtree.h"

void load_config_file();
void exit_handler(gint sig_num);
void load_routing_table();
void load_aux_entries(gchar *filename);
void init_feeds();

pond_t *pond;


gint main(gint argc, gchar **argv)
{
	// ignore SIGPIPE
	signal(SIGPIPE, SIG_IGN);

	// initialize g_thread subsystem
	g_thread_init(NULL);

	// install signal handler to catch CTRL-C and terminate program
	signal(SIGINT, exit_handler);

	// allocate our main structure
	pond = g_new0(pond_t, 1);

	// allocate our feed/client list
	pond->feeds = g_ptr_array_new();
	pond->clients = g_ptr_array_new();

	// allocate our master data structures
	pond->master_ip = g_tree_new_full(g_tree_compare_ip, NULL, g_free, NULL);
	pond->pallet = g_ptr_array_sized_new(65536);

	// load config file (pond.conf)
	load_config_file();

	// start feed processing threads
	init_feeds();

	// start server thread
	init_server();

	return 0;
}



void init_feeds()
{
	guint i;
	nf_feed_t *feed;

	// loop through each feed
	for (i = 0; i < pond->num_feeds; ++i) {
		feed = g_ptr_array_index(pond->feeds, i);

		// attempt to bind each feed to its specified port
		if (!nf_feed_bind(feed)) {
			// remove the feed if the bind fails
			g_ptr_array_remove_index_fast(pond->feeds, i);
			g_free(feed);
			pond->num_feeds--;
			i--;
			continue;
		}

		feed->id = i;

		// create a thread to handle each feed's incoming data
		if (!g_thread_create(nf_feed_thread, (gpointer) feed, FALSE, NULL)) {
			g_error("feed thread creation error");
		}
	}

	// make sure at least one feed is active
	if (pond->num_feeds == 0) {
		g_error("no active feeds available, terminating server");
	}
}


void load_config_file()
{
	GError *error = NULL;
	GKeyFile *server_conf;
	nf_feed_t *feed;
	guint i, port, scale, sample, max_clients, listen_port;
	gboolean aggregation, as_availability;
	gchar **feed_groups, *password, *archive_path, *bgp_table, *aux_table_entries, **aux_table_entries_list;
	gchar *conf_path, *pwd_path, *home_path, *etc_path, *conf_file, *pwd;
	
	conf_file = g_strdup("pond.conf");

	// build all possible paths to our config file
	pwd = g_get_current_dir();
	pwd_path = g_build_filename(pwd, conf_file, NULL);
	home_path = g_build_filename(g_get_home_dir(), ".flamingo", conf_file, NULL);
	etc_path = g_build_filename(G_DIR_SEPARATOR_S, "etc", "flamingo", conf_file, NULL);

	// initialize our GKeyFile prefs
	server_conf = g_key_file_new();

	g_debug("searching paths: %s:%s:%s", pwd_path, home_path, etc_path);

	// search for a pond.conf to use (search order: pwd, $HOME/.flamingo/ /etc/flamingo/)
	if (g_file_test(pwd_path, G_FILE_TEST_EXISTS)) {
		conf_path = pwd_path;
	} else if (g_file_test(home_path, G_FILE_TEST_EXISTS)) {
		conf_path = home_path;
	} else if (g_file_test(etc_path, G_FILE_TEST_EXISTS)) {
		conf_path = etc_path;
	} else {
		conf_path = NULL;
		g_error("no pond.conf found in the search paths");
	}

	g_debug("config file found: %s", conf_path);

	// attempt to load our config file
	if (!g_key_file_load_from_file(server_conf, conf_path, G_KEY_FILE_KEEP_COMMENTS, NULL)) {
		g_error("error loading config from %s", conf_path);
	}
	
	
	// attempt to read the bgp_routing_table name from the config file
	bgp_table = g_key_file_get_string(server_conf, "Preferences", "BGP_Table", NULL);

	if (bgp_table) {
		// if found in the config, load the table into memory
		g_debug("found bgp table (%s) in config file", bgp_table);
		load_routing_table(bgp_table);
	} else {
		// if its not found, use the default table file that is part of the server distribution
		g_debug("couldn't find bgp table in config file, default table loaded");
		load_routing_table("routingtable.input");
	}
	
	
	// attempt to read our netflow archive path from the config file
	aux_table_entries = g_key_file_get_string(server_conf, "Preferences", "Aux_Table_Entries", NULL);

	if (aux_table_entries) {
		// if found in the config, load the tables into memory
		
		aux_table_entries_list = g_strsplit (aux_table_entries, " ", 10000);

		i=0;
		
		while(aux_table_entries_list[i]){
			load_aux_entries(aux_table_entries_list[i]);
			g_debug("loaded aux table entries (%s)", aux_table_entries_list[i]);
			i++;
		}
	}


	// attempt to read our netflow archive path from the config file
	archive_path = g_key_file_get_string(server_conf, "Preferences", "Netflow_Archive_Path", NULL);

	if (archive_path) {
		// if found in the config, set archive_path to it 
		pond->archive_path = g_strdup(archive_path);
		g_debug("archive path found in pond.conf, set to %s", pond->archive_path);
	} else {
		// if its not found, set archive_path to the present working directory
		pond->archive_path = g_strdup(pwd);
		g_debug("archive path not found in pond.conf, set to %s", pond->archive_path);
	}


	// attempt to read our server password from the config file
	password = g_key_file_get_string(server_conf, "Preferences", "Password", NULL);

	if (password) {
		// if found in the config, set password to it 
		pond->password = g_strdup(password);
		g_debug("password found in pond.conf, set to %s", pond->password);
	} else {
		// if its not found, don't set a password
		pond->password = NULL;
		g_debug("password not found in pond.conf, no password set");
	}


	// retrieve max clients number
	max_clients = g_key_file_get_integer(server_conf, "Preferences", "Max_Clients", &error);

	if (!error) {
		g_debug("max clients found in pond.conf");
		pond->max_clients = max_clients;
	} else {
		g_debug("max clients not found in pond.conf");
		pond->max_clients = DEFAULT_MAX_CLIENTS;
		g_error_free(error);
	}

	
	// retrieve the server listen port number
	listen_port = g_key_file_get_integer(server_conf, "Preferences", "Listen_Port", &error);

	if (!error) {
		g_debug("listen port found in pond.conf");
		pond->listen_port = listen_port;
	} else {
		g_debug("listen port not found in config, defaulting to %d", DEFAULT_LISTEN_PORT);
		pond->listen_port = DEFAULT_LISTEN_PORT;
		g_error_free(error);
	}


	// get a list of all the groups found in the config file
	feed_groups = g_key_file_get_groups(server_conf, NULL);

	// loop through the groups
	for (i = 0; feed_groups[i] != NULL; ++i) {
		error = NULL;

		if (g_ascii_strcasecmp(feed_groups[i], "Preferences") == 0) {
			continue;
		}

		// retrieve feed port number
		port = g_key_file_get_integer(server_conf, feed_groups[i], "Port", &error);

		// if there was an error, skip this group
		if (error) {
			g_debug("error retrieving feed parameters from config for group %s", feed_groups[i]);
			g_error_free(error);
			continue;
		}

		// retrieve feed scaling factor
		scale = g_key_file_get_integer(server_conf, feed_groups[i], "Scaling", &error);

		// if there was an error, skip this group
		if (error) {
			g_debug("error retrieving feed parameters from config for group %s", feed_groups[i]);
			g_error_free(error);
			continue;
		}

		// retrieve feed sampling percentage
		sample = g_key_file_get_integer(server_conf, feed_groups[i], "Sampling", &error);

		// if there was an error, skip this group
		if (error) {
			g_debug("error retrieving feed parameters from config for group %s", feed_groups[i]);
			g_error_free(error);
			continue;
		}

		// retrieve feed aggregated flag
		aggregation = g_key_file_get_boolean(server_conf, feed_groups[i], "Aggregation", &error);

		// if there was an error, skip this group
		if (error) {
			g_debug("error retrieving feed parameters from config for group %s", feed_groups[i]);
			g_error_free(error);
			continue;
		}
		
		// retrieve feed as_availability flag
		as_availability = g_key_file_get_boolean(server_conf, feed_groups[i], "AS_Availability", &error);

		// if there was an error, skip this group
		if (error) {
			g_debug("error retrieving feed parameters from config for group %s", feed_groups[i]);
			g_error_free(error);
			continue;
		}
			
		// assign the feeds attributes
		feed = g_new0(nf_feed_t, 1);
		feed->port = port;
		feed->scalefactor = scale;
		feed->samplerate = sample;
		feed->aggregation = aggregation;
		feed->as_availability = as_availability;

		// truncate feed name to EXPORTER_NAME_LEN
		g_strlcpy(feed->name, feed_groups[i], EXPORTER_NAME_LEN);
		g_debug("read feed %s on port %d from config file", feed->name, port);

		// insert our feed into the feed list
		pond->num_feeds++;
		g_ptr_array_add(pond->feeds, feed);
	}

	// free our list
	g_strfreev(feed_groups);

	// free up all our allocated path strings
	g_free(pwd);
	g_free(pwd_path);
	g_free(home_path);
	g_free(etc_path);
	g_free(conf_file);
	g_free(archive_path);
	g_free(password);

	// free our GKeyFile
	g_key_file_free(server_conf);
}


void exit_handler(gint sig_num)
{
	g_debug("caught interrupt signal...server terminating");
	exit(0);
}


void load_routing_table(char *filename)
{
	guint i = 0;
	GRand *rng;
	FILE *infile = NULL;
	ip_master_t *ip_node;
	color_t *color;
	gchar buf[1000], *tmp;
	struct in_addr tmpaddr;
	gint len=0, as=0;

	// initialize the RNG with a static seed!
	rng = g_rand_new_with_seed(1);

	gchar *primary = g_strdup_printf("%s%s", PRIMARY_PATH, filename);
	gchar *backup = g_strdup_printf("%s%s", BACKUP_PATH, filename);

	// read in BGP routing table
	if (g_file_test(primary, G_FILE_TEST_EXISTS)) {
		infile = fopen(primary, "r");
	} else if (g_file_test(backup, G_FILE_TEST_EXISTS)) {
		infile = fopen(backup, "r");
	} else {
		g_error("Could not find %s routing table", filename);
	}

	g_free(primary);
	g_free(backup);

	if (infile == NULL){
		g_error("Error: couldn't open input file '%s'", filename);
	}

	while (fgets(buf, 1000, infile) != NULL ) {
		if ((tmp = strtok(buf, "/")) == NULL) {
			continue;
		}
		
		inet_aton(tmp, &tmpaddr);
		
		if ((tmp = strtok(NULL, " ")) == NULL) {
			continue;
		}

		len = atoi(tmp);

		while ((tmp = strtok(NULL, " ")) != NULL) {
			as = atoi(tmp);
		}

		// make sure our entry is valid (for example, 35.0.0.0/7 is invalid, should be 34.0.0.0/7)
		guint mask = pow(2, 32 - len) - 1;
		guint addr_masked = g_ntohl(tmpaddr.s_addr) & ~mask;
		if (addr_masked != g_ntohl(tmpaddr.s_addr)) {
			g_debug("uh-oh, invalid entry in routing table: %s %d", inet_ntoa(tmpaddr), len);
		}

		// allocate mem for our new ip master node
		ip_node = g_new0(ip_master_t, 1);

		// assign the ip master node's vars
		ip_node->addr = g_ntohl(tmpaddr.s_addr);
		ip_node->network_bits = len;
		ip_node->origin_as = as;
		ip_node->red = g_rand_int_range(rng, 10, 250);
		ip_node->green = g_rand_int_range(rng, 10, 250);
		ip_node->blue = g_rand_int_range(rng, 10, 250);

		// insert that ip master node into the ip master tree
		g_tree_insert(pond->master_ip, ip_node, NULL);

		++i;
	}

	fclose(infile);

	for(i=0; i <= COLOR_SIZE; i++){
		color = g_new0(color_t, 1);
		
		color->red = g_rand_int_range(rng, 10, 250);
		color->green = g_rand_int_range(rng, 10, 250);
		color->blue = g_rand_int_range(rng, 10, 250);

		g_ptr_array_add(pond->pallet, color);
	}

	g_rand_free(rng);
}



void load_aux_entries(gchar *filename)
{
	FILE *infile = NULL;
	gchar buf[1000], *tmp;
	struct in_addr tmpaddr;
	gint len=0, i=0, red=0, green=0, blue=0;
	ip_master_t *ip_node;

	gchar *primary = g_strdup_printf("%s%s", PRIMARY_PATH, filename);
	gchar *backup = g_strdup_printf("%s%s", BACKUP_PATH, filename);

	// read in aux table
	if (g_file_test(primary, G_FILE_TEST_EXISTS)) {
		infile = fopen(primary, "r");
	} else if (g_file_test(backup, G_FILE_TEST_EXISTS)) {
		infile = fopen(backup, "r");
	} else {
		g_error("Could not find %s aux table", filename);
	}

	g_free(primary);
	g_free(backup);

	if(infile == NULL){
		printf("Error: couldn't open input file '%s'\n", filename);
		exit(1);
	}

	while(1){
		fgets (buf, 1000, infile);
		if(buf[0] == 'c')
			break;
	}
	
	strtok(buf, " ");
	red = atoi( strtok(NULL, " ") );
	green = atoi( strtok(NULL, " ") );
	blue = atoi( strtok(NULL, " ") );

	
	while( fgets (buf, 1000, infile) != NULL ){
		if( buf[0] == '#' )
			continue;

		if( (tmp = strtok(buf, "/")) == NULL)
			continue;
		
		inet_aton(tmp, &tmpaddr);
		
		if( (tmp = strtok(NULL, " ")) == NULL)
			continue;

		len = atoi(tmp);

		// allocate mem for our new ip master node
		ip_node = g_new0(ip_master_t, 1);

		// assign the ip master node's vars
		ip_node->addr = g_ntohl(tmpaddr.s_addr);
		ip_node->network_bits = len;
		ip_node->origin_as = 0;
		ip_node->red = red;
		ip_node->green = green;
		ip_node->blue = blue;

		// insert that ip master node into the ip master tree
		g_tree_insert(pond->master_ip, ip_node, NULL);
				
		i++;

	}

	fclose(infile);
}
