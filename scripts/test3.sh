#!/bin/bash
# FILE DI TEST N°3
#
#

# terminazione immediata dopo 30 secondi del server 
(sleep 30; pkill -SIGINT -f server) &

# avvio il primo client
./client -f mipiacequestonome.sk -p -t 1500 -d TestDirectory/output/Client/salvati -D TestDirectory/output/Client/flushati -W TestDirectory/tests/a/b/foto.jpg,TestDirectory/tests/a/b/test3.txt -u TestDirectory/tests/a/b/foto.jpg -l TestDirectory/tests/a/b/foto.jpg -w TestDirectory/tests/a/c -R 3 -c TestDirectory/tests/a/c/test4.txt -W TestDirectory/tests/a/test.txt -r TestDirectory/tests/a/test.txt &
# avvio il secondo client
./client -f mipiacequestonome.sk -p -t 1500 -d TestDirectory/output/Client/salvati -D TestDirectory/output/Client/flushati -W TestDirectory/tests/a/b/foto.jpg,TestDirectory/tests/a/b/test3.txt -u TestDirectory/tests/a/b/foto.jpg -l TestDirectory/tests/a/b/foto.jpg -w TestDirectory/tests/a/c -R 3 -c TestDirectory/tests/a/c/test4.txt -W TestDirectory/tests/a/test.txt -r TestDirectory/tests/a/test.txt &
# avvio il terzo client
./client -f mipiacequestonome.sk -p -t 1500 -d TestDirectory/output/Client/salvati -D TestDirectory/output/Client/flushati -W TestDirectory/tests/a/b/foto.jpg,TestDirectory/tests/a/b/test3.txt -u TestDirectory/tests/a/b/foto.jpg -l TestDirectory/tests/a/b/foto.jpg -w TestDirectory/tests/a/c -R 3 -c TestDirectory/tests/a/c/test4.txt -W TestDirectory/tests/a/test.txt -r TestDirectory/tests/a/test.txt &

# aspetto i client
wait

exit 0