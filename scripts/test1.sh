#!/bin/bash
# FILE DI TEST N°1
#
#

# attendo attivazione valgrind per 3 secondi
sleep 3

# avvio il client
./client -f mipiacequestonome.sk -p -t 200 -d TestDirectory/output/Client/salvati -D TestDirectory/output/Client/flushati -W TestDirectory/tests/a/b/foto.jpg,TestDirectory/tests/a/b/test3.txt -w TestDirectory/tests/a/c -R 3 -c TestDirectory/tests/a/c/test4.txt -W TestDirectory/tests/a/test.txt -r TestDirectory/tests/a/test.txt

# aspetto il client
wait

# terminazione lenta al server
pkill -SIGHUP -f server

# aspetto il termine
sleep 2

# ottengo il numero di errori
r=$(tail -10 TestDirectory/output/Server/valgrind_output.txt | grep "ERROR SUMMARY" | cut -d: -f 2 | cut -d" " -f 2)

# se il numero di errori è diverso da 0 allora non va bene
if [[ $r != 0 ]]; then
    echo "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    echo "xxxxxxxxxxxxxx TEST 1 FALLITO xxxxxxxxxxxxxx"
    echo "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    exit 1
fi

exit 0