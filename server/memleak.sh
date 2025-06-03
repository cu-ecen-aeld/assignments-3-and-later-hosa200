valgrind --error-exitcode=1 --leak-check=full --show-leak-kinds=all --track-origins=yes --errors-for-leak-kinds=definite --verbose --log-file=/tmp/memleak.txt ./aesdsocket
sleep 10s

cat /tmp/memleak.txt