CC       = clang
FORMAT   = clang-format
CFLAGS   = -Wall -Wpedantic -Werror -Wextra

all: queue.o

queue.o: queue.c queue.h 
	$(CC) $(CFLAGS) -c -pthread queue.c

clean:
	rm -f queue.o
