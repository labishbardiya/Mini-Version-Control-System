CC = cc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude

SRC = src/cs.c
OBJ = $(SRC:.c=.o)

all: cs

cs: $(OBJ)
	$(CC) $(CFLAGS) -o cs $(OBJ)

clean:
	rm -f $(OBJ) cs

.PHONY: all clean


