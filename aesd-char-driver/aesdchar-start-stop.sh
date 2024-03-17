#! bin/sh

case "$1" in 
    start)
        echo "Staring aesdchar driver"
        aesdchar_load
        ;;
    stop)
        echo "Stoping aesdchar driver"
        aesdchar_unload
        ;;
        *)
        echo "Usage: $0 {start|stop}"
    exit 1
esac 

exit 0
