CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O3
LDFLAGS = -pthread

all: msf.out

msf.out: main.c xerrori.c grafo.h xerrori.h
	$(CC) $(CFLAGS) main.c xerrori.c -o msf.out $(LDFLAGS)

Msf.class: Msf.java
	javac Msf.java

clean:
	rm -f msf.out Msf.class