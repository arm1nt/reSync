#include "options.h"
#include "../socket.h"

#include <stdio.h>
#include <stdlib.h>

static void
usage(void)
{
    LOG_ERROR("\nUsage: ./resync --command COMMAND --local-ws-path PATH --remote-ws-path PATH --connection-type "
              "{"
              "SSH --hostname HOSTNAME [--username USERNAME] [--identity-file IDENTITY_FILE_PATH]"
              " | SSH_HOST_ALIAS --ssh-host-alias ALIAS"
              " | RSYNC_DAEMON --hostname HOSTNAME [--username USERNAME] [--port PORT_NUMBER]"
              "}");

    LOG_ERROR("\nOptions:");
    LOG_ERROR("\t %s :\t %s", OPT_USAGE_COMMAND, OPT_DESCRIPTION_COMMAND);
    LOG_ERROR("\t %s :\t %s", OPT_USAGE_LOCAL_WS_PATH, OPT_DESCRIPTION_LOCAL_WS_PATH);
    LOG_ERROR("\t %s :\t %s", OPT_USAGE_REMOTE_WS_PATH, OPT_DESCRIPTION_REMOTE_WS_PATH);
    LOG_ERROR("\t %s :\t %s", OPT_USAGE_CONNECTION_TYPE, OPT_DESCRIPTION_CONNECTION_TYPE);
    LOG_ERROR("\t %s :\t %s", OPT_USAGE_SSH_HOST_ALIAS, OPT_DESCRIPTION_SSH_HOST_ALIAS);
    LOG_ERROR("\t %s :\t %s", OPT_USAGE_HOSTNAME, OPT_DESCRIPTION_HOSTNAME);
    LOG_ERROR("\t %s :\t %s", OPT_USAGE_USERNAME, OPT_DESCRIPTION_USERNAME);
    LOG_ERROR("\t %s :\t %s", OPT_USAGE_IDENTITY_FILE, OPT_DESCRIPTION_IDENTITY_FILE);
    LOG_ERROR("\t %s :\t\t %s", OPT_USAGE_PORT, OPT_DESCRIPTION_PORT);

    exit(EXIT_FAILURE);
}

static void
send_cmd_to_daemon(char *stringified_command)
{
    int socket_fd = create_unix_client_socket(DEFAULT_RESYNC_DAEMON_SOCKET_PATH);

    write(socket_fd, stringified_command, strlen(stringified_command));
    write(socket_fd, "\r\n", strlen("\r\n"));

    char buffer[256];
    size_t num_bytes;

    while (1) {
        num_bytes = read(socket_fd, buffer, sizeof(buffer));

        if (num_bytes <= 0) {
            break;
        }

        fprintf(stdout, "%s", buffer);
    }

    close(socket_fd);
}

int
main(const int argc, char **argv)
{
    ResyncDaemonCommand *command = parse_options(argc, argv);
    if (command == NULL) {
        usage();
    }

    char *error_msg;
    char *stringified_command = resync_daemon_command_to_stringified_json(command, &error_msg);
    if (stringified_command == NULL) {
        if (error_msg != NULL) {
            fatal_custom_error(error_msg);
        } else {
            fatal_custom_error("There was an error processing the specified options!");
        }
    }

    send_cmd_to_daemon(stringified_command);

    return EXIT_SUCCESS;
}
