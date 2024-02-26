#!/bin/sh

case "$1" in
    start)
        echo "Starting aesdsocket..."
        start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
        ;;
    stop)
        echo "Stopping aesdsocket..."
        start-stop-daemon -K -n aesdsocket
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

