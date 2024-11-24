CC?=$(CROSS_COMPILE)gcc
CFLAGS?= -g -Wall -Werror
LDFLAGS?= -lpthread -lrt
OBJ?=aesdsocket.o
TARGET?=aesdsocket

.PHONY: all clean

all: $(OBJ) aesdsocket

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f ./*.o aesdsocket
