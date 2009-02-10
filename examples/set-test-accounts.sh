#! /bin/sh

# Set the Flickr user to Ross Burton
gconftool --set --type string /apps/mojito/services/flickr/user 35468147630@N01

# Set the Twitter user to the Mojito Test user
gconftool --set --type string /apps/mojito/services/twitter/user ross@linux.intel.com
gconftool --set --type string /apps/mojito/services/twitter/password password

# Last.fm to Ross Burton
gconftool --set --type string /apps/mojito/services/lastfm/user rossburton
