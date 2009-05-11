#!/bin/sh
intltoolize --copy --force --automake
autoreconf -v -i && ./configure $@
