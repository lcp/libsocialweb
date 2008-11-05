PKG = sqlite3 libsoup-2.4 rss-glib-1.0
CFLAGS = -g -Wall `pkg-config --cflags $(PKG)`
LDFLAGS =`pkg-config --libs $(PKG)`

ifeq ($(NOCACHE), 1)
CFLAGS += -DNO_CACHE
endif

mojito: main.c mojito-core.c mojito-core.h mojito-source.c mojito-source.h mojito-source-blog.c mojito-source-blog.h mojito-utils.c mojito-utils.h generic.c generic.h
	$(LINK.c) -o $@ $(filter %.c, $^)

clean:
	rm -f mojito

.PHONY: clean
