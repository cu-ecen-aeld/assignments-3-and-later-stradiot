CC=$(CROSS_COMPILE)gcc
CFLAGS= -pthread -g -Wall -Werror
OBJ=aesdsocket.o
PROG=aesdsocket

.PHONY: all clean

all: $(OBJ) aesdsocket

$(PROG): $(OBJ)
	$(CC) $(CFLAGS) $< -o $@ -lrt

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f ./*.o aesdsocket
