# Makefile

CC=gcc
CFLAGS=-g -Wall

INCLUDE = -I/usr/local/include
LDFLAGS=-lcurl -lpthread -lpigpio
HDR=client.h request.h uart.h common.h
SRC=client.c request.c uart.c
#OBJECTS=client.o request.o uart.o
TARGET=client

all: $(TARGET) $(HDR)

OBJ=$(SRC:.c=.o)

$(TARGET): $(OBJ) $(HDR)
#	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

clean:
	$(RM) $(TARGET) $(OBJ) *~

