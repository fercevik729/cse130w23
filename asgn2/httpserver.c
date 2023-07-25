#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <limits.h>

#include "asgn2_helper_funcs.h"
#include "parser.h"
#include "globals.h"

int main(int argc, char **argv) {

    // Parse command line arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: ./httpserver <port>\n");
        return 1;
    }
    int port = strtol(argv[1], NULL, 10);
    if (port < 1 || port > 65535) {
        fprintf(stderr, "Invalid Port\n");
        return 1;
    }

    // Bind socket to port
    Listener_Socket listener;
    int ret = listener_init(&listener, port);
    if (ret) {
        fprintf(stderr, "Failed to listen on port: %d\n", port);
        return EXIT_FAILURE;
    }

    // Listen for connections forever
    while (1) {
        int sock = listener_accept(&listener);
        if (sock == -1) {
            fprintf(stderr, "Couldn't connect to a socket\n");
            continue;
        }
        // Buffer to store request line + header fields
        char buf[BUFSIZE];
        memset(buf, 0, BUFSIZE);

        // Buffer to store any part of message body that got sucked into 'buf'
        char msg_content[BUFSIZE];
        memset(msg_content, 0, BUFSIZE);

        // Read in request header and extra data if it exists
        long long nbytes = read_request(sock, buf, BUFSIZE - 1);
        if (nbytes == -1) {
            Response *resp = create_response(400);
            write_response(resp, sock);
            delete_response(resp);
            close(sock);
            continue;
        }
        // Find where the message starts
        char *msg_start = strstr(buf, "\r\n\r\n");
        long long request_size = (msg_start - buf) + 4;
        long long extra_msg_size = nbytes - request_size;
        // Check request size
        if (request_size > REQUEST_LIMIT) {
            Response *resp = create_response(400);
            write_response(resp, sock);
            delete_response(resp);
            close(sock);
            continue;
        }
        // Copy extra message contents to msg_content if they exist
        if (extra_msg_size) {
            memcpy(msg_content, msg_start + 4, extra_msg_size);
            msg_content[extra_msg_size] = 0; // NULL Terminate
            *(msg_start + 4) = 0;
        }
        Request *req = parse_request_header(buf, sock);
        if (!req) {
            // Empty out anything after an invalid request
            char dummy[BUFSIZE];
            while (read_until(sock, dummy, BUFSIZE, NULL) > 0)
                ;
            close(sock);
            continue;
        }
        handle_request(req, msg_content, extra_msg_size);
        close(sock);
    }
}
