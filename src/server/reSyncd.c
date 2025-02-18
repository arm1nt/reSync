#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "config.h"
#include "../socket.h"

#define COMMAND_SOCKET_PATH "/tmp/reSync_cmd.socket"
#define COMMAND_BUFFER_CHUNK_SIZE ((ssize_t)(2048 * sizeof(char)))

#define DEFAULT_RCV_TIMEOUT_SEC 2
#define DEFAULT_RCV_TIMEOUT_USEC 0

#define WORKSPACE_MONITOR_EXECUTABLE "./linux/ws"

volatile sig_atomic_t terminate_daemon = 0;

static void
install_signal_handlers(void)
{
    // TODO:
}

static void
handle_request(char *cmd_buffer)
{
    //TODO:
}

static void
server_loop(void)
{
    const int command_server_socket = create_unix_server_socket(COMMAND_SOCKET_PATH);

    while (!terminate_daemon) {

        int client_fd = accept(command_server_socket, NULL, NULL);
        if (client_fd == -1) {
            unlink(COMMAND_SOCKET_PATH);
            fatal_error("accept");
        }

        // Specify a client socket timeout to prevent infinite waits if invalid input is provided
        set_socket_timeout(client_fd, DEFAULT_RCV_TIMEOUT_SEC, DEFAULT_RCV_TIMEOUT_USEC);

        ssize_t bytes_received;
        ssize_t received_cmd_buffer_chunks = 0;
        char *command_buffer = NULL;
        char buffer[COMMAND_BUFFER_CHUNK_SIZE];

        // '..chunk_size - 1 ' to ensure that the buffer is always 0 terminated by the memset operation
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
            DO_FREE(command_buffer);

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOG("Connection closed as daemon did not receive a valid command in a timely manner!");
                continue;
            }

            break;
        }

        handle_request(command_buffer);
        DO_FREE(command_buffer);
    }

    close(command_server_socket);
    unlink(COMMAND_SOCKET_PATH);
}

static void
start_workspace_monitor(ConfigFileEntryInformation *config_entry_info)
{
    const pid_t pid = fork();

    if (pid < 0) {
        fatal_custom_error(
                "Unable to create fs monitoring process for '%s': %s",
                config_entry_info->workspace_information->local_workspace_root_path,
                strerror(errno)
        );
    } else if (pid == 0) {

        if (setsid() == -1) {
            fatal_error("setsid");
        }

        const pid_t gc_pid = fork();
        if (gc_pid < 0) {
            fatal_error("fork");
        }

        if (gc_pid > 0) {
            exit(EXIT_SUCCESS);
        }

        if (gc_pid == 0) {
            execlp(
                    WORKSPACE_MONITOR_EXECUTABLE,
                    WORKSPACE_MONITOR_EXECUTABLE,
                    config_entry_info->ws_information_json_string,
                    (char *) NULL
            );

            fatal_custom_error(
                    "'execlp' failed for workspace '%s': %s",
                    config_entry_info->workspace_information->local_workspace_root_path,
                    strerror(errno)
            );
        }
    }

    // TODO: map pid -> workspace
}

static void
start_workspace_monitors(ConfigFileEntryInformation *workspaces)
{
    ConfigFileEntryInformation *entry;
    LL_FOREACH(workspaces, entry) {
        start_workspace_monitor(entry);
    }
}

int
main(const int argc, const char **argv)
{
    install_signal_handlers();

    // Parse configuration file and start ws monitoring process for each workspace to be synced
    ConfigFileEntryInformation *config_file_entries = parse_config_file();
    start_workspace_monitors(config_file_entries);

    daemon(0, 0);

    // Start listen for incoming commands and handle them
    server_loop();

    return EXIT_SUCCESS;
}
