#!/bin/sh
intltoolize --copy --force --automake
ACLOCAL="${ACLOCAL-aclocal} $ACLOCAL_FLAGS" autoreconf -v -i && ./configure $@
