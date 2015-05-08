AC_INIT(flamingo, 0.5, jonojono@merit.edu)
AC_CONFIG_SRCDIR(src/main.c)
AC_PREREQ(2.57)
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS(config.h)

AC_PROG_CC
AC_STDC_HEADERS
AC_PROG_INSTALL

build_server="yes"
build_client="yes"

# check for --enable-server-only
AC_ARG_ENABLE(server-only,
[  --enable-server-only    only build the server module],
[ build_client="no" ])

# check for --enable-client-only
AC_ARG_ENABLE(client-only,
[  --enable-client-only    only build the server module],
[ build_server="no" ])

# define conditionals for src/Makefile.am
AM_CONDITIONAL(BUILD_SERVER, test "x$build_server" = "xyes")
AM_CONDITIONAL(BUILD_CLIENT, test "x$build_client" = "xyes")


# check for standard header files
AC_HEADER_STDC
AC_CHECK_HEADERS(stdio.h math.h time.h netdb.h netinet/in.h arpa/inet.h)

# check for glib (needed by both client and server)
PKG_CHECK_MODULES(GLIB,	glib-2.0 >= 2.6, AC_MSG_RESULT([found]), AC_MSG_ERROR([glib-2.0 >= 2.6 not found]))
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

# check for libpcap (needed by both client and server)
AC_CHECK_LIB(pcap, bpf_validate, PCAP_LIBS="-lpcap", AC_MSG_ERROR([libpcap not found]))
AC_SUBST(PCAP_LIBS)

# check for math library (needed by both client and server)
AC_CHECK_LIB(m, pow, MATH_LIBS="-lm", AC_MSG_ERROR([libm not found]))
AC_SUBST(MATH_LIBS)


# determine whether to check server dependencies
if test "x$build_server" = "xyes" ; then
  # check for gthread (needed by server only)
  PKG_CHECK_MODULES(GTHREAD, gthread-2.0 >= 2.6, AC_MSG_RESULT([found]), AC_MSG_ERROR([gthread-2.0 >= 2.6 not found]))
  AC_SUBST(GTHREAD_CFLAGS)
  AC_SUBST(GTHREAD_LIBS)

  # check for flow-tools headers/libs (needed by server only)
  AC_CHECK_HEADERS(ftlib.h, [], AC_MSG_ERROR([ftlib.h not found]))
  FT_LIBS="-lft -lz"
  AC_SUBST(FT_LIBS)
fi


# determine whether to check client dependencies
if test "x$build_client" = "xyes" ; then
  # check for all our gtk related packages (needed by client only)
  PKG_CHECK_MODULES(GTK, gtk+-2.0 >= 2.6 gdk-2.0 >= 2.6, AC_MSG_RESULT([found]), AC_MSG_ERROR([gtk-2.0 >= 2.6 not found]))
  AC_SUBST(GTK_CFLAGS)
  AC_SUBST(GTK_LIBS)

  PKG_CHECK_MODULES(GLADE, libglade-2.0 >= 2.5, AC_MSG_RESULT([found]), AC_MSG_ERROR([libglade-2.0 >= 2.5 not found]))
  AC_SUBST(GLADE_CFLAGS)
  AC_SUBST(GLADE_LIBS)

  PKG_CHECK_MODULES(GTKGLEXT, gtkglext-1.0 >= 1.0 gdkglext-1.0 >= 1.0, AC_MSG_RESULT([found]), AC_MSG_ERROR([gtkglext-1.0 >= 1.0 not found]))
  AC_SUBST(GTKGLEXT_CFLAGS)
  AC_SUBST(GTKGLEXT_LIBS)

  # check for mesa (opengl) headers/libs (needed by client only)
  AC_CHECK_HEADERS(GL/gl.h, [], AC_MSG_ERROR([GL/gl.h not found]))
  AC_CHECK_LIB(GL, glBegin, GL_LIBS="-lGL -lstdc++", AC_MSG_ERROR([libGL not found]),)
  AC_SUBST(GL_LIBS)
fi


AC_CONFIG_FILES([Makefile src/Makefile data/Makefile])
AC_CONFIG_COMMANDS([default],[[ echo timestamp > stamp-h ]],[[]])
AC_OUTPUT
