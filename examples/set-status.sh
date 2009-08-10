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
    --dest=com.intel.Mojito \
    /com/intel/Mojito/Service/$SERVICE \
    com.intel.Mojito.Service.UpdateStatus \
    string:"$MESSAGE"
