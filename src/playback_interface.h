#ifndef __PLAYBACK_INTERFACE_H__
#define __PLAYBACK_INTERFACE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <config.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <glade/glade.h>

#include "socket.h"

void playback_init(void);
void playback_process_response(pbreq_error_t *response, guint len);
void playback_resume_action();
void playback_pause_action();
void playback_stop_action();
void playback_end_action();

#endif
