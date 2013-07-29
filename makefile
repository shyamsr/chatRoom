COMPILER=gcc
CFLAGS=-Wall

all: server client

server:
	$(COMPILER) -g crsd.c -o crsd -pthread
client:
	$(COMPILER) -g crc.c -o crc 
clean:
	rm crsd crc
