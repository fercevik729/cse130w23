#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "globals.h"
#include "asgn2_helper_funcs.h"

/** @brief Creates a Response struct for a GET Request
 * 
 * @param content_length integer representing content length
 * 
 * @return pointer to a new Response struct
*/
Response *create_get_response(long long content_length) {
    Response *resp = calloc(1, sizeof(Response));
    resp->status_line = strdup("200 OK\r\n");
    resp->content_length = content_length;

    return resp;
}

/** @brief Creates a Response struct
 * 
 * @param status_code integer representing a status code
 * 
 * @return pointer to a new Response struct
*/
Response *create_response(int status_code) {
    Response *resp = calloc(1, sizeof(Response));
    switch (status_code) {
    case 200:
        resp->status_line = strdup("200 OK\r\n");
        resp->msg = strdup("OK\n");
        break;
    case 201:
        resp->status_line = strdup("201 Created\r\n");
        resp->msg = strdup("Created\n");
        break;
    case 400:
        resp->status_line = strdup("400 Bad Request\r\n");
        resp->msg = strdup("Bad Request\n");
        break;
    case 403:
        resp->status_line = strdup("403 Forbidden\r\n");
        resp->msg = strdup("Forbidden\n");
        break;
    case 404:
        resp->status_line = strdup("404 Not Found\r\n");
        resp->msg = strdup("Not Found\n");
        break;
    case 500:
        resp->status_line = strdup("500 Internal Server Error\r\n");
        resp->msg = strdup("Internal Server Error\n");
        break;
    case 501:
        resp->status_line = strdup("501 Not Implemented\r\n");
        resp->msg = strdup("Not Implemented\n");
        break;
    default:
        resp->status_line = strdup("505 Version Not Supported\r\n");
        resp->msg = strdup("Version Not Supported\n");
    }
    resp->content_length = strlen(resp->msg);
    return resp;
}

/**
 * @brief Given a pointer to a Response struct, it frees it
 * and sets the pointer to NULL
 * 
 * @param resp Pointer to Response struct
*/
void delete_response(Response *resp) {
    free(resp->status_line);
    if (resp->msg)
        free(resp->msg);
    free(resp);
    resp = NULL;
}

/**
 * @brief Writes a Response to a file desciptor/socket
 * 
 * @param resp Pointer to Response struct
 * 
 * @param outfd File descriptor/socket to output to
*/
void write_response(Response *resp, int outfd) {
    dprintf(
        outfd, "HTTP/1.1 %sContent-Length: %lld\r\n\r\n", resp->status_line, resp->content_length);
    if (resp->msg)
        dprintf(outfd, "%s", resp->msg);
}

/**
 * @brief Creates a Request struct and returns a pointer to it
 *  
 * @param cmd COMMAND
 * 
 * @param fs FILE_STATUS
 * 
 * @param content_length number of bytes in message body
 * 
 * @param in_fd socket from which the request originates from
 * 
 * @param resource URI to resource
 * 
 * @return pointer to a new Request struct
*/
Request *create_request(
    COMMAND cmd, FILE_STATUS fs, long long content_length, int in_fd, char *resource) {
    Request *req = calloc(1, sizeof(Request));
    req->cmd = cmd;
    req->fs = fs;
    req->content_length = content_length;
    req->in_fd = in_fd;
    req->resource = strdup(resource);
    return req;
}

/**
 * @brief Given a pointer to a Request struct, it frees it
 * and sets the pointer to NULL
 * 
 * @param req Pointer to Request struct
*/
void delete_request(Request *req) {
    free(req->resource);
    free(req);
    req = NULL;
}

/// @brief Handles a GET request
/// @param req ptr to Request struct
void handle_get(Request *req) {
    int fd = open(req->resource, O_RDONLY);
    if (fd < 0) {
        Response *resp = create_response(500);
        write_response(resp, req->in_fd);
        perror("File descriptor in handle_get() was invalid: ");

        // Cleanup
        delete_request(req);
        delete_response(resp);
        return;
    }
    // Get file size
    struct stat st;
    fstat(fd, &st);
    size_t file_size = st.st_size;
    // Create response
    Response *resp = create_get_response(file_size);
    write_response(resp, req->in_fd);
    // Pass bytes from resource to socket
    pass_bytes(fd, req->in_fd, file_size);

    // Cleanup
    close(fd);
    delete_response(resp);
    delete_request(req);
}

/// @brief Handles a PUT request
/// @param req ptr to Request struct
void handle_put(Request *req, char *extra, long extra_size) {
    // Handle PUT Request
    int fd = open(req->resource, O_TRUNC | O_WRONLY);
    if (fd < 0) {
        Response *resp = create_response(500);
        write_response(resp, req->in_fd);
        perror("File descriptor in handle_put() was invalid: ");

        // Cleanup
        delete_request(req);
        delete_response(resp);
        return;
    }
    Response *resp = create_response(200 + req->fs);
    write_response(resp, req->in_fd);
    delete_response(resp);
    long long ret = 0;
    // If extra exists
    if (extra_size > 0) {
        ret = write_all(fd, extra, extra_size);
    }
    // Error checking
    if (ret < 0) {
        Response *r = create_response(500);
        write_response(r, req->in_fd);
        close(fd);
        delete_response(r);
        delete_request(req);
        perror("Failed to write to file: ");
        return;
    }
    ret = pass_bytes(req->in_fd, fd, req->content_length - extra_size);
    // Error checking
    if (ret < 0) {
        Response *r = create_response(500);
        write_response(r, req->in_fd);
        close(fd);
        delete_response(r);
        delete_request(req);
        perror("Failed to write to file: ");
        return;
    }
    close(fd);
    // Cleanup
    delete_request(req);
}

/// @brief Handles an HTTP request
/// @param req ptr to Request struct
/// @param extra extra content from msg body that was read
void handle_request(Request *req, char *extra, long extra_size) {
    // Handle GET request
    fprintf(stderr, "Handling request...\n");
    if (req->cmd == GET) {
        handle_get(req);
    } else if (req->cmd == PUT) {
        handle_put(req, extra, extra_size);
    }
}
