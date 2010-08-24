#! /bin/sh


if [ $# -lt 2 ]; then
    echo "Incorrect arguments:"
    echo "$ set-status <SERVICE> <MESSAGE>"
    exit 1
fi

SERVICE=$1
shift
MESSAGE="$@"

dbus-send --session --print-reply \
    --dest=com.meego.libsocialweb \
    /com/meego/libsocialweb/Service/$SERVICE \
    com.meego.libsocialweb.StatusUpdate.UpdateStatus \
    string:"$MESSAGE" dict:string:string:
