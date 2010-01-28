#! /bin/sh

# Set the Flickr user to Ross Burton
gconftool --set --type string /apps/libsocialweb/services/flickr/user 35468147630@N01

# Set the Twitter user to the libsocialweb test user
gconftool --set --type string /apps/libsocialweb/services/twitter/user libsocialwebtest
gconftool --set --type string /apps/libsocialweb/services/twitter/password password

# Last.fm to Ross Burton
gconftool --set --type string /apps/libsocialweb/services/lastfm/user rossburton
