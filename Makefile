CC = gcc
CFLAGS = -Iinclude -Wall
OBJ = src/main.o src/commit.o src/repository.o src/hash.o
TARGET = mvcs

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)
