CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -g
SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)
TARGET = bin/basic-interpreter-c

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(OBJS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: all
	$(TARGET)

test: all
	@echo "No tests defined. Add test targets as needed."

clean:
	rm -rf $(TARGET) $(OBJS)
