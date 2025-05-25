valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=/tmp/memleak.txt ./aesdsocket

cat /tmp/memleak.txt