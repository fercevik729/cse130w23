#pragma once
#include "globals.h"

// PARSE_REGEX matches for request headers of the following form
// HTTPMETHOD /URI HTTP/1.1\r\nOptional-Header-Fields: asdsad\r\nContent-Length: 123123\r\nMoreOptional-HeaderFields\r\n\r\n
// Invalid Method = Not Implemented
// Invalid Version = Version Not Supported
// Anything else invalid = Bad Request
#define PARSE_REGEX                                                                                \
    "^([A-Z]{3,8}) (/[a-zA-Z0-9._]{1,63}) (HTTP/[0-9][.][0-9])\r\n([a-zA-Z0-9.-]{1,128}: [ "       \
    "-~]{1,128}\r\n)*\r\n$"

void run_tests();
long long read_request(int fd, char *buf1, size_t buf1size);
Request *parse_request_header(char *buf, int fd);
int validate_method(char *buf);
void test_validate_method();
int validate_resource(char *method, char *resource);
void test_validate_resource();
int validate_version(char *version);
void test_validate_version();
long long validate_content_length(char *content_length);
void test_validate_content_length();
