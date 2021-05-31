#!/bin/bash
# FILE DI TEST N°3
#
#

# terminazione immediata dopo 30 secondi del server 
(sleep 30; pkill -SIGINT -f server) &

# avvio il primo client
./client -f mipiacequestonome -p -t 1500 -d test3/salvati -D test3/flushati -W a/b/foto.jpg,a/b/test3.txt -u a/b/foto.jpg -l a/b/foto.jpg -w a/c -R 3 -c a/c/test4.txt -W a/test.txt -r a/test.txt &
# avvio il secondo client
./client -f mipiacequestonome -p -t 1500 -d test3/salvati -D test3/flushati -W a/b/foto.jpg,a/b/test3.txt -u a/b/foto.jpg -l a/b/foto.jpg -w a/c -R 3 -c a/c/test4.txt -W a/test.txt -r a/test.txt &
# avvio il terzo client
./client -f mipiacequestonome -p -t 1500 -d test3/salvati -D test3/flushati -W a/b/foto.jpg,a/b/test3.txt -u a/b/foto.jpg -l a/b/foto.jpg -w a/c -R 3 -c a/c/test4.txt -W a/test.txt -r a/test.txt &

# aspetto i client
wait

exit 0