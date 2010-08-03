#!/bin/sh
mkdir -p m4
gtkdocize || exit 1
intltoolize --copy --force --automake || exit 1
ACLOCAL="${ACLOCAL-aclocal} $ACLOCAL_FLAGS" autoreconf -v -i && ./configure $@
