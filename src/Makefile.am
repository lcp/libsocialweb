bin_PROGRAMS = mojito-core

mojito_core_CFLAGS = -I$(top_srcdir)/mojito $(DBUS_GLIB_CFLAGS) $(SOUP_CFLAGS)
mojito_core_SOURCES = main.c
mojito_core_LDADD = ../mojito/libmojito.la $(DBUS_GLIB_LIBS) $(SOUP_LIBS)

