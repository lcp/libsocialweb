#!/bin/sh
mkdir m4
intltoolize --copy --force --automake
ACLOCAL="${ACLOCAL-aclocal} $ACLOCAL_FLAGS" autoreconf -v -i && ./configure $@
