#!/bin/bash
# FILE DI TEST N°2
#
#

# attendo 1 secondo per permettere al server di partire
sleep 1

# avvio il primo client
./client -f mipiacequestonome -p -t 200 -d test2/salvati -D test2/flushati -W a/b/foto.jpg,a/b/test3.txt -u a/b/foto.jpg -l a/b/foto.jpg -w a/c -R 3 -c a/c/test4.txt -W a/test.txt -r a/test.txt &
# avvio il secondo client
./client -f mipiacequestonome -p -t 200 -d test2/salvati -D test2/flushati -W a/b/foto.jpg,a/b/test3.txt -u a/b/foto.jpg -l a/b/foto.jpg -w a/c -R 3 -c a/c/test4.txt -W a/test.txt -r a/test.txt &
# avvio il terzo client
./client -f mipiacequestonome -p -t 200 -d test2/salvati -D test2/flushati -W a/b/foto.jpg,a/b/test3.txt -u a/b/foto.jpg -l a/b/foto.jpg -w a/c -R 3 -c a/c/test4.txt -W a/test.txt -r a/test.txt &

# aspetto i client
wait

# terminazione lenta al server
pkill -SIGHUP -f server

exit 0