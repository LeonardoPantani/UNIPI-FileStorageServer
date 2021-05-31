#!/bin/bash
# FILE DI TEST N°1
#
#

# attendo attivazione valgrind per 3 secondi
sleep 3

# avvio il client
./client -f mipiacequestonome -p -t 200 -d test1/salvati -D test1/flushati -W a/b/foto.jpg,a/b/test3.txt -u a/b/foto.jpg -l a/b/foto.jpg -w a/c -R 3 -c a/c/test4.txt -W a/test.txt -r a/test.txt

# aspetto i client
wait

# terminazione lenta al server
pkill -SIGHUP -f server

# aspetto il termine
sleep 2

# ottengo il numero di errori
r=$(tail -10 ./valgrind_output.txt | grep "ERROR SUMMARY" | cut -d: -f 2 | cut -d" " -f 2)

# se il numero di errori è diverso da 0 allora non va bene
if [[ $r != 0 ]]; then
    echo "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    echo "xxxxxxxxxxxxxx TEST 1 FALLITO xxxxxxxxxxxxxx"
    echo "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    exit 1
fi

exit 0