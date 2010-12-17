vapidir = $(datarootdir)/vala/vapi
vapi_DATA = \
	libsocialweb-client.vapi \
	$(NULL)
dist_vapi_DATA = \
	libsocialweb-client.deps \
	$(NULL)

BUILT_SOURCES = \
	$(vapi_DATA) \
	$(NULL)

pkgconfigdir = ${libdir}/pkgconfig

libsocialweb-client.vapi: $(top_builddir)/libsocialweb-client/SocialWebClient-0.25.gir
	$(VAPIGEN_V)$(VAPIGEN) \
		--library libsocialweb-client \
		--pkg gio-2.0 \
		$< \
		$(NULL)
	@mv $@ libsocialweb-client.vapi.tmp
	$(AM_V_GEN)sed \
		'-e s/SocialWebClient-0.25.h/libsocialweb-client\/libsocialweb-client-hack-for-vala.h/' \
		'-e s/namespace Sw/namespace SocialWebClient/' \
		'-e s/Sw\./SocialWebClient./g' \
		< libsocialweb-client.vapi.tmp > $@
	@rm libsocialweb-client.vapi.tmp

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

# set up the verbosity rules to avoid some build noise
VAPIGEN_V = $(VAPIGEN_V_$(V))
VAPIGEN_V_ = $(VAPIGEN_V_$(AM_DEFAULT_VERBOSITY))
VAPIGEN_V_0 = @echo "  VAPIG " $^;
