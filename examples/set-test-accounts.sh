#! /bin/sh

# Set the Flickr user to Ross Burton
gconftool --set --type string /apps/mojito/sources/flickr/user 35468147630@N01

# Set the Twitter user to the Mojito Test user
gconftool --set --type string /apps/mojito/sources/twitter/user ross@linux.intel.com
gconftool --set --type string /apps/mojito/sources/twitter/password password
