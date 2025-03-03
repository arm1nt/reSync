#include "../util/debug.h"
#include "config.h"
#include "../socket.h"
#include "../types/types.h"
#include "../types/mappers.h"
#include "../../lib/utash.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <syslog.h>
#include <sys/wait.h>

#define COMMAND_BUFFER_CHUNK_SIZE ((ssize_t)(2048 * sizeof(char)))

#define DEFAULT_RCV_TIMEOUT_SEC 2
#define DEFAULT_RCV_TIMEOUT_USEC 0

#define WORKSPACE_MONITOR_EXECUTABLE "./linux/ws"

typedef struct WorkspaceProcessInfo {
    const char *ws_path;
    pid_t process_pid;
    UT_hash_handle hh;
} WorkspaceProcessInfo;

WorkspaceProcessInfo *ws_to_process_map = NULL;

volatile sig_atomic_t terminate_daemon = 0;

static void
install_signal_handlers(void)
{
    // TODO:
    //  Handle e.g. process termination or when child process terminates
}

static bool
start_workspace_monitor(const ConfigFileEntryData *config_entry_info, char **error_msg)
{
    int pipe_fd[2];
    pid_t intermediate_pid, grandchild_pid;

    if (pipe(pipe_fd) == -1) {
        SET_ERROR_MSG_RAW(
                error_msg,
                format_string(
                        "Creating a pipe while trying to create the fs monitoring & syncing process for workspace "
                        "'%s' failed: %s",
                        config_entry_info->workspace_information->local_workspace_root_path,
                        strerror(errno)
                )
        );
        return false;
    }

    intermediate_pid = fork();
    if (intermediate_pid < 0) {
        SET_ERROR_MSG_RAW(
                error_msg,
                format_string(
                        "Creating the intermediate processes while trying to create a fs monitoring & syncing "
                        "process for workspace '%s' failed: %s",
                        config_entry_info->workspace_information->local_workspace_root_path,
                        strerror(errno)
                )
        );
        return false;
    }

    if (intermediate_pid == 0) { /* Intermediate child process */
        close(pipe_fd[0]);

        if (setsid() == -1) {
            fatal_error("setsid");
        }

        grandchild_pid = fork();
        if (grandchild_pid < 0) {
            fatal_error("fork");
        }

        if (grandchild_pid > 0) { /* Intermediate child process */
            write(pipe_fd[1], &grandchild_pid, sizeof(grandchild_pid));
            close(pipe_fd[1]);
            exit(EXIT_SUCCESS);
        }

        /* Grandchild process */
        close(pipe_fd[1]);
        execlp(
                WORKSPACE_MONITOR_EXECUTABLE,
                WORKSPACE_MONITOR_EXECUTABLE,
                config_entry_info->stringified_json_workspace_information,
                (char *) NULL
        );

        LOG_ERROR(
                "'execlp' failed for workspace '%s': %s\n",
                config_entry_info->workspace_information->local_workspace_root_path,
                strerror(errno)
        );
        fatal_error("execlp");
    }

    close(pipe_fd[1]);
    read(pipe_fd[0], &grandchild_pid, sizeof(grandchild_pid));
    close(pipe_fd[0]);

    // Wait for the intermediate child process to terminate and, if successful, store the grandchild PID
    int status;
    waitpid(intermediate_pid, &status, 0);

    if (!WIFEXITED(status)) {
        SET_ERROR_MSG_RAW(
                error_msg,
                format_string(
                        "Creating the fs monitoring & syncing process for workspace '%s' failed: %s",
                        config_entry_info->workspace_information->local_workspace_root_path,
                        strerror(errno)
                )
        );
        return false;
    }

    char *ws_path = resync_strdup(config_entry_info->workspace_information->local_workspace_root_path);
    WorkspaceProcessInfo *new_entry = (WorkspaceProcessInfo *) do_calloc(1, sizeof(WorkspaceProcessInfo));
    new_entry->ws_path = ws_path;
    new_entry->process_pid = grandchild_pid;

    HASH_ADD_STR(ws_to_process_map, ws_path, new_entry);
    return true;
}

static bool
terminate_fs_monitoring_process(const char *ws_path, char **error_msg)
{
    WorkspaceProcessInfo *process_information;
    HASH_FIND_STR(ws_to_process_map, ws_path, process_information);
    if (process_information == NULL) {
        SET_ERROR_MSG(
                error_msg,
                "There is currently no process running that monitors and synchronizes workspace changes, so it cannot be terminated!"
        );
        return false;
    }

    if (kill(process_information->process_pid, SIGTERM) == -1) {
        SET_ERROR_MSG_RAW(
                error_msg,
                format_string(
                        "An error occurred while trying to terminate the fs monitoring & synchronizing process for "
                        "workspace '%s': %s",
                        process_information->ws_path,
                        strerror(errno)
                )
        );
        return false;
    }

    HASH_DEL(ws_to_process_map, process_information);

    return true;
}

static bool
terminate_all_workspace_processes(char **error_msg)
{
    bool res = true;

    WorkspaceProcessInfo *entry, *tmp;
    HASH_ITER(hh, ws_to_process_map, entry, tmp) {
        bool single_res = terminate_fs_monitoring_process(entry->ws_path, error_msg);
        if (single_res == false) {
            LOG_ERROR("%s\n", *error_msg);
        }
        res &= single_res;
    }

    return res;
}

static bool
restart_workspace_process(const ConfigFileEntryData *entry, char **error_msg)
{
    bool res;

    res = terminate_fs_monitoring_process(entry->workspace_information->local_workspace_root_path, error_msg);
    if (res == false) {
        return false;
    }

    res = start_workspace_monitor(entry, error_msg);
    if (res == false) {
        return false;
    }

    return true;
}

static bool
start_workspace_monitors(ConfigFileEntryData *workspace_config_entries)
{
    bool res;
    char *error_msg = NULL;
    int successful_starts_counter = 0;

    ConfigFileEntryData *entry;
    LL_FOREACH(workspace_config_entries, entry) {
        res = start_workspace_monitor(entry, &error_msg);
        if (res == false) {
            LOG_ERROR("%s\n", error_msg);
            continue;
        }

        successful_starts_counter++;
    }

    if (workspace_config_entries != NULL && successful_starts_counter == 0) {
        return false;
    }

    return true;
}

static bool
handle_add_workspace_request(const ResyncServerCommand *command, char **error_msg)
{
    /*
     * Adds a new workspace to be managed by reSync to the configuration file, and subsequently starts a fs monitoring
     * processes that synchronises the ws with all specified remote systems.
     */

    bool result;
    ConfigFileEntryData *config_entry = NULL;

    result = add_workspace_to_configuration_file(
            command->command_metadata.workspace_information,
            &config_entry,
            error_msg
    );

    if (result == false) {
        return false;
    }

    result = start_workspace_monitor(config_entry, error_msg);
    if (result == false) {
        SET_ERROR_MSG_WITH_CAUSE(
                error_msg,
                "Workspace was added to the config file, but starting a fs monitoring & syncing process failed",
                error_msg
        );
        return false;
    }

    return true;
}

static bool
handle_add_remote_system_request(const ResyncServerCommand *command, char **error_msg)
{
    /*
     * Adds a new remote system definition to a workspaces' entry in the reSync configuration file and subsequently
     *  restarts the workspaces' fs monitoring process to also propagate changes to the newly added remote system.
     */

    bool result;
    ConfigFileEntryData *config_entry = NULL;

    result = add_remote_system_to_workspace_config_entry(
            command->command_metadata.workspace_information,
            &config_entry,
            error_msg
    );

    if (result == false) {
        return false;
    }

    result = restart_workspace_process(config_entry, error_msg);
    if (result == false) {
        SET_ERROR_MSG_WITH_CAUSE(
                error_msg,
                "Remote system was successfully added to the workspaces' config file entry, but restarting the "
                "process that monitors and synchronizes the workspace failed",
                error_msg
        );
        return false;
    }

    return true;
}

static bool
handle_remove_workspace_request(const ResyncServerCommand *command, char **error_msg)
{
    /*
     * Remove a workspace from the reSync configuration file and terminate the process that is responsible for monitoring
     * and synchronizing the workspace.
     */

    bool result;

    result = remove_workspace_from_configuration_file(command->command_metadata.local_workspace_root_path, error_msg);
    if (result == false) {
        return false;
    }

    result = terminate_fs_monitoring_process(command->command_metadata.workspace_information->local_workspace_root_path, error_msg);
    if (result == false) {
        SET_ERROR_MSG_WITH_CAUSE(
                error_msg,
                "Removing the workspaces' entry from the reSync configuration file succeeded, but terminating the process "
                "that monitors and synchronizes the workspace failed",
                error_msg
        );
        return false;
    }

    return true;
}

static bool
handle_remove_remote_system_request(const ResyncServerCommand *command, char **error_msg)
{
    /*
     * Remove a remote system definition from the array of remote systems specified for a workspace. Furthermore, the
     * processes responsible for monitoring and synchronizing the workspace is restarted.
     */

    bool result;
    ConfigFileEntryData *config_entry = NULL;

    result = remove_remote_system_from_workspace_config_entry(
            command->command_metadata.rm_remote_system_md,
            &config_entry,
            error_msg
    );

    if (result == false) {
        return false;
    }

    result = restart_workspace_process(config_entry, error_msg);
    if (result == false) {
        SET_ERROR_MSG_WITH_CAUSE(
                error_msg,
                "Removing the remote system from the workspaces' array of remote systems succeeded, but restarting the "
                "process responsible for monitoring and synchronizing the workspace failed",
                error_msg
        );
        return false;
    }

    return true;
}

static bool
handle_request(const char *cmd_buffer, char **error_msg)
{
    ResyncServerCommand *command = stringified_json_to_resyncServerCommand(cmd_buffer, error_msg);
    if (command == NULL) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "reSync daemon was unable to parse the provided command", error_msg);
        return false;
    }

    bool command_handling_result = true;
    switch (command->command_type) {
        case ADD_WORKSPACE:
            command_handling_result = handle_add_workspace_request(command, error_msg);
            break;
        case REMOVE_WORKSPACE:
            command_handling_result = handle_remove_workspace_request(command, error_msg);
            break;
        case ADD_REMOTE_SYSTEM:
            command_handling_result = handle_add_remote_system_request(command, error_msg);
            break;
        case REMOVE_REMOTE_SYSTEM:
            command_handling_result = handle_remove_remote_system_request(command, error_msg);
            break;
        case OTHER_RESYNC_SERVER_COMMAND_TYPE:
        default:
            SET_ERROR_MSG(error_msg, "Unable to process request as specified command is not supported!");
            return false;
    }

    destroy_resyncServerCommand(&command);
    return command_handling_result;
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

        char *error_msg = NULL;
        bool res = handle_request(command_buffer, &error_msg);

        char *response_msg;
        if (res == true) {
            response_msg = resync_strdup("Successfully performed the requested operation!\n");
        } else {
            if (error_msg == NULL) {
                response_msg = resync_strdup("An error occurred while processing the request!\n");
            } else {
                response_msg = format_string("%s\n", error_msg);
                DO_FREE(error_msg);
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

int
main(const int argc, const char **argv)
{
    openlog("reSync", LOG_PERROR | LOG_PID, 0);

    install_signal_handlers();

    // Parse the configuration file and start a fs monitoring & syncing processes for each workspace defined in it.
    bool res;
    char *error_msg = NULL;
    ConfigFileEntryData *config_file_entries = NULL;

    res = parse_configuration_file(&config_file_entries, &error_msg);
    if (res == false) {
        LOG_ERROR("Unable to parse configuration file: %s", error_msg);
        fatal_custom_error("Error: %s", error_msg);
    }

    res = start_workspace_monitors(config_file_entries);
    if (res == false) {
        LOG_ERROR("Unable to start any workspace monitoring & syncing processes");
        fatal_custom_error("Unable to start any workspace monitoring & syncing processes");
    }

    daemon(0, 0);

    // Start listen for incoming commands and handle them
    server_loop();

    return EXIT_SUCCESS;
}
