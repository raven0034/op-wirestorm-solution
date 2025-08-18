CC=gcc
CFLAGS=-Wall -Wextra -g
SRC = src/main.c
EXEC=proxy

.PHONY: all clean

all: $(EXEC)

$(EXEC): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(EXEC)

clean:
	rm -f $(EXEC)