bin_PROGRAMS = upnpmpd
upnpmpd_SOURCES = main.c \
	upnp.c upnp_control.c upnp_connmgr.c  upnp_transport.c \
	upnp.h upnp_control.h upnp_connmgr.h  upnp_transport.h \
	upnp_device.c upnp_device.h \
	upnp_renderer.h upnp_renderer.c \
	webserver.c webserver.h \
	output_mpd.c  output_mpd.h \
	logging.h \
	xmlescape.c xmlescape.h

AM_LDFLAGS = $(UPNP_LDFLAGS)
AM_CPPFLAGS = $(MPD_CFLAGS) $(GLIB_CFLAGS) $(UPNP_CPPFLAGS) -DPKG_DATADIR=\"$(datadir)/upnpmpd\"
upnpmpd_LDADD = $(MPD_LIBS) $(GLIB_LIBS) $(CFG_LIBS) $(UPNP_LIBS) 

distclean-local:
	rm -rf bin obj
