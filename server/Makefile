CC=$(CROSS_COMPILE)gcc
CFLAGS=-g -Wall -Werror

all: default

default: aesdsocket

aesdsocket: aesdsocket.o
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f aesdsocket *.o

