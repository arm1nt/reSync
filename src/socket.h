#ifndef RESYNC_SOCKET_H
#define RESYNC_SOCKET_H

#include "util/error.h"

#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>

#define DEFAULT_RESYNC_DAEMON_SOCKET_PATH "/tmp/reSync_cmd.socket"

#define DEFAULT_UNIX_SOCKET_BACKLOG_SIZE 20

int create_unix_server_socket(const char *socket_path);

int create_unix_server_socket_with_opts(const char *socket_path, const int socket_type, const int backlog_size);

int create_unix_client_socket(const char *socket_path);

void set_socket_timeout(const int socket_fd, const long sec, const long usec);

#endif //RESYNC_SOCKET_H
