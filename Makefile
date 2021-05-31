#
# @file		Makefile
# @brief	File di tipo makefile che compila e rimuove i file obsoleti.
# @author	Leonardo Pantani
#

PATH_UNIX_SOCKET = mipiacequestonome
PATH_OUTPUT = output/* salvati/* flushati/*


CC = gcc
#CFLAGS += -std=c99 -Wall -Wpedantic -Wno-variadic-macros -DDEBUG
CFLAGS += -g -std=c99 -Wall
INCLUDES = -I.
LDFLAGS = -L.
OPTFLAGS = -O3
LIBS = -pthread

TARGETS =	server \
			client

.PHONY: all clean cleanall test1 test2 test3


%: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

all: $(TARGETS)


#######
clean:
	@rm -f $(TARGETS)

cleanall: clean
	@rm -f *.o *~ $(PATH_UNIX_SOCKET) $(PATH_OUTPUT)

test1:
	@$(MAKE) -s cleanall
	@$(MAKE) -s all
	@echo "*********************************************"
	@echo "*************** AVVIO TEST 1 ****************"
	@echo "*********************************************"
	@rm -f valgrind_output
	@valgrind --leak-check=full ./server -conf test1/config.txt > ./valgrind_output 2>&1 &
	@bash test1.sh
	@echo "*********************************************"
	@echo "************** TEST 1 SUPERATO **************"
	@echo "*********************************************"

test2:
	@$(MAKE) -s cleanall
	@$(MAKE) -s all
	@echo "*********************************************"
	@echo "*************** AVVIO TEST 2 ****************"
	@echo "*********************************************"
	@./server -conf test2/config.txt &
	@bash test2.sh
	@echo "*********************************************"
	@echo "************** TEST 2 SUPERATO **************"
	@echo "*********************************************"

test3:
	@$(MAKE) -s cleanall
	@$(MAKE) -s all
	@echo "*********************************************"
	@echo "*************** AVVIO TEST 3 ****************"
	@echo "*********************************************"
	@./server -conf test3/config.txt &
	@./test3.sh
	@echo "*********************************************"
	@echo "************** TEST 3 SUPERATO **************"
	@echo "*********************************************"