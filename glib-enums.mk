# Rules for generating enumeration types using glib-mkenums
#
# Define:
# 	glib_enum_h = header template file
# 	glib_enum_c = source template file
# 	glib_enum_headers = list of headers to parse
#
# before including Makefile.am.enums. You will also need to have
# the following targets already defined:
#
# 	CLEANFILES
#	DISTCLEANFILES
#	BUILT_SOURCES
#	EXTRA_DIST
#
# Author: Emmanuele Bassi <ebassi@linux.intel.com>

enum_tmpl_h=$(glib_enum_h:.h=.h.in)
enum_tmpl_c=$(glib_enum_c:.c=.c.in)

CLEANFILES += stamp-enum-types
DISTCLEANFILES += $(glib_enum_h) $(glib_enum_c)
BUILT_SOURCES += $(glib_enum_h) $(glib_enum_c)
EXTRA_DIST += $(srcdir)/$(enum_tmpl_h) $(srcdir)/$(enum_tmpl_c)

stamp-enum-types: $(glib_enum_headers) $(srcdir)/$(enum_tmpl_h)
	$(AM_V_GEN)$(GLIB_MKENUMS) \
		--template $(srcdir)/$(enum_tmpl_h) \
	$(glib_enum_headers) > xgen-eh \
	&& (cmp -s xgen-eh $(glib_enum_h) || cp -f xgen-eh $(glib_enum_h)) \
	&& rm -f xgen-eh \
	&& echo timestamp > $(@F)

$(glib_enum_h): stamp-enum-types
	@true

$(glib_enum_c): $(glib_enum_h) $(srcdir)/$(enum_tmpl_c)
	$(AM_V_GEN)$(GLIB_MKENUMS) \
		--template $(srcdir)/$(enum_tmpl_c) \
	$(glib_enum_headers) > xgen-ec \
	&& cp -f xgen-ec $(glib_enum_c) \
	&& rm -f xgen-ec

