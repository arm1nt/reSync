#include "socket.h"

void
set_socket_timeout(const int socket_fd, const long sec, const long usec)
{
    struct timeval timeout = {.tv_sec = sec, .tv_usec = usec};

    const int ret = setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
    if (ret < 0) {
        fatal_custom_error("setsockopt(SO_RCVTIMEO) failed: %s", strerror(errno));
    }
}

int
create_unix_server_socket(const char *socket_path)
{
    create_unix_server_socket_with_opts(socket_path, SOCK_STREAM, DEFAULT_UNIX_SOCKET_BACKLOG_SIZE);
}

int
create_unix_server_socket_with_opts(const char *socket_path, const int socket_type, const int backlog_size)
{
    int ret, server_fd;
    struct sockaddr_un server_addr;

    server_fd = socket(AF_UNIX, socket_type, 0);
    if (server_fd == -1) {
        fatal_error("socket");
    }

    memset(&server_addr, 0, sizeof server_addr);
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, socket_path, sizeof(server_addr.sun_path) - 1);

    // In case a previous cleanup failed, remove the (possibly still existing) socket file
    unlink(socket_path);

    ret = bind(server_fd, (const struct sockaddr_un *) &server_addr, sizeof server_addr);
    if (ret == -1) {
        fatal_error("bind");
    }

    ret = listen(server_fd, backlog_size);
    if (ret == -1) {
        unlink(socket_path);
        fatal_error("listen");
    }

    return server_fd;
}

int
create_unix_client_socket(const char *socket_path)
{
    int ret, socket_fd;
    struct sockaddr_un addr;

    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        fatal_error("socket");
    }

    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    ret = connect(socket_fd, (const struct sockaddr_un *) &addr, sizeof addr);
    if (ret == -1) {
        fatal_error("connect");
    }

    return socket_fd;
}

