#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <errno.h>
#include "memory.h"

// Helper functions to indicate failure
int fail() {
    fprintf(stderr, "Invalid Command\n");
    return EXIT_FAILURE;
}

int fail2() {
    fprintf(stderr, "Operation Failed\n");
    return 1;
}

// FILE FUNCTIONS
// Function to abstract file opening and/or creating files
int open_file(char *filename, bool is_set) {
    // If the command is not set open the file in read only mode
    // if it doesn't exist this should return -1
    if (!is_set) {
        return open(filename, O_RDONLY, 0);
    }

    // If the command is set, setup the file (create it if needed, otherwise clear it)
    // Then return the fd for it
    return open(filename, O_CREAT | O_TRUNC | O_WRONLY, 0666);
}

// I/O Functions
// Reads header of the command: <command-type> <location>\n
char *read_command(char *extra) {
    char *comm = calloc(PATH_MAX + 4, sizeof(char));
    int bytes_read = 0;
    char temp[1];
    // Read one char at a time from stdin
    bool new_line_exists = false;
    while (read(0, temp, 1) > 0) {
        // If more bytes are read than the maximum filename + 4 chars (3 for command and 1 for space)
        // Return NULL
        if (bytes_read > PATH_MAX + 4) {
            free(comm);
            return NULL;
        }
        if (temp[0] == '\n') {
            new_line_exists = true;
            break;
        }
        // If newline character is read, the entire header should've been read
        // and loop can be terminated
        comm[bytes_read] = temp[0];
        bytes_read++;
    }
    // If new line doesn't exist return NULL
    if (!new_line_exists) {
        free(comm);
        return NULL;
    }
    // Read one past the end
    read(0, extra, 1);
    extra[1] = 0;
    return comm;
}

// Returns a pointer to a Request struct, provided the command header
Request *parse_command(char *comm, char *extra) {
    COMMAND c = None;
    char *com, *fn;
    char *saveptr;
    // Check if command starts with a space
    if (comm && comm[0] == ' ')
        return NULL;
    com = strtok_r(comm, " ", &saveptr);
    // Get the command type
    if (!strcmp(com, "get")) {
        c = Get;
    } else if (!strcmp(com, "set")) {
        c = Set;
    }
    // Invalid command
    if (c == None)
        return NULL;

    // Get sholdn't have any extra fields after the header
    if (c == Get && extra[0] != 0)
        return NULL;
    fn = strtok_r(NULL, "\n", &saveptr);
    // No filename
    if (!fn)
        return NULL;
    // Filename starts with space
    if (fn[0] == ' ')
        return NULL;
    int fd = open_file(fn, c == Set);
    // File doesn't exist/couldn't be opened
    if (fd < 0)
        return NULL;

    Request *req = calloc(1, sizeof(Request));
    req->c = c;
    if (req->c == Set) {
        req->rs = STDIN_FILENO;
        req->wd = fd;
    } else {
        req->rs = fd;
        req->wd = STDOUT_FILENO;
    }
    return req;
}

int read_write(Request *req, char *extra) {
    // Adapted from Professor Andrew Quinn's January 18th Lecture
    int bytes_read = 0;
    int rs = req->rs;
    int wd = req->wd;
    char buf[BUF_SIZE];

    // Try to write the 1 character in the extra buffer if the command is set
    if (req->c == Set) {
        if (extra[0] != 0) {
            // Check for errors
            if (write(wd, extra, 1) <= 0) {
                return fail2();
            }
            // If no field exists, do nothing and return
        } else {
            return 0;
        }
    }

    // Courtesy of Professor Quinn
    do {
        // Read bytes from read-src
        bytes_read = read(rs, buf, BUF_SIZE);
        // Error checking
        if (bytes_read < 0) {
            return fail();
        }
        // Attempt to write all bytes to write-dest
        else if (bytes_read > 0) {
            int bytes_wrote = 0;
            do {
                int bytes = write(wd, buf + bytes_wrote, bytes_read - bytes_wrote);
                if (bytes <= 0) {
                    return fail2();
                }
                bytes_wrote += bytes;
            } while (bytes_wrote < bytes_read);
        }
    } while (bytes_read > 0);

    return EXIT_SUCCESS;
}

int main() {
    // Prepare Input
    char extraBuf[2] = { 0, 0 };
    char *comm = read_command(extraBuf);
    if (!comm)
        return fail();

    // Create request struct
    Request *req = parse_command(comm, extraBuf);
    free(comm);
    // Error checking
    if (!req) {
        // If it's a permission issue return 'Operation Failed'
        if (errno == EACCES)
            return fail2();
        return fail();
    }

    // Main logic + Final bit of error checking
    int res = read_write(req, extraBuf);
    // Close read-src if command is 'get'
    if (req->c == Get) {
        close(req->rs);
    } else {
        close(req->wd); // closing write destination fd results in a segfault
        printf("OK\n");
    }

    free(req);
    return res;
}
