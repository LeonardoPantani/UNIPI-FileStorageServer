#!/bin/bash
# FILE CHE STAMPA LE STATISTICHE FORMATTATE
#
#
NOMESCRIPT=${0##*/}

# se non ci sono argomenti
if [ $# = 0 ]; then
    echo -e "${NOMESCRIPT} > Fornisci anche il nome di un file." 1>&2
    exit 1
fi

# scorro gli argomenti ricevuti
for argomento do
	if [ -f "$argomento" ] && [ -r $argomento ]; then
		FILESTATS=$argomento
    else
        echo "${NOMESCRIPT} > File \"$argomento\" inesistente o non hai i permessi di lettura." 1>&2
        exit 1
    fi
done

# se il file è vuoto stampo errore
if [ ! -s $NOMESCRIPT ]; then
	echo "${NOMESCRIPT} > Errore, file \"$FILESTATS\" vuoto." 1>&2
	exit 1
fi

# apro in lettura lo statfile
exec 4< $FILESTATS

# salvo le variabili
read -r -u 4 data avg_read avg_written n_read n_write n_lock n_opencreate n_unlock n_delete n_close max_size_reached max_file_number_reached n_replace_applied max_concurrent_connections blocked_connections

data=$(TZ=":Europe/Rome" date -d @${data});
# stampo a schermo i risultati
echo -e "\t======================================================"
echo -e "\t================ STATISTICHE SERVER =================="
echo -e "\t\t$data"
echo -e "\t======================================================"

echo -e "READ MEDIA\tWRITE MEDIA\t#READ\t#WRITE\t#LOCK\t#OPEN_CREATE\t#UNLOCK\t#DELETE\tCLOSE"
echo -e "$avg_read bytes\t$avg_written bytes\t$n_read\t$n_write\t$n_lock\t$n_opencreate\t\t$n_unlock\t$n_delete\t$n_close"
echo ''
echo -e "DIM. MAX OCCUPATA\t#FILE MAX\t#APPLICAZIONI ALGORITMO DI ESPULSIONE\t#CONNESSIONI CONCORRENTI"
echo -e "$(bc <<< "scale=6;$max_size_reached/1000000") MB\t\t$max_file_number_reached\t\t$n_replace_applied\t\t\t\t\t$max_concurrent_connections"
echo ''
echo -e "#CONNESSIONI BLOCCATE"
echo -e "$blocked_connections"
echo ''
echo -e "ID WORKER\t#RICHIESTE ESEGUITE"

read -r -u 4 riga
for worker in $riga; do
    nWorker=$(echo $worker | cut -d ':' -f1)
    nReq=$(echo $worker | cut -d ':' -f2)
    echo -e "$nWorker\t\t$nReq"
done
echo ''