CC=gcc
CFLAGS=-I.. -Iunity -g -O0 -Wall -Wextra
SRCS=unity/unity.c main.c test_pmm.c test_kheap.c ../kernel/mm/pmm.c ../kernel/mm/kheap.c
TARGET=run_tests

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@

.PHONY: test

test: all
	./$(TARGET)

.PHONY: clean

clean:
	rm -f $(TARGET) *.o
