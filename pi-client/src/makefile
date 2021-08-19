# Makefile

CC=gcc
CFLAGS=-g -Wall

INCLUDE = -I/usr/local/include
LDFLAGS=-lcurl -lpthread -lpigpio
OBJECTS=client.o request.o uart.o
TARGET=client

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

clean:
	$(RM) ./$(TARGET) *.o *~

