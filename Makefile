PKG = dbus-glib-1 sqlite3 libsoup-2.4 rss-glib-1.0 rest
CFLAGS = -g -Wall `pkg-config --cflags $(PKG)`
LDFLAGS =`pkg-config --libs $(PKG)`

ifeq ($(NOCACHE), 1)
CFLAGS += -DNO_CACHE
endif

all: mojito

%-glue.h: %.xml
	dbus-binding-tool --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

%-bindings.h: %.xml
	dbus-binding-tool --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

%.c: %.list
	glib-genmarshal --body --prefix=mojito_marshal $^ > $@

%.h: %.list
	glib-genmarshal --header --prefix=mojito_marshal $^ > $@

TELEPATHY_GLIB ?= ../telepathy-glib/
%-ginterface.c: %.xml
	python $(TELEPATHY_GLIB)/tools/glib-ginterface-gen.py --include='"marshals.h"' --filename=$(basename $@) $^ Mojito_

BUILT_SOURCES = \
	marshals.c marshals.h

SOURCES = main.c \
	mojito-core.c mojito-core.h \
	mojito-view.c mojito-view.h \
	mojito-source.c mojito-source.h \
	mojito-source-blog.c mojito-source-blog.h \
	mojito-photos.c mojito-photos.h \
	mojito-source-flickr.c mojito-source-flickr.h \
	mojito-web.c mojito-web.h \
	mojito-utils.c mojito-utils.h \
	generic.c generic.h \
	$(BUILT_SOURCES) \
	mojito-core-ginterface.c mojito-core-ginterface.h \
	mojito-view-ginterface.c mojito-view-ginterface.h \
# The ginterfaces are actually built sources, but until telepathy-glib has my
# patches merged we'll ship the files.

mojito: $(SOURCES)
	$(LINK.c) -o $@ $(filter %.c, $^)

clean:
	rm -f mojito $(BUILT_SOURCES)

.PHONY: clean
