#
# @file		Makefile
# @brief	File di tipo makefile che compila e rimuove i file obsoleti.
# @author	Leonardo Pantani
#

# Bersagli phony (non sono nomi file ma una ricetta esplicita da eseguire)
.PHONY: all client server clean test1 test2 test3

CC = gcc
CFLAGS += -g -std=c99 -Wall
THREAD_FLAGS = -pthread
INCLUDES = -I./headers

# ------------ CARTELLE ------------ 
# Cartella dei file oggetto
OBJECT_FOLDER = build/objects
# Cartella delle librerie
LIB_FOLDER = build/libs
# Cartella delle api
API_FOLDER = sources/api
# Cartella delle strutture dati
DS_FOLDER = sources/data_structures


# Dipendenze server e client
server_dependencies = libs/libds.so libs/libserver.so libs/libcomm.so
client_dependencies = libs/libcomm.so libs/libapi.so

all: clean server client

server: $(server_dependencies)
	$(CC) $(INCLUDES) $(CFLAGS) $(THREAD_FLAGS) sources/server.c -o server -DDEBUG -Wl,-rpath,./build/libs -L ./build/libs -lds -lserver -lcomm

client: $(client_dependencies)
	$(CC) $(INCLUDES) $(CFLAGS) sources/client.c -o client -DDEBUG -Wl,-rpath,./build/libs -L ./build/libs -lcomm -lapi


libs/libserver.so: $(OBJECT_FOLDER)/config.o $(OBJECT_FOLDER)/statistics.o
	$(CC) -shared -o $(LIB_FOLDER)/libserver.so $^

libs/libapi.so: $(OBJECT_FOLDER)/utils.o $(OBJECT_FOLDER)/communication.o $(OBJECT_FOLDER)/api.o
	$(CC) -shared -o $(LIB_FOLDER)/libapi.so $^

libs/libds.so: $(OBJECT_FOLDER)/list.o $(OBJECT_FOLDER)/hashtable.o
	$(CC) -shared -o $(LIB_FOLDER)/libds.so $^

libs/libcomm.so: $(OBJECT_FOLDER)/utils.o $(OBJECT_FOLDER)/communication.o
	$(CC) -shared -o $(LIB_FOLDER)/libcomm.so $^



$(OBJECT_FOLDER)/api.o: sources/utils/communication.c sources/api/api.c
	$(CC) $(INCLUDES) $(CFLAGS) sources/api/api.c -c -fPIC -o $@

$(OBJECT_FOLDER)/communication.o: sources/utils/communication.c
	$(CC) $(INCLUDES) $(CFLAGS) sources/utils/communication.c -c -fPIC -o $@

$(OBJECT_FOLDER)/statistics.o: sources/utils/statistics.c
	$(CC) $(INCLUDES) $(CFLAGS) sources/utils/statistics.c -c -fPIC -o $@

$(OBJECT_FOLDER)/config.o:
	$(CC) $(INCLUDES) $(CFLAGS) sources/utils/config.c -c -fPIC -o $@

$(OBJECT_FOLDER)/utils.o: sources/utils/utils.c
	$(CC) $(INCLUDES) $(CFLAGS) sources/utils/utils.c -c -fPIC -o $@

$(OBJECT_FOLDER)/list.o:
	$(CC) $(INCLUDES) $(CFLAGS) sources/data_structures/list.c -c -fPIC -o $@

$(OBJECT_FOLDER)/hashtable.o:
	$(CC) $(INCLUDES) $(CFLAGS) sources/data_structures/hashtable.c -c -fPIC -o $@

#######
clean:
	rm -f -rf build
	rm -f -rf TestDirectory/output/Client/*.txt
	rm -f -rf TestDirectory/output/Client/flushati/*.*
	rm -f -rf TestDirectory/output/Client/salvati/*.*
	rm -f -rf TestDirectory/output/Server/*.txt
	rm -f *.sk
	rm -f server
	rm -f client
	mkdir build
	mkdir build/objects
	mkdir build/libs


test1:
	@$(MAKE) -s clean
	@$(MAKE) -s all
	@echo "*********************************************"
	@echo "*************** AVVIO TEST 1 ****************"
	@echo "*********************************************"
	@rm -f valgrind_output.txt
	@valgrind --leak-check=full ./server -conf TestDirectory/serverconfigs/config_1.txt > TestDirectory/output/Server/valgrind_output.txt 2>&1 &
	@bash scripts/test1.sh
	@echo "*********************************************"
	@echo "************** TEST 1 SUPERATO **************"
	@echo "*********************************************"

test2:
	@$(MAKE) -s clean
	@$(MAKE) -s all
	@echo "*********************************************"
	@echo "*************** AVVIO TEST 2 ****************"
	@echo "*********************************************"
	@./server -conf TestDirectory/serverconfigs/config_2.txt &
	@bash scripts/test2.sh
	@echo "*********************************************"
	@echo "************** TEST 2 SUPERATO **************"
	@echo "*********************************************"

test3:
	@$(MAKE) -s clean
	@$(MAKE) -s all
	@echo "*********************************************"
	@echo "*************** AVVIO TEST 3 ****************"
	@echo "*********************************************"
	@./server -conf TestDirectory/serverconfigs/config_3.txt &
	@bash scripts/test3.sh
	@echo "*********************************************"
	@echo "************** TEST 3 SUPERATO **************"
	@echo "*********************************************"