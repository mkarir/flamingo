AM_CFLAGS = -g -g3 -O2 -Wall
bin_PROGRAMS = 

flamingodir = $(datadir)/flamingo

if BUILD_SERVER
bin_PROGRAMS += pond
endif

if BUILD_CLIENT
bin_PROGRAMS += flamingo
endif



flamingo_SOURCES =	main.c main.h \
			main_interface.c main_interface.h \
			info_interface.c info_interface.h \
			prefs_interface.c prefs_interface.h \
			playback_interface.c playback_interface.h \
			controls_interface.c controls_interface.h \
			handler.c handler.h \
			handler_as.c handler_as.h \
			handler_ip.c handler_ip.h \
			handler_port.c handler_port.h \
			handler_src_dst_ip.c handler_src_dst_ip.h \
			handler_src_dst_as.c handler_src_dst_as.h \
			handler_src_dst_port.c handler_src_dst_port.h \
			glext3d.c glext3d.h \
			glext2d.c glext2d.h \
			alerts.c alerts.h \
			range.c range.h \
			trackball.c trackball.h \
			socket.c socket.h \
			protocol.h

flamingo_LDADD =	$(GLIB_LIBS) \
		 	$(GTK_LIBS) \
			$(GLADE_LIBS) \
			$(GTKGLEXT_LIBS) \
			$(GL_LIBS) \
			$(PCAP_LIBS) \
			$(MATH_LIBS)

flamingo_CFLAGS =	$(AM_CFLAGS) \
			$(GTK_CFLAGS) \
			$(GLADE_CFLAGS) \
			$(GTKGLEXT_CFLAGS) \
			$(GLIB_CFLAGS) \
			-DFLAMINGO_DIR=\""$(flamingodir)"\"



pond_SOURCES =		pond.c pond.h \
			server.c server.h \
			gtree.c gtree.h \
			myflow.c myflow.h \
			anomaly_detection.c anomaly_detection.h \
			eth.h ip.h udp.h tcp.h icmp.h \
			protocol.h

pond_LDADD =		$(FT_LIBS) \
			$(GLIB_LIBS) \
			$(GTHREAD_LIBS) \
			$(PCAP_LIBS) \
			$(MATH_LIBS)

pond_CFLAGS =		$(AM_CFLAGS) \
			$(GLIB_CFLAGS) \
			$(GTHREAD_CFLAGS) \
			-DFLAMINGO_DIR=\""$(flamingodir)"\"
