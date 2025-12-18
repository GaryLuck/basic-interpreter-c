CC=gcc
CFLAGS=-Wall -Wextra -O2
TARGET=basic_interpreter

all: $(TARGET)

$(TARGET): src/basic.c
	$(CC) $(CFLAGS) -o $(TARGET) src/basic.c

clean:
	rm -f $(TARGET)