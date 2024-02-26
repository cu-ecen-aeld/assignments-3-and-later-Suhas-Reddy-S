#!/bin/sh

DAEMON=/usr/bin/aesdsocket

case "$1" in
    start)
        echo "Starting aesdsocket..."
        start-stop-daemon --start --background --exec $DAEMON -- -d
        ;;
    stop)
        echo "Stopping aesdsocket..."
        start-stop-daemon --stop --exec -d
        ;;
    restart)
        echo "Restarting aesdsocket..."
        $0 stop
        sleep 1
        $0 start
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac

exit 0

