#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>

int main(int argc, char **argv) {

    fprintf(stdout, "%d %s\n", argc, *argv);

    return 0;
}
