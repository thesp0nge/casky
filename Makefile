# Compiler e flags
CC = gcc
CFLAGS = -Wall -Wextra -I./src

# File sorgenti e test
SRC = src/casky.c
TEST = tests/test_casky.c
TEST_BIN = test_casky

.PHONY: all test clean

# Compilazione solo libreria (opzionale)
all: $(SRC)
	$(CC) $(CFLAGS) -c $(SRC)

# Compilazione ed esecuzione test
test: $(SRC) $(TEST)
	$(CC) $(CFLAGS) -o $(TEST_BIN) $(SRC) $(TEST)
	./$(TEST_BIN)

# Pulizia dei binari
clean:
	rm -f $(TEST_BIN) *.o

