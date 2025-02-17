#include "types.h"

static char *
get_string_value(const cJSON *object, const char *key, const bool required)
{
    cJSON *entry = cJSON_GetObjectItemCaseSensitive(object, key);
    if (required && (entry == NULL || entry->valuestring == NULL)) {
        fatal_custom_error("Key '%s' is missing from \n'%s'", key, cJSON_Print(object));
    }

    return entry == NULL ? NULL : resync_strdup(entry->valuestring);
}

static cJSON *
stringified_json_to_cJSON(const char *stringified_json_object)
{
    cJSON *object = cJSON_Parse(stringified_json_object);
    if (object == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fatal_custom_error("Error parsing JSON object before: \n'%s'", error_ptr);
        }
        fatal_custom_error("Error parsing stringified JSON object \n'%s'", stringified_json_object);
    }

    return object;
}

static SshConnectionInformation *
extract_ssh_connection_information(const cJSON *connection_info_object)
{
    SshConnectionInformation *sshConnectionInformation = (SshConnectionInformation *) do_malloc(sizeof(SshConnectionInformation));

    sshConnectionInformation->username = get_string_value(
            connection_info_object,
            KEY_SSH_CONNECTION_INFO_USERNAME,
            false
    );

    sshConnectionInformation->hostname = get_string_value(
            connection_info_object,
            KEY_SSH_CONNECTION_INFO_HOSTNAME,
            true
    );

    sshConnectionInformation->path_to_identity_file = get_string_value(
            connection_info_object,
            KEY_SSH_CONNECTION_INFO_IDENTITY_FILE,
            false
    );

    return sshConnectionInformation;
}

static RsyncConnectionInformation *
extract_rsync_daemon_connection_information(const cJSON *connection_info_object)
{
    RsyncConnectionInformation *rsyncConnectionInformation = (RsyncConnectionInformation *) do_malloc(sizeof(RsyncConnectionInformation));

    rsyncConnectionInformation->username = get_string_value(
            connection_info_object,
            KEY_RSYNC_DAEMON_CONNECTION_INFO_USERNAME,
            false
    );

    rsyncConnectionInformation->hostname = get_string_value(
            connection_info_object,
            KEY_RSYNC_DAEMON_CONNECTION_INFO_HOSTNAME,
            true
    );

    cJSON *port_entry = cJSON_GetObjectItemCaseSensitive(connection_info_object, KEY_RSYNC_DAEMON_CONNECTION_INFO_PORT);
    if (port_entry != NULL) {
        if (port_entry->valuestring != NULL) {
            fatal_custom_error(
                    "Invalid port value '%s' found in connection info definition \n'%s'",
                    port_entry->valuestring,
                    cJSON_Print(connection_info_object)
            );
        }

        if (port_entry->valueint < 0 || port_entry->valueint > 65535) {
            fatal_custom_error(
                    "Invalid port number '%d' found in connection info definition \n'%s'",
                    port_entry->valueint,
                    cJSON_Print(connection_info_object)
            );
        }
        rsyncConnectionInformation->port = port_entry->valueint;
    } else {
        rsyncConnectionInformation->port = -1;
    }

    return rsyncConnectionInformation;
}

static char *
extract_ssh_host_alias_connection_information(const cJSON *connection_info_object)
{
    return get_string_value(connection_info_object, KEY_SSH_HOST_ALIAS_CONNECTION_INFO_NAME, true);
}

static RemoteWorkspaceMetadata *
remote_system_json_to_remote_workspace_metadata(const cJSON *remote_system_object)
{
    RemoteWorkspaceMetadata *remote_ws_metadata = (RemoteWorkspaceMetadata *) do_malloc(sizeof(RemoteWorkspaceMetadata));

    remote_ws_metadata->remote_workspace_root_path = get_string_value(
            remote_system_object,
            KEY_REMOTE_WORKSPACE_ROOT_PATH,
            true
    );

    char *connection_type = get_string_value(remote_system_object, KEY_CONNECTION_TYPE, true);

    if (IS_SSH_CONNECTION_TYPE(connection_type)) {
        remote_ws_metadata->connection_type = SSH;
    } else if (IS_SSH_HOST_ALIAS_CONNECTION_TYPE(connection_type)) {
        remote_ws_metadata->connection_type = SSH_HOST_ALIAS;
    } else if (IS_RSYNC_DAEMON_CONNECTION_TYPE(connection_type)) {
        remote_ws_metadata->connection_type = RSYNC_DAEMON;
    } else {
        fatal_custom_error(
                "Invalid connection type '%s' specified for remote system \n'%s'",
                connection_type,
                cJSON_Print(remote_system_object)
        );
    }

    cJSON *connection_info_entry = cJSON_GetObjectItemCaseSensitive(remote_system_object, KEY_CONNECTION_INFORMATION);
    if (connection_info_entry == NULL) {
        fatal_custom_error(
                "No connection information of type '%s' specified in remote system definition \n'%s'",
                connection_type,
                cJSON_Print(remote_system_object)
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
            break;
        default:
            fatal_custom_error("Unknown error related to connection type of '%s' occurred", cJSON_Print(remote_system_object));
    }

    DO_FREE(connection_type);
    return remote_ws_metadata;
}

WorkspaceInformation *
stringified_json_to_workspace_information(const char *stringified_json_object)
{
    cJSON *json_object = stringified_json_to_cJSON(stringified_json_object);
    return json_to_workspace_information(json_object);
}

WorkspaceInformation *
json_to_workspace_information(const cJSON *json_object)
{
    WorkspaceInformation *workspace_information = (WorkspaceInformation *) do_malloc(sizeof(WorkspaceInformation));
    workspace_information->remote_systems = NULL;

    workspace_information->local_workspace_root_path = get_string_value(
            json_object,
            KEY_LOCAL_WORKSPACE_ROOT_PATH,
            true
    );

    cJSON *remote_system_entry;
    cJSON *remote_systems = cJSON_GetObjectItemCaseSensitive(json_object, KEY_REMOTE_SYSTEMS);

    int remote_systems_counter = 0;
    cJSON_ArrayForEach(remote_system_entry, remote_systems) {
        remote_systems_counter++;
        RemoteWorkspaceMetadata *remote_ws_metadata = remote_system_json_to_remote_workspace_metadata(remote_system_entry);
        LL_APPEND(workspace_information->remote_systems, remote_ws_metadata);
    }

    if (remote_systems_counter == 0) {
        fatal_custom_error(
                "No remote systems (expected key: '%s') specified in workspace definition \n'%s'",
                KEY_REMOTE_SYSTEMS,
                cJSON_Print(json_object)
        );
    }

    return workspace_information;
}

char *
workspace_information_to_stringified_json(WorkspaceInformation *workspace_information)
{
    return cJSON_Print(workspace_information_to_json(workspace_information));
}

cJSON *
workspace_information_to_json(WorkspaceInformation *workspace_information)
{
    // TODO
    return NULL;
}
