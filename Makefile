#
# @file		Makefile
# @brief	File di tipo makefile che compila e rimuove i file obsoleti.
# @author	Leonardo Pantani
#

PATH_UNIX_SOCKET = mipiacequestonome
PATH_OUTPUT = output


CC = gcc
CFLAGS += -std=c99 -Wall -pedantic -DDEBUG
INCLUDES = -I.
LDFLAGS = -L.
OPTFLAGS = -O3
LIBS = -pthread

TARGETS =	server \
			client

OBJECTS =	\ #todo

INCLUDE_FILES = \ #todo

.PHONY: all clean cleanall test1 test2 test3


all: $(TARGETS)

#######
clean:
	rm -f $(TARGETS)

cleanall: clean
	rm -f *.o *~ $(PATH_UNIX_SOCKET) $(PATH_OUTPUT)

test1:
	make cleanall
	make all

test2:
	make cleanall
	make all

test3:
	make cleanall
	make all