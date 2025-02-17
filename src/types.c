#include "types.h"

static char *
get_string_value(const cJSON *object, const char *key, const bool required)
{
    cJSON *entry = cJSON_GetObjectItemCaseSensitive(object, key);
    if (required && (entry == NULL || entry->valuestring == NULL)) {
        fatal_custom_error("Error: Key '%s' is missing from \n'%s'", key, cJSON_Print(object));
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
                    "Error: Invalid port value '%s' found in connection info definition \n'%s'",
                    port_entry->valuestring,
                    cJSON_Print(connection_info_object)
            );
        }

        if (port_entry->valueint < 0 || port_entry->valueint > 65535) {
            fatal_custom_error(
                    "Error: Invalid port number '%d' found in connection info definition \n'%s'",
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
                "Error: Invalid connection type '%s' specified for remote system \n'%s'",
                connection_type,
                cJSON_Print(remote_system_object)
        );
    }

    cJSON *connection_info_entry = cJSON_GetObjectItemCaseSensitive(remote_system_object, KEY_CONNECTION_INFORMATION);
    if (connection_info_entry == NULL) {
        fatal_custom_error(
                "Error: No connection information of type '%s' specified in remote system definition \n'%s'",
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
                "Error: No remote systems (expected key: '%s') specified in workspace definition \n'%s'",
                KEY_REMOTE_SYSTEMS,
                cJSON_Print(json_object)
        );
    }

    return workspace_information;
}

static cJSON *
create_json_object(void)
{
    cJSON *json_object = cJSON_CreateObject();
    if (json_object == NULL) {
        fatal_custom_error("Error: cJSON_CreateObject failed");
    }

    return json_object;
}

static cJSON *
create_json_array(void)
{
    cJSON *json_array = cJSON_CreateArray();
    if (json_array == NULL) {
        fatal_custom_error("Error: cJSON_CreateArray failed");
    }

    return json_array;
}

static cJSON *
create_json_string(const char *string)
{
    cJSON *json_string = cJSON_CreateString(string);
    if (json_string == NULL) {
        fatal_custom_error("Error: cJSON_CreateString failed for string '%s'", string);
    }

    return json_string;
}

static cJSON *
create_json_number(const int number)
{
    cJSON *json_number = cJSON_CreateNumber(number);
    if (json_number == NULL) {
        fatal_custom_error("Error: cJSON_CreateNumber failed for number '%d'", number);
    }

    return json_number;
}

static cJSON *
ssh_connection_information_to_json(SshConnectionInformation *ssh_info)
{
    if (ssh_info == NULL) {
        fatal_custom_error("Error: SSH connection information struct is missing");
    }

    cJSON *connection_information = create_json_object();

    if (ssh_info->username != NULL) {
        cJSON *username = create_json_string(ssh_info->username);
        cJSON_AddItemToObject(connection_information, KEY_SSH_CONNECTION_INFO_USERNAME, username);
    }

    if (ssh_info->path_to_identity_file != NULL) {
        cJSON *identity_file = create_json_string(ssh_info->path_to_identity_file);
        cJSON_AddItemToObject(connection_information, KEY_SSH_CONNECTION_INFO_IDENTITY_FILE, identity_file);
    }

    if (ssh_info->hostname != NULL) {
        cJSON *hostname = create_json_string(ssh_info->hostname);
        cJSON_AddItemToObject(connection_information, KEY_SSH_CONNECTION_INFO_HOSTNAME, hostname);
    } else {
        fatal_custom_error(
                "Error: SSH connection information struct is missing the required '%s' entry",
                KEY_SSH_CONNECTION_INFO_HOSTNAME
        );
    }

    return connection_information;
}

static cJSON *
rsync_daemon_connection_information_to_json(RsyncConnectionInformation *rsync_daemon_info)
{
    if (rsync_daemon_info == NULL) {
        fatal_custom_error("Error: RSYNC daemon connection information struct is missing");
    }

    cJSON *connection_information = create_json_object();

    if (rsync_daemon_info->username != NULL) {
        cJSON *username = create_json_string(rsync_daemon_info->username);
        cJSON_AddItemToObject(connection_information, KEY_RSYNC_DAEMON_CONNECTION_INFO_USERNAME, username);
    }

    if (rsync_daemon_info->port >= 0) {
        if (rsync_daemon_info->port > 65535) {
            fatal_custom_error("Error: Invalid port '%d' found", rsync_daemon_info->port);
        }

        cJSON *port = create_json_number(rsync_daemon_info->port);
        cJSON_AddItemToObject(connection_information, KEY_RSYNC_DAEMON_CONNECTION_INFO_PORT, port);
    }

    if (rsync_daemon_info->hostname != NULL) {
        cJSON *hostname = create_json_string(rsync_daemon_info->hostname);
        cJSON_AddItemToObject(connection_information, KEY_RSYNC_DAEMON_CONNECTION_INFO_HOSTNAME, hostname);
    } else {
        fatal_custom_error(
                "Error: RSYNC daemon connection information struct is missing the required '%s' entry",
                KEY_RSYNC_DAEMON_CONNECTION_INFO_HOSTNAME
        );
    }

    return connection_information;
}

static cJSON *
ssh_host_alias_connection_information_to_json(char *ssh_host_alias)
{
    cJSON *connection_information = create_json_object();

    if (ssh_host_alias != NULL) {
        cJSON *host_alias = create_json_string(ssh_host_alias);
        cJSON_AddItemToObject(connection_information, KEY_SSH_HOST_ALIAS_CONNECTION_INFO_NAME, host_alias);
    } else {
        fatal_custom_error(
                "Error: SSH host alias connection information is missing required '%s' entry",
                KEY_SSH_HOST_ALIAS_CONNECTION_INFO_NAME
        );
    }

    return connection_information;
}

static cJSON *
remote_workspace_metadata_to_json(RemoteWorkspaceMetadata *remote_workspace_metadata)
{
    cJSON *remote_system = create_json_object();

    if (remote_workspace_metadata->remote_workspace_root_path != NULL) {
        cJSON *remote_workspace_root_path = create_json_string(remote_workspace_metadata->remote_workspace_root_path);
        cJSON_AddItemToObject(remote_system, KEY_REMOTE_WORKSPACE_ROOT_PATH, remote_workspace_root_path);
    } else {
        fatal_custom_error(
                "Error: Remote workspace struct is missing the required '%s' entry",
                KEY_REMOTE_WORKSPACE_ROOT_PATH
        );
    }

    cJSON *connection_type;
    cJSON *connection_information;
    switch (remote_workspace_metadata->connection_type) {
        case SSH:
            connection_type = create_json_string(CONNECTION_TYPE_SSH);
            connection_information = ssh_connection_information_to_json(
                    remote_workspace_metadata->connection_information.sshConnectionInformation
            );
            break;
        case SSH_HOST_ALIAS:
            connection_type = create_json_string(CONNECTION_TYPE_SSH_HOST_ALIAS);
            connection_information = ssh_host_alias_connection_information_to_json(
                    remote_workspace_metadata->connection_information.ssh_host_alias
            );
            break;
        case RSYNC_DAEMON:
            connection_type = create_json_string(CONNECTION_TYPE_RSYNC_DAEMON);
            connection_information = rsync_daemon_connection_information_to_json(
                    remote_workspace_metadata->connection_information.rsyncConnectionInformation
            );
            break;
        default:
            fatal_custom_error(
                    "Unknown error related to connection type '%s' of remote system",
                    remote_workspace_metadata->connection_type
            );
    }

    cJSON_AddItemToObject(remote_system, KEY_CONNECTION_TYPE, connection_type);
    cJSON_AddItemToObject(remote_system, KEY_CONNECTION_INFORMATION, connection_information);

    return remote_system;
}

char *
workspace_information_to_stringified_json(WorkspaceInformation *workspace_information)
{
    cJSON *json_object = workspace_information_to_json(workspace_information);
    char *stringified_json_object = cJSON_Print(json_object);
    cJSON_Delete(json_object);
    return stringified_json_object;
}

cJSON *
workspace_information_to_json(WorkspaceInformation *workspace_information)
{
    cJSON *json_ws_information = create_json_object();

    if (workspace_information->local_workspace_root_path != NULL) {
        cJSON *local_workspace_root_path = create_json_string(workspace_information->local_workspace_root_path);
        cJSON_AddItemToObject(json_ws_information, KEY_LOCAL_WORKSPACE_ROOT_PATH, local_workspace_root_path);
    } else {
        fatal_custom_error(
                "Error: workspace information struct is missing the required '%s' entry",
                KEY_LOCAL_WORKSPACE_ROOT_PATH
        );
    }

    cJSON *remote_systems = create_json_array();
    cJSON_AddItemToObject(json_ws_information, KEY_REMOTE_SYSTEMS, remote_systems);

    int remote_systems_counter = 0;
    RemoteWorkspaceMetadata *entry;
    LL_FOREACH(workspace_information->remote_systems, entry) {
        remote_systems_counter++;
        cJSON *remote_system = remote_workspace_metadata_to_json(entry);
        cJSON_AddItemToArray(remote_systems, remote_system);
    }

    if (remote_systems_counter == 0) {
        fatal_custom_error("Error: workspace information struct does not contain any remote system definitions");
    }

    return json_ws_information;
}
