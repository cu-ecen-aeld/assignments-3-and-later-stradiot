CC=$(CROSS_COMPILE)gcc
CFLAGS= -Wall -Werror

all: aesdsocket

default: aesdsocket

aesdsocket: aesdsocket.o

.PHONY: clean

clean:
	rm -f ./*.o aesdsocket
