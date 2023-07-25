#pragma once

#define BUFSIZE       3072
#define REQUEST_LIMIT 2048

// ENUMS
typedef enum { GET, PUT } COMMAND;
typedef enum { OK, CREATED } FILE_STATUS;

// STRUCTS
typedef struct {
    COMMAND cmd;
    FILE_STATUS fs;
    char *resource;
    long long content_length;
    int in_fd;
} Request;

typedef struct {
    char *status_line;
    char *msg;
    long long content_length;
} Response;

// Response Helper Funcs
Response *create_get_response(long long content_length);
Response *create_response(int status_code);
void delete_response(Response *resp);
void write_response(Response *resp, int outfd);

// Request Helper Funcs
Request *create_request(
    COMMAND cmd, FILE_STATUS fs, long long content_length, int in_fd, char *resource);
void handle_request(Request *req, char *extra, long extra_size);
void delete_request(Request *req);
