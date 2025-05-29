echo $1
case "$1" in
    start)
        echo "Starting aesdsocket socket"
        start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
        ;;
    stop)
        echo "Stoping the aesdsocket server"
        start-stop-daemon -K -n aesdsocket
        ;;
    *)
        echo "Usage: $0 {start | stop}"
    exit 1
esac