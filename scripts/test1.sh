#!/bin/bash
# FILE DI TEST N°1
#
#

# avvio il server con valgrind
valgrind --leak-check=full ./server -conf TestDirectory/serverconfigs/config_1.txt > TestDirectory/output/Server/valgrind_output.txt 2>&1 &

# mi salvo il pid ($! viene sostituito col PID del processo più recente avviato in background)
pid=$!

# attendo attivazione valgrind per 3 secondi
sleep 2

# avvio il client
./client -f mipiacequestonome.sk -p -t 200 -d TestDirectory/output/Client/salvati -D TestDirectory/output/Client/flushati -W TestDirectory/tests/a/b/foto.jpg,TestDirectory/tests/a/b/test3.txt -w TestDirectory/tests/a/c -R 3 -c TestDirectory/tests/a/c/test4.txt -W TestDirectory/tests/a/test.txt -r TestDirectory/tests/a/test.txt
./client -f mipiacequestonome.sk -p -t 200 -d TestDirectory/output/Client/salvati -D TestDirectory/output/Client/flushati -W TestDirectory/tests/a/b/foto.jpg,TestDirectory/tests/a/b/test3.txt -w TestDirectory/tests/a/c -R 3 -c TestDirectory/tests/a/c/test4.txt -W TestDirectory/tests/a/test.txt -r TestDirectory/tests/a/test.txt
./client -f mipiacequestonome.sk -p -t 200 -d TestDirectory/output/Client/salvati -D TestDirectory/output/Client/flushati -W TestDirectory/tests/a/b/foto.jpg,TestDirectory/tests/a/b/test3.txt -w TestDirectory/tests/a/c -R 3 -c TestDirectory/tests/a/c/test4.txt -W TestDirectory/tests/a/test.txt -r TestDirectory/tests/a/test.txt

# aspetto il termine
sleep 2

# terminazione lenta al server
kill -s SIGHUP $pid

# aspetto il server
wait $pid

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