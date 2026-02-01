CC = gcc
CFLAGS = -Wall -Wextra -g

TARGET = testlib

SRC = hash.c main.c list.c bitmap.c debug.c hex_dump.c
OBJ = $(SRC:.c=.o)

DEPS =  hash.h list.h bitmap.h debug.h hex_dump.h limits.h round.h

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
