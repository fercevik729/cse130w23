#include <stdlib.h>
#define BUF_SIZE 20000

typedef enum { None, Get, Set } COMMAND;

typedef struct {
    COMMAND c;
    int rs; // read source
    int wd; // write destination
} Request;

int fail();

int fail2();

size_t read_file(int fd);

size_t write_file(int fd, char *contents, size_t size);
