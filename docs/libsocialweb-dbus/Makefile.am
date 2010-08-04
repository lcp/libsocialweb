# We require automake 1.6 at least.
AUTOMAKE_OPTIONS = 1.6

# This is a blank Makefile.am for using gtk-doc.
# Copy this to your project's API docs directory and modify the variables to
# suit your project. See the GTK+ Makefiles in gtk+/docs/reference for examples
# of using the various options.

# The name of the module, e.g. 'glib'.
DOC_MODULE=libsocialweb-dbus

# Uncomment for versioned docs and specify the version of the module, e.g. '2'.
#DOC_MODULE_VERSION=2

# The top-level SGML file. You can change this if you want to.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# Images to copy into HTML directory.
# e.g. HTML_IMAGES=$(top_srcdir)/gtk/stock-icons/stock_about_24.png
HTML_IMAGES=

DBUS=lastfm.xml sw-avatar.xml sw-banishable.xml sw-core.xml sw-item-view.xml sw-photo-upload.xml sw-query.xml sw-service.xml sw-status-update.xml

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
# e.g. content_files=running.sgml building.sgml changes-2.0.sgml
content_files=$(foreach dbus, $(DBUS), xml/ref-$(dbus))

# SGML files where gtk-doc abbrevations (#GtkWidget) are expanded
# These files must be listed here *and* in content_files
# e.g. expand_content_files=running.sgml
expand_content_files=

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/gtk-doc.make

# Other files to distribute
# e.g. EXTRA_DIST += version.xml.in
EXTRA_DIST +=

# Files not to distribute
# for --rebuild-types in $(SCAN_OPTIONS), e.g. $(DOC_MODULE).types
# for --rebuild-sections in $(SCAN_OPTIONS) e.g. $(DOC_MODULE)-sections.txt
#DISTCLEANFILES +=

# Comment this out if you want your docs-status tested during 'make check'
if ENABLE_GTK_DOC
#TESTS_ENVIRONMENT = cd $(srcsrc) &&
#TESTS = $(GTKDOC_CHECK)
endif

xml/ref-%.xml: $(top_srcdir)/interfaces/%.xml
	$(XSLT) -o $@ $(top_srcdir)/docs/libsocialweb-dbus/spec-to-docbook.xsl $<

-include $(top_srcdir)/git.mk

# We don't use these to clean them away
CLEANFILES += libsocialweb-dbus-sections.txt libsocialweb-dbus.types libsocialweb-dbus-decl-list.txt libsocialweb-dbus-decl.txt libsocialweb-dbus-overrides.txt
