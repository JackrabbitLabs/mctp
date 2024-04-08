# SPDX-License-Identifier: Apache-2.0
# ******************************************************************************
#
# @file			Makefile
#
# @brief        Makefile for MCTP library
#
# @copyright    Copyright (C) 2024 Jackrabbit Founders LLC. All rights reserved.
#
# @date         Mar 2024
# @author       Barrett Edwards <code@jrlabs.io>
#
# ******************************************************************************

CC=gcc
CFLAGS?= -g3 -O0 -Wall -Wextra
MACROS?=-D MCTP_VERBOSE
INCLUDE_DIR?=/usr/local/include
LIB_DIR?=/usr/local/lib
INCLUDE_PATH=-I $(INCLUDE_DIR) 
LIB_PATH=-L $(LIB_DIR)
LIBS=-l uuid -l ptrqueue -l arrayutils -l fmapi -l emapi -l timeutils
TARGET=mctp

all: lib$(TARGET).a

client: client.c main.o threads.o ctrl.o
	$(CC) $^ $(CFLAGS) $(MACROS) $(INCLUDE_PATH) $(LIB_PATH) $(LIBS) -o $@ 

server: server.c main.o threads.o ctrl.o
	$(CC) $^ $(CFLAGS) $(MACROS) $(INCLUDE_PATH) $(LIB_PATH) $(LIBS) -o $@ 

lib$(TARGET).a: main.o threads.o ctrl.o 
	ar rcs $@ $^

ctrl.o: ctrl.c main.o
	$(CC) -c $< $(CFLAGS) $(MACROS) $(INCLUDE_PATH) -o $@ 

threads.o: threads.c main.o
	$(CC) -c $< $(CFLAGS) $(MACROS) $(INCLUDE_PATH) -o $@ 

main.o: main.c main.h
	$(CC) -c $< $(CFLAGS) $(MACROS) $(INCLUDE_PATH) -o $@ 

clean:
	rm -rf ./*.o ./*.a server client

doc: 
	doxygen

install: lib$(TARGET).a main.h
	sudo cp lib$(TARGET).a $(LIB_DIR)/
	sudo cp main.h $(INCLUDE_DIR)/$(TARGET).h

uninstall:
	sudo rm $(LIB_DIR)/lib$(TARGET).a
	sudo rm $(INCLUDE_DIR)/$(TARGET).h

# List all non file name targets as PHONY
.PHONY: all clean doc install uninstall

# Variables 
# $^ 	Will expand to be all the sensitivity list
# $< 	Will expand to be the frist file in sensitivity list
# $@	Will expand to be the target name (the left side of the ":" )
# -c 	gcc will compile but not try and link 
