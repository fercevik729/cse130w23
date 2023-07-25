#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <regex.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "asgn2_helper_funcs.h"
#include "globals.h"
#include "parser.h"

/// @brief Runs all unit tests
void run_tests() {
    test_validate_method();
    test_validate_resource();
    test_validate_version();
    test_validate_content_length();
}

/** @brief Returns a buffer containing the HTTP Request's request line and header fields terminated by double \r\n and potentially some of Message Content
 * 
 * @param fd socker from which to read the HTTP Request from
 * 
 * @param buf buffer to read into, will be null-terminated at the end
 * 
 * @param bufsize size of buffer
 * 
 * @return number of bytes read or -1 if timeout or Bad Request
*/
long long read_request(int fd, char *buf, size_t bufsize) {
    // Read from socket
    int nbytes = read_until(fd, buf, bufsize, "\r\n\r\n");
    if (nbytes < 1)
        return nbytes;

    // Check if request terminates with double \r\n
    if (!strstr(buf, "\r\n\r\n"))
        return -1;

    buf[nbytes] = 0;
    return nbytes;
}

/** @brief given a buffer containing '\r\n\r\n', parse request line and header fields using regex
 * 
 * @param buf buffer to parse
 * 
 * @param msg_content buffer containing first couple bytes of message body
 * 
 * @return pointer to Request struct
*/
Request *parse_request_header(char *buf, int fd) {
    // Copy request line and header fields
    regex_t re;
    regmatch_t matches[5];
    int rc;

    // Compile regex
    rc = regcomp(&re, PARSE_REGEX, REG_EXTENDED);
    if (rc)
        return NULL;

    // Execute regex on buffer
    rc = regexec(&re, buf, 5, matches, 0);
    char *cmd, *uri, *version, *content_length;
    if (rc == 0) {
        cmd = buf;
        uri = buf + matches[2].rm_so;
        version = buf + matches[3].rm_so;
        content_length = strstr(buf + matches[0].rm_so, "Content-Length");

        // Get all the matches, then validate them
        cmd[matches[1].rm_eo] = '\0';
        uri[matches[2].rm_eo - matches[2].rm_so] = '\0';
        version[matches[3].rm_eo - matches[3].rm_so] = '\0';

        // Get content length header if it exists
        if (content_length) {
            content_length[strcspn(content_length, "\r\n")] = '\0';
        }
    } else {
        regfree(&re);
        Response *resp = create_response(400);
        write_response(resp, fd);
        delete_response(resp);
        fprintf(stderr, "Bad Request, doesn't match regex\n");
        return NULL;
    }
    regfree(&re);

    // Creates request if everything was valid
    int ret;
    COMMAND method;
    FILE_STATUS fs;
    if ((ret = validate_method(cmd)) < 0) {
        Response *resp = create_response(501);
        write_response(resp, fd);
        delete_response(resp);
        fprintf(stderr, "Method Not Implemented\n");
        return NULL;
    }
    method = ret;

    // Validate version
    if (validate_version(version)) {
        Response *resp = create_response(505);
        write_response(resp, fd);
        delete_response(resp);
        fprintf(stderr, "Version Not Supported\n");
        return NULL;
    }

    // Validate and process resource if version and method are valid
    char resource[512] = ".";
    strcat(resource, uri);
    if ((ret = validate_resource(cmd, resource)) < 0) {
        Response *resp;
        if (ret == -1) {
            resp = create_response(404);
            fprintf(stderr, "Not Found\n");
        } else {
            resp = create_response(403);
            fprintf(stderr, "Unauthorized, invalid permissions or file, %s, is a dir\n", resource);
        }
        write_response(resp, fd);
        delete_response(resp);
        return NULL;
    }
    fs = ret;

    ssize_t cl = 0;
    if (content_length && (cl = validate_content_length(content_length)) < 0) {
        Response *resp = create_response(400);
        write_response(resp, fd);
        delete_response(resp);
        fprintf(stderr, "Bad Request, invalid content length\n");
        return NULL;
    }
    // PUT must have a content_length and message body
    if (method == PUT && !content_length) {
        Response *resp = create_response(400);
        write_response(resp, fd);
        fprintf(stderr, "Bad Request, invalid use of 'Content-Length' header\n");
        delete_response(resp);
        return NULL;
    }
    return create_request(method, fs, cl, fd, resource);
}

// VALIDATION HELPER FUNCS
/**
 * @brief Validates the HTTP Method of a Request
 * 
 * @param buf that contains the HTTP Method
 * 
 * @return an int, 0 if GET, 1 if PUT, -1 if invalid
*/
int validate_method(char *buf) {
    if (!strcmp("GET", buf))
        return 0;
    if (!strcmp("PUT", buf))
        return 1;
    return -1;
}

/// @brief Unit tests validate_method
void test_validate_method() {
    // Invalid methods
    assert(-1 == validate_method("PUTS"));
    assert(-1 == validate_method("PUT\n"));
    assert(-1 == validate_method("GETS"));
    assert(-1 == validate_method("GET\n"));
    assert(-1 == validate_method("IPUTS"));
    assert(-1 == validate_method("IGETS"));
    assert(-1 == validate_method("GETTER"));
    assert(-1 == validate_method("PUTTER"));

    // Valid methods
    assert(0 == validate_method("GET"));
    assert(1 == validate_method("PUT"));

    fprintf(stderr, "PASSED 10/10 for validate_method()\n");
}

/**
 * @brief Validates the resource of a Request
 * 
 * @param method that contains the HTTP Method (validated)
 *
 * @param resource that contains the URI to the resource
 * 
 * @return an int, 0 if exists, 1 if it got created, -1 if doesn't exist, -2 if wrong perms/isdir
*/
int validate_resource(char *method, char *resource) {
    int fd;
    struct stat st;
    stat(resource, &st);
    errno = 0;

    if (!strcmp(method, "PUT")) {
        fd = open(resource, O_TRUNC | O_WRONLY);
        if (fd < 0) {
            // If it cannot be accessed/isdir return -2
            if (errno == EACCES || errno == EISDIR)
                return -2;
            fd = open(resource, O_CREAT | O_RDWR, 0666);
            close(fd);
            return 1;
        }
    } else {
        fd = open(resource, O_RDONLY, 0);
    }

    // Error checking
    if (fd < 0 && errno == ENOENT)
        return -1;
    // Invalid perms but file exists
    if (fd < 0 && errno == EACCES)
        return -2;
    // File is a dir
    if (fd > 0 && S_ISDIR(st.st_mode)) {
        close(fd);
        return -2;
    }

    close(fd);
    return 0;
}

///@brief Unit tests validate resource
// TODO: update these
void test_validate_resource() {
    // Invalid GET's
    assert(-1 == validate_resource("GET", "./invalid.txt"));
    mkdir("invalid_dir", 0777);
    assert(-2 == validate_resource("GET", "./invalid_dir"));
    rmdir("invalid_dir");
    int fd = open("locked.txt", O_CREAT | O_WRONLY | O_TRUNC, 0222);
    close(fd);
    assert(-2 == validate_resource("GET", "./locked.txt"));
    remove("locked.txt");
    // Valid calls
    assert(0 == validate_resource("GET", "./foo.txt"));
    assert(1 == validate_resource("PUT", "./new.txt"));
    assert(0 == validate_resource("PUT", "./new.txt"));
    assert(0 == validate_resource("GET", "./new.txt"));
    remove("./new.txt");
    fprintf(stderr, "PASSED 6/6 for validate_resource()\n");
}
/**
 * @brief Validates the HTTP Version of a Request
 * 
 * @param version that contains the HTTP Version
 * 
 * @return an int, 0 if valid, 1 if invalid
*/
int validate_version(char *version) {
    if (!strcmp(version, "HTTP/1.1"))
        return 0;
    return 1;
}

void test_validate_version() {
    // Invalid versions
    assert(1 == validate_version("HTTP/1.2"));
    assert(1 == validate_version("HTTP/1.3"));
    assert(1 == validate_version("HTTP/2.1"));
    assert(1 == validate_version("HTTP/2.0"));

    // Valid version
    assert(0 == validate_version("HTTP/1.1"));
    fprintf(stderr, "PASSED 5/5 for validate_version()\n");
}

/**
 * @brief Validates the Content-Length Header of a Request and returns the value
 * 
 * @param content_length that contains the Content-Length value
 * 
 * @return -1 if invalid, content length if valid
*/
long long validate_content_length(char *content_length) {
    long long val;
    char temp[BUFSIZE];
    char *end_ptr;
    errno = 0;
    // Get ptr to ': ' and increment it by 2 to get to first digit of content length
    char *start = strstr(content_length, ": ") + 2;
    // Shouldn't happen
    if (!start)
        return -1;
    strcpy(temp, start);
    val = strtoll(temp, &end_ptr, 10);

    // Error checking
    if (errno != 0)
        return -1;
    if (end_ptr == temp)
        return -1;
    if (*end_ptr != '\0')
        return -1;
    return val;
}

void test_validate_content_length() {
    assert(-1 == validate_content_length("Content Length: hjko"));
    assert(-1 == validate_content_length("Content Length: h1ko"));
    assert(-1 == validate_content_length("Content Length: o1o"));
    assert(-1 == validate_content_length("Content Length: 1o9kol"));
    assert(-1 == validate_content_length("Content Length: 4000000000000000000000"));
    assert(-1 == validate_content_length("Content Length: "));
    assert(-1 == validate_content_length("Content Length: abc"));

    assert(12345 == validate_content_length("Content Length: 12345"));
    assert(87981 == validate_content_length("Content Length: 87981"));
    fprintf(stderr, "PASSED 9/9 for validate_content_length()\n");
}
