// Asgn 2: A simple HTTP server.
// By: Eugene Chou
//     Andrew Quinn
//     Brian Zhao

#include "asgn2_helper_funcs.h"
#include "connection.h"
#include "httpserver.h"
#include "response.h"
#include "request.h"
#include "queue.h"

#include <err.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include <sys/stat.h>

// Global connection queue
queue_t *conn_queue;

// Global file_creation lock
pthread_mutex_t file_creation_lock = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char **argv) {
    if (argc < 2 || argc == 3 || argc >= 5) {
        warnx("wrong arguments: %s [-t threads] port_num", argv[0]);
        fprintf(stderr, "usage: %s [-t threads] <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *endptr = NULL;
    size_t port = (size_t) strtoull(argv[argc - 1], &endptr, 10);
    if (endptr && *endptr != '\0') {
        warnx("invalid port number: %s", argv[argc - 1]);
        return EXIT_FAILURE;
    }
    // Parse command line args
    int c;
    long threads = 4;
    opterr = 0;
    while ((c = getopt(argc, argv, ":t:")) != -1) {
        switch (c) {
        case 't':
            endptr = NULL;
            threads = strtol(optarg, &endptr, 10);
            if ((endptr && *endptr != '\0') || threads <= 0) {
                warnx("invalid number of worker threads: %s", optarg);
                return EXIT_FAILURE;
            }
            break;
        default: fprintf(stderr, "usage: %s [-t threads] <port>\n", argv[0]); return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Expected a positive integer after option -t\n");
        exit(EXIT_FAILURE);
    }

    signal(SIGPIPE, SIG_IGN);
    Listener_Socket sock;
    int ret = listener_init(&sock, port);

    // Check the port value just in case
    if (ret) {
        fprintf(stderr, "Failed to listen on port %zu. Already in use\n", port);
        exit(EXIT_FAILURE);
    }

    // Create queue & thread pool
    conn_queue = queue_new(threads);
    thread_pool_new(threads);

    while (1) {
        int connfd = listener_accept(&sock);
        queue_push(conn_queue, (void *) (intptr_t) connfd);
    }

    return EXIT_SUCCESS;
}

/** @brief Writes to audit log in stderr given a ptr to a conn_t struct and a pointer to a Response_t struct
 * 
*/
void write_to_audit(conn_t *conn, const Response_t *res) {
    if (!conn || !res)
        return;
    char *header = conn_get_header(conn, "Request-Id");
    if (!header)
        header = "0";
    fprintf(stderr, "%s,%s,%u,%s\n", request_get_str(conn_get_request(conn)), conn_get_uri(conn),
        response_get_code(res), header);
}

void handle_connection(int connfd) {

    conn_t *conn = conn_new(connfd);

    const Response_t *res = conn_parse(conn);

    if (res != NULL) {
        write_to_audit(conn, res);
        conn_send_response(conn, res);
    } else {
        //debug("%s", conn_str(conn));
        const Request_t *req = conn_get_request(conn);
        if (req == &REQUEST_GET) {
            handle_get(conn);
        } else if (req == &REQUEST_PUT) {
            handle_put(conn);
        } else {
            handle_unsupported(conn);
        }
    }

    // Delete conn struct
    conn_delete(&conn);
}

void handle_get(conn_t *conn) {

    char *uri = conn_get_uri(conn);
    //debug("handling get request for %s", uri);
    const Response_t *res = NULL;

    // What are the steps in here?

    // 1. Open the file.
    pthread_mutex_lock(&file_creation_lock);
    pthread_mutex_unlock(&file_creation_lock);

    int fd = open(uri, O_RDONLY);
    // If  open it returns < 0, then use the result appropriately
    //   a. Cannot access -- use RESPONSE_FORBIDDEN
    //   b. Cannot find the file -- use RESPONSE_NOT_FOUND
    //   c. other error? -- use RESPONSE_INTERNAL_SERVER_ERROR
    // (hint: check errno for these cases)!
    if (fd < 0) {
        if (errno == EACCES) {
            res = &RESPONSE_FORBIDDEN;
            goto out_failed;
        }
        if (errno == ENOENT) {
            res = &RESPONSE_NOT_FOUND;
            goto out_failed;
        }
        res = &RESPONSE_INTERNAL_SERVER_ERROR;
        goto out_failed;
    }
    // Put a shared lock on the resource when GET-ing
    int ret = flock(fd, LOCK_SH);
    if (ret) {
        goto out_failed;
    }
    // 2. Get the size of the file.
    // (hint: checkout the function fstat)!
    struct stat st;
    fstat(fd, &st);
    size_t file_size = st.st_size;

    // 3. Check if the file is a directory, because directories *will*
    // open, but are not valid.
    // (hint: checkout the macro "S_IFDIR", which you can use after you call fstat!)
    if (S_IFDIR == (st.st_mode & S_IFMT)) {
        res = &RESPONSE_FORBIDDEN;
        goto out_failed;
    }

    // 4. Send the file
    // (hint: checkout the conn_send_file function!)
    res = conn_send_file(conn, fd, file_size);
    if (res == NULL) {
        res = &RESPONSE_OK;
    }

    // Close the file descriptor and remove the lock
    write_to_audit(conn, res);
    close(fd);
    return;

// Write an auxilliary response only if the response code is erroneous
out_failed:
    write_to_audit(conn, res);
    conn_send_response(conn, res);
    close(fd);
}

void handle_unsupported(conn_t *conn) {

    // send responses
    write_to_audit(conn, &RESPONSE_NOT_IMPLEMENTED);
    conn_send_response(conn, &RESPONSE_NOT_IMPLEMENTED);
}

void handle_put(conn_t *conn) {

    char *uri = conn_get_uri(conn);
    const Response_t *res = NULL;

    // Lock the fcl (Start of critical region)
    pthread_mutex_lock(&file_creation_lock);

    // Check if file already exists before opening it.
    bool existed = access(uri, F_OK) == 0;
    // Create the file atomically
    int fd;
    if (!existed) {
        fd = open(uri, O_CREAT | O_WRONLY, 0600);
    }
    // Otherwise wait to see if another thread is going to create the file if it doesn't exist already
    else {
        fd = open(uri, O_WRONLY, 0);
    }
    // Error checking
    if (fd < 0) {
        if (errno == EACCES || errno == EISDIR || errno == ENOENT) {
            res = &RESPONSE_FORBIDDEN;
            goto out;
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
            goto out;
        }
    }
    // Put exclusive flock
    if (flock(fd, LOCK_EX)) {
        res = &RESPONSE_INTERNAL_SERVER_ERROR;
        goto out;
    }

    pthread_mutex_unlock(&file_creation_lock);
    // Truncate the file
    ftruncate(fd, 0);

    res = conn_recv_file(conn, fd);
    if (res == NULL && existed) {
        res = &RESPONSE_OK;
    } else if (res == NULL && !existed) {
        res = &RESPONSE_CREATED;
    }

out:
    write_to_audit(conn, res);
    conn_send_response(conn, res);

    // Release this lock if it was held and an error arose
    pthread_mutex_unlock(&file_creation_lock);
    close(fd);
}

// THREAD POOL CODE
// thread_pool struct
struct thread_pool {
    size_t num_threads;
    pthread_t *workers;
};
// start_worker: main function that handles the request
// 1) While there is nothing in the queue block
// 2) Try to pop off a connection file descriptor
// 3) Handle any errors
// 4) Process request (wait until resource is usable) & write to stderr
// 6) Close connection
// 8) Repeat forever
void *start_worker() {
    int connfd;
    while (1) {
        queue_pop(conn_queue, (void **) &connfd);
        // Don't handle the connection if it was bad nor print anything to audit log (not expected)
        if (connfd < 0)
            continue;
        handle_connection(connfd);
        close(connfd);
    }

    return (void *) NULL;
}

/** @brief Dynamically allocates and initializes a new thread pool with a certain
 *         number of threads, an associated task queue, & a max buffer size
 *
 *  @param num_threads number of worker threads in the thread pool
 *
 */
thread_pool_t *thread_pool_new(size_t num_threads) {
    // Create thread_pool struct
    struct thread_pool *tp = calloc(1, sizeof(struct thread_pool));
    tp->num_threads = num_threads;
    tp->workers = calloc(num_threads, sizeof(pthread_t));

    // Start up the threads
    for (size_t i = 0; i < num_threads; i++)
        pthread_create(&tp->workers[i], NULL, start_worker, NULL);

    return tp;
}
