#!/bin/bash
# FILE DI TEST N°2
#
#

# attendo 1 secondo per permettere al server di partire
sleep 1

# avvio il primo client
./client -f mipiacequestonome.sk -p -t 200 -d TestDirectory/output/Client/salvati -D TestDirectory/output/Client/flushati -W TestDirectory/tests/a/b/foto.jpg,TestDirectory/tests/a/b/test3.txt -u TestDirectory/tests/a/b/foto.jpg -l TestDirectory/tests/a/b/foto.jpg -w TestDirectory/tests/a/c -R 3 -c TestDirectory/tests/a/c/test4.txt -W TestDirectory/tests/a/test.txt -r TestDirectory/tests/a/test.txt &
# avvio il secondo client
./client -f mipiacequestonome.sk -p -t 200 -d TestDirectory/output/Client/salvati -D TestDirectory/output/Client/flushati -W TestDirectory/tests/a/b/foto.jpg,TestDirectory/tests/a/b/test3.txt -u TestDirectory/tests/a/b/foto.jpg -l TestDirectory/tests/a/b/foto.jpg -w TestDirectory/tests/a/c -R 3 -c TestDirectory/tests/a/c/test4.txt -W TestDirectory/tests/a/test.txt -r TestDirectory/tests/a/test.txt &
# avvio il terzo client
./client -f mipiacequestonome.sk -p -t 200 -d TestDirectory/output/Client/salvati -D TestDirectory/output/Client/flushati -W TestDirectory/tests/a/b/foto.jpg,TestDirectory/tests/a/b/test3.txt -u TestDirectory/tests/a/b/foto.jpg -l TestDirectory/tests/a/b/foto.jpg -w TestDirectory/tests/a/c -R 3 -c TestDirectory/tests/a/c/test4.txt -W TestDirectory/tests/a/test.txt -r TestDirectory/tests/a/test.txt &

# aspetto i client
wait

# terminazione lenta al server
pkill -SIGHUP -f server

exit 0