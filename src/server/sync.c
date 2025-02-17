#include "sync.h"

static char *
construct_rsync_local_dir_arg(WorkspaceInformation *ws_info, const char *relative_path)
{
    char *arg, *local_dir_path;
    local_dir_path = concat_paths(ws_info->local_workspace_root_path, relative_path);
    arg = format_string("%s/", local_dir_path);
    DO_FREE(local_dir_path);
    return arg;
}

static char *
construct_remote_dir_arg_for_ssh(RemoteWorkspaceMetadata *remote_system, const char *relative_dir_path)
{
    char *remote_dir_path = concat_paths(remote_system->remote_workspace_root_path, relative_dir_path);

    SshConnectionInformation *connection_information = remote_system->connection_information.sshConnectionInformation;

    if (connection_information->username != NULL) {
        char *arg = format_string(
                "%s@%s:%s",
                connection_information->username,
                connection_information->hostname,
                remote_dir_path
        );

        DO_FREE(remote_dir_path);
        return arg;
    }

    char *arg = format_string("%s:%s", connection_information->hostname, remote_dir_path);

    DO_FREE(remote_dir_path);
    return arg;
}

static char *
construct_remote_dir_arg_for_ssh_host_alias(RemoteWorkspaceMetadata *remote_system, const char *relative_dir_path)
{
    char *remote_dir_path = concat_paths(remote_system->remote_workspace_root_path, relative_dir_path);

    char *arg = format_string(
            "%s:%s",
            remote_system->connection_information.ssh_host_alias,
            remote_dir_path
    );

    DO_FREE(remote_dir_path);
    return arg;
}

static char *
construct_remote_dir_arg_for_rsync_daemon(RemoteWorkspaceMetadata *remote_system, const char *relative_dir_path)
{
    char *remote_dir_path = concat_paths(remote_system->remote_workspace_root_path, relative_dir_path);

    RsyncConnectionInformation *connection_information = remote_system->connection_information.rsyncConnectionInformation;

    char *user_and_host_segment;
    if (connection_information->username != NULL) {
        user_and_host_segment = format_string("%s@%s", connection_information->username, connection_information->hostname);
    } else {
        user_and_host_segment = resync_strdup(connection_information->hostname);
    }

    char *arg;
    if (connection_information->port >= 0) {
        arg = format_string(
                "rsync://%s:%d%s",
                user_and_host_segment,
                connection_information->port,
                remote_dir_path
        );
    } else {
        arg = format_string("%s::%s", user_and_host_segment, remote_dir_path);
    }

    DO_FREE(remote_dir_path);
    DO_FREE(user_and_host_segment);
    return arg;

}

static char *
construct_rsync_remote_dir_arg(RemoteWorkspaceMetadata *remote_system, const char *relative_path)
{
    switch (remote_system->connection_type) {
        case SSH:
            return construct_remote_dir_arg_for_ssh(remote_system, relative_path);
        case SSH_HOST_ALIAS:
            return construct_remote_dir_arg_for_ssh_host_alias(remote_system, relative_path);
        case RSYNC_DAEMON:
            return construct_remote_dir_arg_for_rsync_daemon(remote_system, relative_path);
        default:
            fatal_custom_error(
                    "Unknown error occurred while constructing rsync remote dir arg because of connection type '%s'",
                    remote_system->connection_type
            );
    }
}

static char**
construct_rsync_cmd_arguments(WorkspaceInformation *ws_info, RemoteWorkspaceMetadata *remote_system, const char *relative_path)
{
    int index = 0;
    int current_args_buffer_size = 6;
    char **args =  (char **) do_malloc(current_args_buffer_size * sizeof(char *));

    args[index++] = resync_strdup("rsync");
    args[index++] = resync_strdup("-azq");
    args[index++] = resync_strdup("--delete");

    if (remote_system->connection_type == SSH
        && remote_system->connection_information.sshConnectionInformation->path_to_identity_file != NULL) {

        args = (char **) do_realloc(args, ++current_args_buffer_size * sizeof(char*));

        char *path_to_identity_file = remote_system->connection_information.sshConnectionInformation->path_to_identity_file;
        args[index++] = format_string("-e \"ssh -i %s\"", path_to_identity_file);
    }

    args[index++] = construct_rsync_local_dir_arg(ws_info, relative_path);
    args[index++] = construct_rsync_remote_dir_arg(remote_system, relative_path);
    args[index++] = (char *) NULL;

    return args;
}

static void
execute_rsync_command(void)
{
    //TODO:
}

void
synchronize_with_remote_system(WorkspaceInformation *ws_info, RemoteWorkspaceMetadata *remote_system, const char *relative_path)
{
    char **args = construct_rsync_cmd_arguments(ws_info, remote_system, relative_path);

    // TODO: execute command

    char **ptr = args;
    while (*ptr != NULL) {
        DO_FREE(*ptr);
        *ptr++;
    }
}

void
synchronize_workspace(WorkspaceInformation *workspace_information, const char *relative_path)
{
    RemoteWorkspaceMetadata *entry;
    LL_FOREACH(workspace_information->remote_systems, entry) {
        synchronize_with_remote_system(workspace_information, entry, relative_path);
    }
}

