CC = gcc

CFLAGS = -Wall -Wextra -O3 -g -pthread 
LDLIBS = -pthread

TARGET = msf.out

OBJS = main.o xerrori.o

.PHONY: all clean run valgrind

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)


main.o: main.c grafo.h xerrori.h debug.h
	$(CC) $(CFLAGS) -c main.c -o main.o

xerrori.o: xerrori.c xerrori.h
	$(CC) $(CFLAGS) -c xerrori.c -o xerrori.o

run: $(TARGET)
	./$(TARGET) grafo.gr

valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET) grafo.gr

clean:
	rm -f $(OBJS) $(TARGET)
