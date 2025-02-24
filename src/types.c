#include "types.h"

ResyncCommandType
string_to_resync_command_type(const char *stringified_command_type)
{
    if (IS_ADD_WORKSPACE_CMD(stringified_command_type)) {
        return ADD_WORKSPACE;
    } else if (IS_ADD_REMOTE_SYSTEM_CMD(stringified_command_type)) {
        return ADD_REMOTE_SYSTEM;
    } else if (IS_REMOVE_WORKSPACE_CMD(stringified_command_type)) {
        return REMOVE_WORKSPACE;
    } else if (IS_REMOVE_REMOTE_SYSTEM_CMD(stringified_command_type)) {
        return REMOVE_REMOTE_SYSTEM;
    }

    return OTHER_RESYNC_COMMAND_TYPE;
}

char *
resync_command_type_to_string(ResyncCommandType command_type)
{
    switch (command_type) {
        case ADD_WORKSPACE:
            return CMD_TYPE_ADD_WORKSPACE;
        case ADD_REMOTE_SYSTEM:
            return CMD_TYPE_ADD_REMOTE_SYSTEM;
        case REMOVE_WORKSPACE:
            return CMD_TYPE_REMOVE_WORKSPACE;
        case REMOVE_REMOTE_SYSTEM:
            return CMD_TYPE_REMOVE_REMOTE_SYSTEM;
        default:
            return NULL;
    }
}

ConnectionType
string_to_connection_type(const char *stringified_connection_type)
{
    if (IS_SSH_CONNECTION_TYPE(stringified_connection_type)) {
        return SSH;
    } else if (IS_SSH_HOST_ALIAS_CONNECTION_TYPE(stringified_connection_type)) {
        return SSH_HOST_ALIAS;
    } else if (IS_RSYNC_DAEMON_CONNECTION_TYPE(stringified_connection_type)) {
        return RSYNC_DAEMON;
    }

    return OTHER_CONNECTION_TYPE;
}

char *
connection_type_to_string(ConnectionType connection_type)
{
    switch (connection_type) {
        case SSH:
            return CONNECTION_TYPE_SSH;
        case SSH_HOST_ALIAS:
            return CONNECTION_TYPE_SSH_HOST_ALIAS;
        case RSYNC_DAEMON:
            return CONNECTION_TYPE_RSYNC_DAEMON;
        default:
            return NULL;
    }
}

static void
destroy_ssh_connection_info_struct(SshConnectionInformation **info)
{
    if (info == NULL || *info == NULL) {
        return;
    }

    if ((*info)->username != NULL) {
        DO_FREE((*info)->username);
    }

    if ((*info)->path_to_identity_file != NULL) {
        DO_FREE((*info)->path_to_identity_file);
    }

    if ((*info)->hostname != NULL) {
        DO_FREE((*info)->hostname);
    }

    DO_FREE(*info);
}

static void
destroy_rsync_daemon_connection_info_struct(RsyncConnectionInformation **info) {
    if (info == NULL || *info == NULL) {
        return;
    }

    if ((*info)->username != NULL) {
        DO_FREE((*info)->username);
    }

    if ((*info)->hostname != NULL) {
        DO_FREE((*info)->hostname);
    }

    DO_FREE(*info);
}

static void
destroy_remote_system_struct(RemoteWorkspaceMetadata **remote_system_ptr)
{
    if (remote_system_ptr == NULL || *remote_system_ptr == NULL) {
        return;
    }

    RemoteWorkspaceMetadata *remote_system = *remote_system_ptr;

    if (remote_system->remote_workspace_root_path != NULL) {
        DO_FREE(remote_system->remote_workspace_root_path);
    }

    switch (remote_system->connection_type) {
        case SSH:
            destroy_ssh_connection_info_struct(&remote_system->connection_information.sshConnectionInformation);
            break;
        case SSH_HOST_ALIAS:
            if (remote_system->connection_information.ssh_host_alias != NULL) {
                DO_FREE(remote_system->connection_information.ssh_host_alias);
            }
            break;
        case RSYNC_DAEMON:
            destroy_rsync_daemon_connection_info_struct(&remote_system->connection_information.rsyncConnectionInformation);
            break;
        default:
            fatal_custom_error("Cannot free connection info's memory as the connection type is unknown");
    }

    DO_FREE(remote_system);
}


static cJSON *
stringified_json_to_cJSON(const char *stringified_json_object, char **error_msg)
{
    cJSON *object = cJSON_Parse(stringified_json_object);
    if (object == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            *error_msg = format_string("Error parsing JSON object before: \n'%s'", error_ptr);
        }
        *error_msg = format_string("Error parsing stringified JSON object: \n'%s'", stringified_json_object);
    }

    return object;
}

static SshConnectionInformation *
extract_ssh_connection_information(const cJSON *connection_info_object, char **error_msg)
{
    cJSON *entry;
    SshConnectionInformation *sshConnectionInformation = (SshConnectionInformation *) do_calloc(1, sizeof(SshConnectionInformation));

    entry = cJSON_GetObjectItemCaseSensitive(connection_info_object, KEY_SSH_CONNECTION_INFO_HOSTNAME);
    if (STRING_VAL_EXISTS(entry)) {
        sshConnectionInformation->hostname = resync_strdup(entry->valuestring);
    } else {
        *error_msg = JSON_MEMBER_MISSING(KEY_SSH_CONNECTION_INFO_HOSTNAME, connection_info_object);
        goto error_out;
    }

    entry = cJSON_GetObjectItemCaseSensitive(connection_info_object, KEY_SSH_CONNECTION_INFO_USERNAME);
    if (STRING_VAL_EXISTS(entry)) {
        sshConnectionInformation->username = resync_strdup(entry->valuestring);
    }

    entry = cJSON_GetObjectItemCaseSensitive(connection_info_object, KEY_SSH_CONNECTION_INFO_IDENTITY_FILE);
    if (STRING_VAL_EXISTS(entry)) {
        sshConnectionInformation->path_to_identity_file = resync_strdup(entry->valuestring);
    }

    return sshConnectionInformation;

error_out:
    DO_FREE(sshConnectionInformation);
    return NULL;
}

static RsyncConnectionInformation *
extract_rsync_daemon_connection_information(const cJSON *connection_info_object, char **error_msg)
{
    cJSON *entry;
    RsyncConnectionInformation *rsyncConnectionInformation = (RsyncConnectionInformation *) do_malloc(sizeof(RsyncConnectionInformation));

    entry = cJSON_GetObjectItemCaseSensitive(connection_info_object, KEY_RSYNC_DAEMON_CONNECTION_INFO_USERNAME);
    if (STRING_VAL_EXISTS(entry)) {
        rsyncConnectionInformation->username = resync_strdup(entry->valuestring);
    }

    entry = cJSON_GetObjectItemCaseSensitive(connection_info_object, KEY_RSYNC_DAEMON_CONNECTION_INFO_PORT);
    if (entry != NULL) {

        if (entry->valuestring != NULL) {
            *error_msg = format_string(
                    "Invalid port value '%s' found in connection info struct: \n'%s'",
                    entry->valuestring,
                    JSON_PRINT(connection_info_object)
            );
            goto error_out;
        }

        if (entry->valueint < MIN_PORT_NUMBER || entry->valueint > MAX_PORT_NUMBER) {
            *error_msg = format_string(
                    "Port value '%d' is outside of range [%d, %d]: \n'%s'",
                    entry->valueint,
                    MIN_PORT_NUMBER,
                    MAX_PORT_NUMBER,
                    JSON_PRINT(connection_info_object)
            );
            goto error_out;
        }

        rsyncConnectionInformation->port = entry->valueint;
    } else {
        rsyncConnectionInformation->port = -1;
    }

    entry = cJSON_GetObjectItemCaseSensitive(connection_info_object, KEY_RSYNC_DAEMON_CONNECTION_INFO_HOSTNAME);
    if (STRING_VAL_EXISTS(entry)) {
        rsyncConnectionInformation->hostname = resync_strdup(entry->valuestring);
    } else {
        *error_msg = JSON_MEMBER_MISSING(KEY_RSYNC_DAEMON_CONNECTION_INFO_HOSTNAME, connection_info_object);
        goto error_out;
    }

    return rsyncConnectionInformation;

error_out:
    DO_FREE(rsyncConnectionInformation);
    return NULL;
}

static char *
extract_ssh_host_alias_connection_information(const cJSON *connection_info_object, char **error_msg)
{
    cJSON *entry = cJSON_GetObjectItemCaseSensitive(connection_info_object, KEY_SSH_HOST_ALIAS_CONNECTION_INFO_NAME);
    if (!STRING_VAL_EXISTS(entry)) {
        *error_msg = JSON_MEMBER_MISSING(KEY_SSH_HOST_ALIAS_CONNECTION_INFO_NAME, connection_info_object);
        return NULL;
    }

    return resync_strdup(entry->valuestring);
}

static RemoteWorkspaceMetadata *
remote_system_json_to_remote_workspace_metadata(const cJSON *remote_system_object, char **error_msg)
{
    cJSON *entry;
    RemoteWorkspaceMetadata *remote_ws_metadata = (RemoteWorkspaceMetadata *) do_calloc(1, sizeof(RemoteWorkspaceMetadata));

    entry = cJSON_GetObjectItemCaseSensitive(remote_system_object, KEY_REMOTE_WORKSPACE_ROOT_PATH);
    if (STRING_VAL_EXISTS(entry)) {
        remote_ws_metadata->remote_workspace_root_path = resync_strdup(entry->valuestring);
    } else {
        DO_FREE(remote_ws_metadata);
        *error_msg = JSON_MEMBER_MISSING(KEY_REMOTE_WORKSPACE_ROOT_PATH, remote_system_object);
        return NULL;
    }

    entry = cJSON_GetObjectItemCaseSensitive(remote_system_object, KEY_CONNECTION_TYPE);
    if (!STRING_VAL_EXISTS(entry)) {
        *error_msg = JSON_MEMBER_MISSING(KEY_CONNECTION_TYPE, remote_system_object);
        return NULL;
    }

    char *connection_type = entry->valuestring;
    if (IS_SSH_CONNECTION_TYPE(connection_type)) {
        remote_ws_metadata->connection_type = SSH;
    } else if (IS_SSH_HOST_ALIAS_CONNECTION_TYPE(connection_type)) {
        remote_ws_metadata->connection_type = SSH_HOST_ALIAS;
    } else if (IS_RSYNC_DAEMON_CONNECTION_TYPE(connection_type)) {
        remote_ws_metadata->connection_type = RSYNC_DAEMON;
    } else {
        *error_msg = format_string("Invalid connection type '%s' specified for remote system: \n'%s'", connection_type, JSON_PRINT(remote_system_object));
        return NULL;
    }

    entry = cJSON_GetObjectItemCaseSensitive(remote_system_object, KEY_CONNECTION_INFORMATION);
    if (entry == NULL) {
        *error_msg = JSON_MEMBER_MISSING(KEY_CONNECTION_INFORMATION, remote_system_object);
        goto error_out;
    }

    void *info;
    switch (remote_ws_metadata->connection_type) {
        case SSH:
            info = extract_ssh_connection_information(entry, error_msg);
            remote_ws_metadata->connection_information.sshConnectionInformation = (SshConnectionInformation *) info;
            break;
        case SSH_HOST_ALIAS:
            info = extract_ssh_host_alias_connection_information(entry, error_msg);
            remote_ws_metadata->connection_information.ssh_host_alias = (char *) info;
            break;
        case RSYNC_DAEMON:
            info = extract_rsync_daemon_connection_information(entry, error_msg);
            remote_ws_metadata->connection_information.rsyncConnectionInformation = (RsyncConnectionInformation *) info;
            break;
        default:
            *error_msg = format_string("Unknown error related to connection type of '%s' occurred", JSON_PRINT(remote_system_object));
            goto error_out;
    }

    if (info == NULL) {
        goto error_out;
    }

    return remote_ws_metadata;

error_out:
    DO_FREE(remote_ws_metadata->remote_workspace_root_path);
    DO_FREE(remote_ws_metadata);
    return NULL;
}

WorkspaceInformation *
stringified_json_to_workspace_information(const char *stringified_json_object, char **error_msg)
{
    cJSON *json_object = stringified_json_to_cJSON(stringified_json_object, error_msg);
    if (json_object == NULL) {
        return NULL;
    }

    return json_to_workspace_information(json_object, error_msg);
}

WorkspaceInformation *
json_to_workspace_information(const cJSON *json_object, char **error_msg)
{
    WorkspaceInformation *workspace_information = (WorkspaceInformation *) do_calloc(1, sizeof(WorkspaceInformation));
    workspace_information->remote_systems = NULL;

    cJSON *entry = cJSON_GetObjectItemCaseSensitive(json_object, KEY_LOCAL_WORKSPACE_ROOT_PATH);
    if (STRING_VAL_EXISTS(entry)) {
        workspace_information->local_workspace_root_path = resync_strdup(entry->valuestring);
    } else {
        DO_FREE(workspace_information);
        *error_msg = JSON_MEMBER_MISSING(KEY_LOCAL_WORKSPACE_ROOT_PATH, json_object);
        return NULL;
    }

    cJSON *remote_system_entry;
    cJSON *remote_systems = cJSON_GetObjectItemCaseSensitive(json_object, KEY_REMOTE_SYSTEMS);

    int remote_systems_counter = 0;
    cJSON_ArrayForEach(remote_system_entry, remote_systems) {
        remote_systems_counter++;

        RemoteWorkspaceMetadata *remote_ws_metadata = remote_system_json_to_remote_workspace_metadata(remote_system_entry, error_msg);
        if (remote_ws_metadata == NULL) {
            goto error_out;
        }

        LL_APPEND(workspace_information->remote_systems, remote_ws_metadata);
    }

    if (remote_systems_counter == 0) {
        *error_msg = format_string(
                "No remote systems (expected key '%s') specified in workspace definition: \n'%s'",
                KEY_REMOTE_SYSTEMS,
                JSON_PRINT(json_object)
        );
        goto error_out;
    }

    return workspace_information;

error_out:
    DO_FREE(workspace_information->local_workspace_root_path);
    RemoteWorkspaceMetadata *del_entry, *del_tmp;
    LL_FOREACH_SAFE(workspace_information->remote_systems, del_entry, del_tmp) {
        LL_DELETE(workspace_information->remote_systems, del_entry);
        destroy_remote_system_struct(&del_entry);
    }
    DO_FREE(workspace_information);
    return NULL;
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
ssh_connection_information_to_json(SshConnectionInformation *ssh_info, char **error_msg)
{
    if (ssh_info == NULL) {
        *error_msg = resync_strdup("SSH connection information struct is missing");
        return NULL;
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
        *error_msg = format_string(
                "Error: SSH connection information struct is missing the required '%s' entry",
                KEY_SSH_CONNECTION_INFO_HOSTNAME
        );
        goto error_out;
    }

    return connection_information;

error_out:
    cJSON_Delete(connection_information);
    return NULL;
}

static cJSON *
rsync_daemon_connection_information_to_json(RsyncConnectionInformation *rsync_daemon_info, char **error_msg)
{
    if (rsync_daemon_info == NULL) {
        *error_msg = resync_strdup("RSYNC daemon connection information struct is missing");
        return NULL;
    }

    cJSON *connection_information = create_json_object();

    if (rsync_daemon_info->username != NULL) {
        cJSON *username = create_json_string(rsync_daemon_info->username);
        cJSON_AddItemToObject(connection_information, KEY_RSYNC_DAEMON_CONNECTION_INFO_USERNAME, username);
    }

    if (rsync_daemon_info->port >= MIN_PORT_NUMBER) {
        if (rsync_daemon_info->port > MAX_PORT_NUMBER) {
            *error_msg = format_string("Invalid port '%d' found", rsync_daemon_info->port);
            goto error_out;
        }

        cJSON *port = create_json_number(rsync_daemon_info->port);
        cJSON_AddItemToObject(connection_information, KEY_RSYNC_DAEMON_CONNECTION_INFO_PORT, port);
    }

    if (rsync_daemon_info->hostname != NULL) {
        cJSON *hostname = create_json_string(rsync_daemon_info->hostname);
        cJSON_AddItemToObject(connection_information, KEY_RSYNC_DAEMON_CONNECTION_INFO_HOSTNAME, hostname);
    } else {
        *error_msg = format_string (
                "Error: RSYNC daemon connection information struct is missing the required '%s' entry",
                KEY_RSYNC_DAEMON_CONNECTION_INFO_HOSTNAME
        );
        goto error_out;
    }

    return connection_information;

error_out:
    cJSON_Delete(connection_information);
    return NULL;
}

static cJSON *
ssh_host_alias_connection_information_to_json(char *ssh_host_alias, char **error_msg)
{
    cJSON *connection_information = create_json_object();

    if (ssh_host_alias != NULL) {
        cJSON *host_alias = create_json_string(ssh_host_alias);
        cJSON_AddItemToObject(connection_information, KEY_SSH_HOST_ALIAS_CONNECTION_INFO_NAME, host_alias);
    } else {
        *error_msg = format_string (
                "Error: SSH host alias connection information is missing required '%s' entry",
                KEY_SSH_HOST_ALIAS_CONNECTION_INFO_NAME
        );
        cJSON_Delete(connection_information);
        return NULL;
    }

    return connection_information;
}

cJSON *
remote_workspace_metadata_to_json(RemoteWorkspaceMetadata *remote_workspace_metadata, char **error_msg)
{
    cJSON *remote_system = create_json_object();

    if (remote_workspace_metadata->remote_workspace_root_path != NULL) {
        cJSON *remote_workspace_root_path = create_json_string(remote_workspace_metadata->remote_workspace_root_path);
        cJSON_AddItemToObject(remote_system, KEY_REMOTE_WORKSPACE_ROOT_PATH, remote_workspace_root_path);
    } else {
        *error_msg = format_string ("Error: Remote workspace struct is missing the required '%s' entry",KEY_REMOTE_WORKSPACE_ROOT_PATH);
        goto error_out;
    }

    cJSON *connection_type;
    cJSON *connection_information;
    switch (remote_workspace_metadata->connection_type) {
        case SSH:
            connection_type = create_json_string(CONNECTION_TYPE_SSH);
            connection_information = ssh_connection_information_to_json(
                    remote_workspace_metadata->connection_information.sshConnectionInformation,
                    error_msg
            );
            break;
        case SSH_HOST_ALIAS:
            connection_type = create_json_string(CONNECTION_TYPE_SSH_HOST_ALIAS);
            connection_information = ssh_host_alias_connection_information_to_json(
                    remote_workspace_metadata->connection_information.ssh_host_alias,
                    error_msg
            );
            break;
        case RSYNC_DAEMON:
            connection_type = create_json_string(CONNECTION_TYPE_RSYNC_DAEMON);
            connection_information = rsync_daemon_connection_information_to_json(
                    remote_workspace_metadata->connection_information.rsyncConnectionInformation,
                    error_msg
            );
            break;
        default:
            *error_msg = format_string(
                    "Unknown error related to connection type '%s' of remote system",
                    remote_workspace_metadata->connection_type
            );
            goto error_out;
    }

    if (connection_information == NULL) {
        goto error_out;
    }

    cJSON_AddItemToObject(remote_system, KEY_CONNECTION_TYPE, connection_type);
    cJSON_AddItemToObject(remote_system, KEY_CONNECTION_INFORMATION, connection_information);

    return remote_system;

error_out:
    cJSON_Delete(remote_system);
    return NULL;
}

char *
workspace_information_to_stringified_json(WorkspaceInformation *workspace_information, char **error_msg)
{
    cJSON *json_object = workspace_information_to_json(workspace_information, error_msg);
    if (json_object == NULL) {
        return NULL;
    }

    char *stringified_json_object = cJSON_Print(json_object);
    cJSON_Delete(json_object);
    return stringified_json_object;
}

cJSON *
workspace_information_to_json(WorkspaceInformation *workspace_information, char **error_msg)
{
    if (workspace_information == NULL) {
        *error_msg = resync_strdup("workspace_information struct is NULL");
        return NULL;
    }

    cJSON *json_ws_information = create_json_object();

    if (workspace_information->local_workspace_root_path == NULL) {
        *error_msg = format_string("workspace_information struct is missing a value for '%s'",KEY_LOCAL_WORKSPACE_ROOT_PATH);
        goto error_out;
    }

    cJSON *local_workspace_root_path = create_json_string(workspace_information->local_workspace_root_path);
    cJSON_AddItemToObject(json_ws_information, KEY_LOCAL_WORKSPACE_ROOT_PATH, local_workspace_root_path);

    cJSON *remote_systems = create_json_array();
    cJSON_AddItemToObject(json_ws_information, KEY_REMOTE_SYSTEMS, remote_systems);

    int remote_systems_counter = 0;
    RemoteWorkspaceMetadata *entry;
    LL_FOREACH(workspace_information->remote_systems, entry) {
        remote_systems_counter++;

        cJSON *remote_system = remote_workspace_metadata_to_json(entry, error_msg);
        if (remote_system == NULL) {
            goto error_out;
        }

        cJSON_AddItemToArray(remote_systems, remote_system);
    }

    if (remote_systems_counter == 0) {
        *error_msg = resync_strdup("workspace information struct does not contain any remote system definitions");
        goto error_out;
    }

    return json_ws_information;

error_out:
    cJSON_Delete(json_ws_information);
    return NULL;
}

ResyncDaemonCommand *
stringified_json_to_resync_daemon_command(const char *stringified_json_object, char **error_msg)
{
    if (stringified_json_object == NULL) {
        *error_msg = resync_strdup("No stringified json object provided");
        return NULL;
    }

    cJSON *json_object = stringified_json_to_cJSON(stringified_json_object, error_msg);
    if (json_object == NULL) {
        return NULL;
    }

    return json_to_resync_daemon_command(json_object, error_msg);
}

ResyncDaemonCommand *
json_to_resync_daemon_command(const cJSON *json_object, char **error_msg)
{
    if (json_object == NULL) {
        *error_msg = resync_strdup("No json object provided");
        return NULL;
    }

    cJSON *entry;
    ResyncDaemonCommand *resync_daemon_command = (ResyncDaemonCommand *) do_calloc(1, sizeof(ResyncDaemonCommand));

    entry = cJSON_GetObjectItemCaseSensitive(json_object, KEY_DAEMON_CMD_TYPE);
    if (!STRING_VAL_EXISTS(entry)) {
        *error_msg = JSON_MEMBER_MISSING(KEY_DAEMON_CMD_TYPE, json_object);
        goto error_out;
    }

    char *stringified_command_type = entry->valuestring;
    if (IS_ADD_WORKSPACE_CMD(stringified_command_type)) {
        resync_daemon_command->command_type = ADD_WORKSPACE;
    } else if (IS_ADD_REMOTE_SYSTEM_CMD(stringified_command_type)) {
        resync_daemon_command->command_type = ADD_REMOTE_SYSTEM;
    } else if (IS_REMOVE_WORKSPACE_CMD(stringified_command_type)) {
        resync_daemon_command->command_type = REMOVE_WORKSPACE;
    } else if (IS_REMOVE_REMOTE_SYSTEM_CMD(stringified_command_type)) {
        resync_daemon_command->command_type = REMOVE_REMOTE_SYSTEM;
    } else {
        *error_msg = format_string(
                "Found invalid command type '%s' in resync_command JSON : \n'%s'",
                stringified_command_type,
                JSON_PRINT(json_object)
        );
        goto error_out;
    }

    cJSON *workspace_info_json = cJSON_GetObjectItemCaseSensitive(json_object, KEY_DAEMON_CMD_WORKSPACE_INFO);
    if (workspace_info_json == NULL) {
        *error_msg = JSON_MEMBER_MISSING(KEY_DAEMON_CMD_WORKSPACE_INFO, json_object);
        goto error_out;
    }

    WorkspaceInformation *ws_info = json_to_workspace_information(workspace_info_json, error_msg);
    if (ws_info == NULL) {
        goto error_out;
    }

    resync_daemon_command->workspace_information = ws_info;
    return resync_daemon_command;

error_out:
    DO_FREE(resync_daemon_command);
    return NULL;
}

char *
resync_daemon_command_to_stringified_json(ResyncDaemonCommand *resync_daemon_command, char **error_msg)
{
    if (resync_daemon_command == NULL) {
        *error_msg = resync_strdup("No resync_daemon_command struct provided");
        return NULL;
    }

    cJSON *json_object = resync_daemon_command_to_json(resync_daemon_command, error_msg);
    if (json_object == NULL) {
        return NULL;
    }

    char *stringified_json_object = JSON_PRINT(json_object);
    cJSON_Delete(json_object);
    return stringified_json_object;
}

cJSON *
resync_daemon_command_to_json(ResyncDaemonCommand *resync_daemon_command, char **error_msg)
{
    if (resync_daemon_command == NULL) {
        *error_msg = resync_strdup("No resync_daemon_command struct provided");
        return NULL;
    }

    cJSON *json_object = create_json_object();

    cJSON *command_type = NULL;
    switch (resync_daemon_command->command_type) {
        case ADD_WORKSPACE:
            command_type = create_json_string(CMD_TYPE_ADD_WORKSPACE);
            break;
        case ADD_REMOTE_SYSTEM:
            command_type = create_json_string(CMD_TYPE_ADD_REMOTE_SYSTEM);
            break;
        case REMOVE_WORKSPACE:
            command_type = create_json_string(CMD_TYPE_REMOVE_WORKSPACE);
            break;
        case REMOVE_REMOTE_SYSTEM:
            command_type = create_json_string(CMD_TYPE_REMOVE_REMOTE_SYSTEM);
            break;
        default:
            *error_msg = resync_strdup("Encountered unknown reSync daemon command");
            goto error_out;
    }
    cJSON_AddItemToObject(json_object, KEY_DAEMON_CMD_TYPE, command_type);

    if (resync_daemon_command->workspace_information == NULL) {
        *error_msg = format_string("resync_daemon_command struct is missing a value for member: '%s'", KEY_DAEMON_CMD_WORKSPACE_INFO);
        goto error_out;
    }

    cJSON *ws_info = workspace_information_to_json(resync_daemon_command->workspace_information, error_msg);
    if (ws_info == NULL) {
        goto error_out;
    }
    cJSON_AddItemToObject(json_object, KEY_DAEMON_CMD_WORKSPACE_INFO, ws_info);

    return json_object;

error_out:
    cJSON_Delete(json_object);
    return NULL;
}
