#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "config.h"
#include "../socket.h"

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

static bool
handle_add_workspace_request(ResyncDaemonCommand *cmd, char **error_msg)
{
    //TODO
    return true;
}

static bool
handle_add_remote_system_request(ResyncDaemonCommand *cmd, char **error_msg)
{
    //TODO
    return true;
}

static bool
handle_remove_workspace_request(ResyncDaemonCommand *cmd, char **error_msg)
{
    //TODO
    return true;
}

static bool
handle_remove_remote_system_request(ResyncDaemonCommand *cmd, char **error_msg)
{
    //TODO:
    return true;
}

static bool
handle_request(char *cmd_buffer, char **error_msg)
{
    ResyncDaemonCommand *command = stringified_json_to_resync_daemon_command(cmd_buffer, error_msg);
    if (command == NULL) {
        if (error_msg != NULL && *error_msg == NULL) {
            *error_msg = resync_strdup("Unable to parse command provided to reSync daemon");
        }
        return false;
    }

    bool res = true;
    switch (command->command_type) {
        case ADD_WORKSPACE:
            res = handle_add_workspace_request(command, error_msg);
            break;
        case ADD_REMOTE_SYSTEM:
            res = handle_add_remote_system_request(command, error_msg);
            break;
        case REMOVE_WORKSPACE:
            res = handle_remove_workspace_request(command, error_msg);
            break;
        case REMOVE_REMOTE_SYSTEM:
            res = handle_remove_remote_system_request(command, error_msg);
            break;
        default:
            *error_msg = resync_strdup("reSync daemon encountered an unsupported command");
            return false;
    }

    // TODO: free command object

    return res;
}

static void
server_loop(void)
{
    const int command_server_socket = create_unix_server_socket(DEFAULT_RESYNC_DAEMON_SOCKET_PATH);

    while (!terminate_daemon) {

        int client_fd = accept(command_server_socket, NULL, NULL);
        if (client_fd == -1) {
            unlink(DEFAULT_RESYNC_DAEMON_SOCKET_PATH);
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
                unlink(DEFAULT_RESYNC_DAEMON_SOCKET_PATH);
                fatal_error("realloc");
            }

            const ssize_t offset = COMMAND_BUFFER_CHUNK_SIZE * (received_cmd_buffer_chunks - 1);
            memset(command_buffer + offset, '\0', COMMAND_BUFFER_CHUNK_SIZE);
            strncat(command_buffer, buffer, bytes_received);

            if (strncmp(buffer + (bytes_received-2), "\r\n", 2) == 0) {
                break;
            }
        }

        if (bytes_received < 0) {
            DO_FREE(command_buffer);

            char *response_msg;

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                response_msg = "Connection closed as daemon did not receive a valid/complete request in a timely manner!\n";
                write(client_fd, response_msg, strlen(response_msg));
                close(client_fd);
                continue;
            }

            response_msg = "An error occurred while reading the command from the client!\n";
            write(client_fd, response_msg, strlen(response_msg));
            close(client_fd);
            break;
        }

        char *error_msg;
        bool res = handle_request(command_buffer, &error_msg);

        char *response_msg;
        if (res == true) {
            response_msg = resync_strdup("Successfully performed the requested operation!\n");
        } else {
            if (error_msg == NULL) {
                response_msg = resync_strdup("An error occurred while processing the request!\n");
            } else {
                response_msg = format_string("Error: %s\n", error_msg);
            }
        }

        write(client_fd, response_msg, strlen(response_msg));
        DO_FREE(response_msg);

        close(client_fd);
        DO_FREE(command_buffer);
    }

    close(command_server_socket);
    unlink(DEFAULT_RESYNC_DAEMON_SOCKET_PATH);
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
