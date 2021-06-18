#
# @file		Makefile
# @brief	File di tipo makefile che compila gli eseguibili, file oggetto, librerie statiche e rimuove i file obsoleti.
# @author	Leonardo Pantani
#

# ------------ CARTELLE ------------ 
# Cartella dei file oggetto
OBJECT_FOLDER = build/objects
# Cartella delle librerie
LIB_FOLDER = build/libs
# Cartella delle api
API_FOLDER = sources/api
# Cartella delle strutture dati
DS_FOLDER = sources/data_structures
# Cartella delle utils
UTL_FOLDER = sources/utils
# Cartella degli headers
HDR_FOLDER = headers


# ------------- ALTRO --------------
# Estensione del file socket da rimuovere da ovunque
SOCKET_EXTENSION = sk
# Bersagli phony (non sono nomi file ma una ricetta esplicita da eseguire)
.PHONY: all client server clean test1 test2 test3


# --------- COMPILAZIONE -----------
CC = gcc
CFLAGS += -g -std=c99 -Wall
CXXFLAGS += -DDEBUG -DDEBUG_VERBOSE
THREAD_FLAGS = -pthread
INCLUDES = -I./$(HDR_FOLDER)

# Dipendenze server e client
server_dependencies = libs/libds.so libs/libserver.so libs/libcomm.so
client_dependencies = libs/libcomm.so libs/libapi.so

all: clean server client

server: $(server_dependencies)
	$(CC) $(INCLUDES) $(CFLAGS) $(THREAD_FLAGS) sources/server.c -o server $(CXXFLAGS) -Wl,-rpath,./build/libs -L ./build/libs -lds -lserver -lcomm

client: $(client_dependencies)
	$(CC) $(INCLUDES) $(CFLAGS) sources/client.c -o client $(CXXFLAGS) -Wl,-rpath,./build/libs -L ./build/libs -lcomm -lapi


libs/libserver.so: $(OBJECT_FOLDER)/config.o $(OBJECT_FOLDER)/statistics.o
	$(CC) -shared -o $(LIB_FOLDER)/libserver.so $^

libs/libapi.so: $(OBJECT_FOLDER)/utils.o $(OBJECT_FOLDER)/communication.o $(OBJECT_FOLDER)/api.o
	$(CC) -shared -o $(LIB_FOLDER)/libapi.so $^

libs/libds.so: $(OBJECT_FOLDER)/node.o $(OBJECT_FOLDER)/list.o $(OBJECT_FOLDER)/hashtable.o
	$(CC) -shared -o $(LIB_FOLDER)/libds.so $^

libs/libcomm.so: $(OBJECT_FOLDER)/utils.o $(OBJECT_FOLDER)/communication.o
	$(CC) -shared -o $(LIB_FOLDER)/libcomm.so $^



$(OBJECT_FOLDER)/api.o: $(API_FOLDER)/api.c $(HDR_FOLDER)/api.h
	$(CC) $(INCLUDES) $(CFLAGS) $(API_FOLDER)/api.c -c -fPIC -o $@

$(OBJECT_FOLDER)/communication.o: $(UTL_FOLDER)/communication.c $(HDR_FOLDER)/communication.h
	$(CC) $(INCLUDES) $(CFLAGS) $(UTL_FOLDER)/communication.c -c -fPIC -o $@

$(OBJECT_FOLDER)/statistics.o: $(UTL_FOLDER)/statistics.c $(HDR_FOLDER)/statistics.h
	$(CC) $(INCLUDES) $(CFLAGS) $(UTL_FOLDER)/statistics.c -c -fPIC -o $@

$(OBJECT_FOLDER)/config.o: $(UTL_FOLDER)/config.c $(HDR_FOLDER)/config.h
	$(CC) $(INCLUDES) $(CFLAGS) $(UTL_FOLDER)/config.c -c -fPIC -o $@

$(OBJECT_FOLDER)/utils.o: $(UTL_FOLDER)/utils.c $(HDR_FOLDER)/utils.h
	$(CC) $(INCLUDES) $(CFLAGS) $(UTL_FOLDER)/utils.c -c -fPIC -o $@

$(OBJECT_FOLDER)/node.o: $(DS_FOLDER)/node.c $(HDR_FOLDER)/node.h
	$(CC) $(INCLUDES) $(CFLAGS) $(DS_FOLDER)/node.c -c -fPIC -o $@

$(OBJECT_FOLDER)/list.o: $(DS_FOLDER)/list.c $(HDR_FOLDER)/list.h
	$(CC) $(INCLUDES) $(CFLAGS) $(DS_FOLDER)/list.c -c -fPIC -o $@

$(OBJECT_FOLDER)/hashtable.o: $(DS_FOLDER)/hashtable.c $(HDR_FOLDER)/hashtable.h
	$(CC) $(INCLUDES) $(CFLAGS) $(DS_FOLDER)/hashtable.c -c -fPIC -o $@


# ------------- TARGET PHONY --------------
clean:
	rm -f -rf build
	rm -f -rf TestDirectory/output/Client/*.txt
	rm -f -rf TestDirectory/output/Client/flushati/*.*
	rm -f -rf TestDirectory/output/Client/salvati/*.*
	rm -f -rf TestDirectory/output/Server/*.txt
	rm -f *.$(SOCKET_EXTENSION)
	rm -f server
	rm -f client
	mkdir build
	mkdir $(OBJECT_FOLDER)
	mkdir $(LIB_FOLDER)


test1:
	@$(MAKE) -s clean
	@$(MAKE) -s all
	@echo "*********************************************"
	@echo "*************** AVVIO TEST 1 ****************"
	@echo "*********************************************"
	@rm -f valgrind_output.txt
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