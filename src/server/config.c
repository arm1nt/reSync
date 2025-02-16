#include "config.h"

/*
 * Structure of the reSync configuration file:
 *
 * [
 *  {
 *      "local_workspace_root_path": VALUE,
 *      "remote_systems": [
 *          {
 *              "remote_workspace_root_path": VALUE,
 *              "connection_type": SSH | SSH_HOST_ALIAS | RSYNC_DAEMON,
 *              "connection_information": VALUE
 *          },
 *          ...
 *       ]
 *  },
 *  ...
 * ]
 *
 */

static char *
read_config_file_into_buffer(void)
{
    FILE *config_file = fopen(CONFIG_FILE_PATH, "r");
    if (config_file == NULL) {
        fatal_custom_error(
                "Unable to open reSync config file, expected file to be located at '%s': %s",
                CONFIG_FILE_PATH,
                strerror(errno)
        );
    }

    char *file_buffer = NULL;
    ssize_t buffer_size = 0;

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    while ((read = getline(&line, &len, config_file)) != -1) {

        if (is_blank(line)) {
            continue;
        }

        buffer_size += (read + 1); //Allocate memory for a terminating null byte
        file_buffer = (char *) do_realloc(file_buffer, buffer_size);
        strncat(file_buffer, line, read);
    }

    fclose(config_file);
    DO_FREE(line);

    return file_buffer;
}

static int
get_number(const cJSON *object, const char *key, const bool required)
{
    cJSON *entry = cJSON_GetObjectItemCaseSensitive(object, key);
    if (required && entry == NULL) {
        fatal_custom_error("Key '%s' is missing from '%s'", key, cJSON_Print(object));
    }
    return entry == NULL ? -1 : entry->valueint;
}

static char*
get_string(const cJSON *object, const char *key, const bool required)
{
    cJSON *entry = cJSON_GetObjectItemCaseSensitive(object, key);
    if (required && (entry == NULL || entry->valuestring == NULL)) {
        fatal_custom_error("Key '%s' is missing from '%s'", key, cJSON_Print(object));
    }
    return entry == NULL ? NULL : resync_strdup(entry->valuestring);
}

static SshConnectionInformation *
extract_ssh_connection_information(const cJSON *connection_info_entry)
{
    SshConnectionInformation *sshConnectionInformation = (SshConnectionInformation *) do_malloc(sizeof(SshConnectionInformation));
    sshConnectionInformation->username = get_string(connection_info_entry, CFG_KEY_SSH_CONNECTION_INFO_USERNAME, false);
    sshConnectionInformation->hostname = get_string(connection_info_entry, CFG_KEY_SSH_CONNECTION_INFO_HOSTNAME, true);
    sshConnectionInformation->path_to_identity_file = get_string(connection_info_entry, CFG_KEY_SSH_CONNECTION_INFO_IDENTITY_FILE, false);
    return sshConnectionInformation;
}

static RsyncConnectionInformation *
extract_rsync_daemon_connection_information(const cJSON *connection_info_entry)
{
    RsyncConnectionInformation *rsyncConnectionInformation = (RsyncConnectionInformation *) do_malloc(sizeof(RsyncConnectionInformation));
    rsyncConnectionInformation->username = get_string(connection_info_entry, CFG_KEY_RSYNC_DAEMON_CONNECTION_INFO_USERNAME, false);
    rsyncConnectionInformation->hostname = get_string(connection_info_entry, CFG_KEY_RSYNC_DAEMON_CONNECTION_INFO_HOSTNAME, true);
    rsyncConnectionInformation->port = get_number(connection_info_entry, CFG_KEY_RSYNC_DAEMON_CONNECTION_INFO_PORT, false);
    return rsyncConnectionInformation;
}

static char *
extract_ssh_host_alias_connection_information(const cJSON *connection_info_entry)
{
    return get_string(connection_info_entry, CFG_KEY_SSH_HOST_ALIAS_CONNECTION_INFO_NAME, true);
}

static RemoteWorkspaceMetadata *
remote_system_entry_to_remote_workspace_metadata(const cJSON *remote_system_entry)
{
    RemoteWorkspaceMetadata *remote_ws_metadata = (RemoteWorkspaceMetadata *) do_malloc(sizeof(RemoteWorkspaceMetadata));

    cJSON *remote_workspace_root_path_entry = cJSON_GetObjectItemCaseSensitive(remote_system_entry, CFG_KEY_REMOTE_WORKSPACE_ROOT_PATH);
    if (remote_workspace_root_path_entry == NULL || remote_workspace_root_path_entry->valuestring == NULL) {
        fatal_custom_error(
                "Key '%s' is missing in remote system definition '%s'",
                CFG_KEY_REMOTE_WORKSPACE_ROOT_PATH,
                cJSON_Print(remote_system_entry)
        );
    } else {
        remote_ws_metadata->remote_workspace_root_path = resync_strdup(remote_workspace_root_path_entry->valuestring);
    }

    cJSON *connection_type_entry = cJSON_GetObjectItemCaseSensitive(remote_system_entry, CFG_KEY_CONNECTION_TYPE);
    if (connection_type_entry == NULL || connection_type_entry->valuestring == NULL) {
        fatal_custom_error("No connection type specified for remote system definition '%s'", cJSON_Print(remote_system_entry));
    } else {

        if (IS_SSH_CONNECTION_TYPE(connection_type_entry->valuestring)) {
            remote_ws_metadata->connection_type = SSH;
        } else if (IS_SSH_HOST_ALIAS_CONNECTION_TYPE(connection_type_entry->valuestring)) {
            remote_ws_metadata->connection_type = SSH_HOST_ALIAS;
        } else if (IS_RSYNC_DAEMON_CONNECTION_TYPE(connection_type_entry->valuestring)) {
            remote_ws_metadata->connection_type = RSYNC_DAEMON;
        } else {
            fatal_custom_error(
                    "Invalid connection type '%s' specified for remote system '%s'",
                    connection_type_entry->valuestring,
                    cJSON_Print(remote_system_entry)
            );
        }
    }

    cJSON *connection_info_entry = cJSON_GetObjectItemCaseSensitive(remote_system_entry, CFG_KEY_CONNECTION_INFORMATION);
    if (connection_type_entry == NULL) {
        fatal_custom_error(
                "No connection information of type '%s' specified for remote system definition '%s'",
                remote_ws_metadata->connection_type,
                cJSON_Print(remote_system_entry)
        );
    }

    switch (remote_ws_metadata->connection_type) {
        case SSH:
            remote_ws_metadata->connection_information.sshConnectionInformation =
                    extract_ssh_connection_information(connection_info_entry);
            break;
        case SSH_HOST_ALIAS:
            remote_ws_metadata->connection_information.ssh_host_alias =
                    extract_ssh_host_alias_connection_information(connection_info_entry);
            break;
        case RSYNC_DAEMON:
            remote_ws_metadata->connection_information.rsyncConnectionInformation =
                    extract_rsync_daemon_connection_information(connection_info_entry);
        default:
            fatal_custom_error("Unknown error related to connection type of '%s' occurred", cJSON_Print(remote_system_entry));
    }

    return remote_ws_metadata;
}

static WorkspaceInformation *
config_entry_to_workspace_information(const cJSON *config_entry)
{
    WorkspaceInformation *workspace_information = (WorkspaceInformation *) do_malloc(sizeof(WorkspaceInformation));
    workspace_information->remote_systems = NULL;

    cJSON *local_workspace_root_path_entry = cJSON_GetObjectItemCaseSensitive(config_entry, CFG_KEY_LOCAL_WORKSPACE_ROOT_PATH);
    if (local_workspace_root_path_entry == NULL || local_workspace_root_path_entry->valuestring == NULL) {
        fatal_custom_error("Key '%s' is missing in config entry '%s'.", CFG_KEY_LOCAL_WORKSPACE_ROOT_PATH, cJSON_Print(config_entry));
    } else {
        workspace_information->local_workspace_root_path = resync_strdup(local_workspace_root_path_entry->valuestring);
    }

    cJSON *remote_system_entry;
    cJSON *remote_systems = cJSON_GetObjectItemCaseSensitive(config_entry, CFG_KEY_REMOTE_SYSTEMS);

    int remote_systems_counter = 0;
    cJSON_ArrayForEach(remote_system_entry, remote_systems) {
        remote_systems_counter++;
        RemoteWorkspaceMetadata *remote_ws_metadata = remote_system_entry_to_remote_workspace_metadata(remote_system_entry);
        LL_APPEND(workspace_information->remote_systems, remote_ws_metadata);
    }

    if (remote_systems_counter == 0) {
        fatal_custom_error("No remote systems specified in config entry '%s'", cJSON_Print(config_entry));
    }

    return workspace_information;
}

WorkspaceInformation *
parse_config_file(void)
{
    WorkspaceInformation *head = NULL;
    char *config_file_buffer = read_config_file_into_buffer();

    cJSON *config_object = cJSON_Parse(config_file_buffer);
    if (config_object == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fatal_custom_error("Error in config file before: %s", error_ptr);
        }
        goto out;
    }

    cJSON *config_entry;
    cJSON_ArrayForEach(config_entry, config_object) {
        WorkspaceInformation *ws_information = config_entry_to_workspace_information(config_entry);
        LL_APPEND(head, ws_information);
    }

out:
    DO_FREE(config_file_buffer);
    cJSON_Delete(config_object);

    return head;
}
