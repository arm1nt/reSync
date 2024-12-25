#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/un.h>
#include <sys/socket.h>

#include "util/util.h"

#define COMMAND_SOCKET_PATH "/tmp/reSync_cmd.socket"
#define COMMAND_SOCKET_BACKLOG_SIZE 20
#define COMMAND_BUFFER_CHUNK_SIZE ((ssize_t)(2048 * sizeof(char)))

#define DEFAULT_RCV_TIMEOUT_SEC 2
#define DEFAULT_RCV_TIMEOUT_USEC 0

volatile sig_atomic_t terminate_daemon = 0;

static void
install_signal_handlers(void)
{
    // TODO:
}

static void
set_so_timeout(const int fd)
{
    struct timeval timeout = {.tv_sec = DEFAULT_RCV_TIMEOUT_SEC, .tv_usec = DEFAULT_RCV_TIMEOUT_USEC};

    const int ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (ret < 0) {
        unlink(COMMAND_SOCKET_PATH);
        fatal_error("setsockopt(SO_RCVTIMEO)");
    }
}

static int
create_server_socket(void)
{
    int ret, server_fd;
    struct sockaddr_un server_addr;

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        fatal_error("socket");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, COMMAND_SOCKET_PATH, sizeof(server_addr.sun_path)-1);

    // In case a previous cleanup failed, remove the (possibly existing) socket file
    unlink(COMMAND_SOCKET_PATH);

    ret = bind (server_fd, (const struct sockaddr_un *) &server_addr, sizeof(server_addr));
    if (ret == -1) {
        fatal_error("bind");
    }

    ret = listen(server_fd, COMMAND_SOCKET_BACKLOG_SIZE);
    if (ret == -1) {
        unlink(COMMAND_SOCKET_PATH);
        fatal_error("listen");
    }

    return server_fd;
}

static void
server_loop(void)
{
    const int command_server_socket = create_server_socket();

    while (!terminate_daemon) {

        int client_fd = accept(command_server_socket, NULL, NULL);
        if (client_fd == -1) {
            unlink(COMMAND_SOCKET_PATH);
            fatal_error("accept");
        }

        // Specify a client socket timeout to prevent infinite waits if invalid input is provided
        set_so_timeout(client_fd);

        ssize_t bytes_received;
        ssize_t received_cmd_buffer_chunks = 0;
        char *command_buffer = NULL;
        char buffer[COMMAND_BUFFER_CHUNK_SIZE];

        // '..chunk_size -1 ' to ensure that the buffer is always 0 terminated by the memset operation
        while ((bytes_received = recv(client_fd, buffer, COMMAND_BUFFER_CHUNK_SIZE-1, 0)) > 0) {
            received_cmd_buffer_chunks++;

            command_buffer = (char *) realloc(command_buffer, received_cmd_buffer_chunks * COMMAND_BUFFER_CHUNK_SIZE);
            if (command_buffer == NULL) {
                unlink(COMMAND_SOCKET_PATH);
                fatal_error("realloc");
            }
            const ssize_t offset = COMMAND_BUFFER_CHUNK_SIZE * (received_cmd_buffer_chunks - 1);
            memset(command_buffer + offset, 0, COMMAND_BUFFER_CHUNK_SIZE);

            // Search for a newline character as that signifies the end of the command
            const char *newline_position = strchr(buffer, '\n');
            if (newline_position != NULL) {
                int cmd_end_index = (int) (newline_position - buffer);
                strncat(command_buffer, buffer, cmd_end_index);
                break;
            }

            // strncat appends a nullbyte in any case
            strncat(command_buffer, buffer, bytes_received);
        }

        close(client_fd);

        if (bytes_received < 0) {
            do_free(&command_buffer);

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                L("Connection closed as daemon did not receive a valid command in a timely manner!");
                continue;
            }

            break;
        }

        // TODO: process command - validate the received command and, if valid, perform the requested operation
        L(command_buffer);

        do_free(&command_buffer);
    }

    close(command_server_socket);
    unlink(COMMAND_SOCKET_PATH);
}

int
main(const int argc, const char **argv)
{
    install_signal_handlers();

    server_loop();

    return EXIT_SUCCESS;
}
