#include "../mappers.h"
#include "mapper_private.h"
#include "util/json_utils.h"

#include "../../../lib/ulist.h"
#include "../../util/fs_util.h"

#define MIN_PORT_NUMBER 0
#define MAX_PORT_NUMBER 65535

static cJSON *
sshConnectionInformation_to_cJSON(SshConnectionInformation *connection_information, char **error_msg)
{
    if (connection_information == NULL) {
        SET_ERROR_MSG(error_msg, "SSH connection information is missing!");
        return NULL;
    }

    cJSON *ssh_connection_info_json = create_json_object();

    if (connection_information->hostname != NULL) {
        cJSON *hostname = create_json_string(connection_information->hostname);
        cJSON_AddItemToObject(ssh_connection_info_json, WS_INFO_RSMD_SSH_CI_HOSTNAME, hostname);
    } else {
        SET_ERROR_MSG(error_msg, "SSH connection info is missing the hostname information!");
        goto error_out;
    }

    if (connection_information->username != NULL) {
        cJSON *username = create_json_string(connection_information->username);
        cJSON_AddItemToObject(ssh_connection_info_json, WS_INFO_RSMD_SSH_CI_USERNAME, username);
    }

    if (connection_information->path_to_identity_file != NULL) {

        if (!validate_absolute_path(connection_information->path_to_identity_file, error_msg)) {
            SET_ERROR_MSG_WITH_CAUSE_RAW(
                    error_msg,
                    format_string("Identity file path '%s' is not absolute", connection_information->path_to_identity_file),
                    error_msg
            );
            goto error_out;
        }

        if (!validate_file_exists(connection_information->path_to_identity_file, error_msg)) {
            SET_ERROR_MSG_RAW(
                    error_msg,
                    format_string("Identity file '%s' does not exist!", connection_information->path_to_identity_file)
            );
            goto error_out;
        }

        cJSON *identity_file = create_json_string(connection_information->path_to_identity_file);
        cJSON_AddItemToObject(ssh_connection_info_json, WS_INFO_RSMD_SSH_CI_IDENTITY_FILE, identity_file);
    }

    return ssh_connection_info_json;

error_out:
    cJSON_Delete(ssh_connection_info_json);
    return NULL;
}

static cJSON *
rsyncConnectionInformation_to_cJSON(RsyncConnectionInformation *connection_information, char **error_msg)
{
    if (connection_information == NULL) {
        SET_ERROR_MSG(error_msg, "Rsync connection information is missing!");
        return NULL;
    }

    cJSON *rsync_connection_info_json = create_json_object();

    if (connection_information->hostname != NULL) {
        cJSON *hostname = create_json_string(connection_information->hostname);
        cJSON_AddItemToObject(rsync_connection_info_json, WS_INFO_RSMD_RSYNC_CI_HOSTNAME, hostname);
    } else {
        SET_ERROR_MSG(error_msg, "Rsync connection information is missing the hostname!");
        goto error_out;
    }

    if (connection_information->username != NULL) {
        cJSON *username = create_json_string(connection_information->username);
        cJSON_AddItemToObject(rsync_connection_info_json, WS_INFO_RSMD_RSYNC_CI_USERNAME, username);
    }

    if (connection_information->port != 0) {
        if (connection_information->port >= MIN_PORT_NUMBER && connection_information->port <= MAX_PORT_NUMBER) {
            cJSON *port = create_json_number(connection_information->port);
            cJSON_AddItemToObject(rsync_connection_info_json, WS_INFO_RSMD_RSYNC_CI_PORT, port);
        } else {
            SET_ERROR_MSG_RAW(
                    error_msg,
                    format_string(
                            "Port '%d' is not a positive integer in the range [%d, %d]",
                            connection_information->port,
                            MIN_PORT_NUMBER,
                            MAX_PORT_NUMBER
                    )
            );
            goto error_out;
        }
    }

    return rsync_connection_info_json;

error_out:
    cJSON_Delete(rsync_connection_info_json);
    return NULL;
}

static cJSON *
sshHostAliasConnectionInformation_to_cJSON(char *ssh_host_alias, char **error_msg)
{
    if (ssh_host_alias == NULL) {
        SET_ERROR_MSG(error_msg, "SSH host alias information is missing!");
        return NULL;
    }

    cJSON *ssh_host_alias_connection_info_json = create_json_object();
    cJSON_AddItemToObject(
            ssh_host_alias_connection_info_json,
            WS_INFO_RSMD_SSH_HOST_ALIAS_ALIAS,
            create_json_string(ssh_host_alias)
    );

    return ssh_host_alias_connection_info_json;
}

cJSON *
remoteWorkspaceMetadata_to_cjson(RemoteWorkspaceMetadata *remote_ws_metadata, char **error_msg)
{
    if (remote_ws_metadata == NULL) {
        SET_ERROR_MSG(error_msg, "RemoteWorkspaceMetadata struct is NULL");
        return NULL;
    }

    cJSON *remote_ws_metadata_json = create_json_object();

    if (remote_ws_metadata->remote_workspace_root_path != NULL) {

        if (!validate_absolute_path(remote_ws_metadata->remote_workspace_root_path, error_msg)) {
            SET_ERROR_MSG_WITH_CAUSE_RAW(
                    error_msg,
                    format_string("Remote path '%s' is not absolute", remote_ws_metadata->remote_workspace_root_path),
                    error_msg
            );
            goto error_out;
        }

        cJSON *remote_ws_root_path = create_json_string(remote_ws_metadata->remote_workspace_root_path);
        cJSON_AddItemToObject(remote_ws_metadata_json, WS_INFO_RSMD_REMOTE_WORKSPACE_ROOT_PATH, remote_ws_root_path);
    } else {
        SET_ERROR_MSG(error_msg, "RemoteWorkspaceMetadata does not specify a value for the remote workspace root path!");
        goto error_out;
    }

    char *stringified_connection_type = connection_type_to_string(remote_ws_metadata->connection_type);
    if (stringified_connection_type == NULL) {
        SET_ERROR_MSG(error_msg, "RemoteWorkspaceMetadata struct specifies an unsupported connection type!");
        goto error_out;
    }
    cJSON_AddItemToObject(remote_ws_metadata_json, WS_INFO_RSMD_CONNECTION_TYPE, create_json_string(stringified_connection_type));

    cJSON *connection_information;
    switch (remote_ws_metadata->connection_type) {
        case SSH:
            connection_information = sshConnectionInformation_to_cJSON(
                    remote_ws_metadata->connection_information.ssh_connection_information, error_msg
            );
            break;
        case SSH_HOST_ALIAS:
            connection_information = sshHostAliasConnectionInformation_to_cJSON(
                    remote_ws_metadata->connection_information.ssh_host_alias, error_msg
            );
            break;
        case RSYNC_DAEMON:
            connection_information = rsyncConnectionInformation_to_cJSON(
                    remote_ws_metadata->connection_information.rsync_connection_information, error_msg
            );
            break;
        case OTHER_CONNECTION_TYPE:
        default:
            SET_ERROR_MSG(error_msg, "RemoteWorkspaceMetadata struct specifies an unsupported connection type!");
            goto error_out;
    }

    if (connection_information == NULL) {
        goto error_out;
    }

    cJSON_AddItemToObject(remote_ws_metadata_json, WS_INFO_RSMD_CONNECTION_INFORMATION, connection_information);

    return remote_ws_metadata_json;

error_out:
    cJSON_Delete(remote_ws_metadata_json);
    return NULL;
}

WorkspaceInformation *
cjson_to_workspaceInformation(const cJSON *json_ws_info, char **error_msg)
{
    //TODO:
    return NULL;
}

WorkspaceInformation *
stringified_json_to_workspaceInformation(const char *stringified_json_ws_info, char **error_msg)
{
    if (stringified_json_ws_info == NULL) {
        SET_ERROR_MSG(error_msg, "No stringified json object representation of a workspaces information specified");
        return NULL;
    }

    cJSON *json_ws_info = stringified_cjson_to_cjson(stringified_json_ws_info, error_msg);
    if (json_ws_info == NULL) {
        return NULL;
    }

    return cjson_to_workspaceInformation(json_ws_info, error_msg);
}

cJSON *
workspaceInformation_to_cjson(WorkspaceInformation *ws_info, char **error_msg)
{
    if (ws_info == NULL) {
        SET_ERROR_MSG(error_msg, "Workspace information struct is NULL");
        return NULL;
    }

    cJSON *ws_info_json = create_json_object();

    if (ws_info->local_workspace_root_path != NULL) {

        if (!validate_absolute_path(ws_info->local_workspace_root_path, error_msg)) {
            SET_ERROR_MSG_WITH_CAUSE_RAW(
                    error_msg,
                    format_string("Local workspace root path '%s' is not absolute", ws_info->local_workspace_root_path),
                    error_msg
            );
            goto error_out;
        }

        if (!validate_directory_exists(ws_info->local_workspace_root_path, error_msg)) {
            SET_ERROR_MSG_RAW(
                    error_msg,
                    format_string("Workspace with absolute path '%s' does not exist!", ws_info->local_workspace_root_path)
            );
            goto error_out;
        }

        cJSON *local_ws_root_path = create_json_string(ws_info->local_workspace_root_path);
        cJSON_AddItemToObject(ws_info_json, WS_INFO_KEY_LOCAL_WORKSPACE_ROOT_PATH, local_ws_root_path);
    } else {
        SET_ERROR_MSG(error_msg, "WorkspaceInformation struct does not specify a value for the local workspace root path!");
        goto error_out;
    }

    cJSON *remote_systems_array = create_json_array();
    cJSON_AddItemToObject(ws_info_json, WS_INFO_KEY_REMOTE_SYSTEMS, remote_systems_array);

    int remote_systems_counter = 0;
    RemoteWorkspaceMetadata *entry;
    LL_FOREACH(ws_info->remote_systems, entry) {
        remote_systems_counter++;

        cJSON *remote_system_array_entry = remoteWorkspaceMetadata_to_cjson(entry, error_msg);
        if (remote_system_array_entry == NULL) {
            goto error_out;
        }

        cJSON_AddItemToArray(remote_systems_array, remote_system_array_entry);
    }

    if (remote_systems_counter == 0) {
        SET_ERROR_MSG(error_msg, "WorkspaceInformation struct does not contain any remote system definitions!");
        goto error_out;
    }

    return ws_info_json;

error_out:
    cJSON_Delete(ws_info_json);
    return NULL;
}

char *
workspaceInformation_to_stringified_json(WorkspaceInformation *ws_info, char **error_msg)
{
    cJSON *json_ws_info = workspaceInformation_to_cjson(ws_info, error_msg);
    if (json_ws_info == NULL) {
        return NULL;
    }

    char *stringified_json_ws_info = cJSON_Print(json_ws_info);
    if (stringified_json_ws_info == NULL) {
        SET_ERROR_MSG(error_msg, "Unable to stringify the JSON representation of the workspace information");
        cJSON_Delete(json_ws_info);
        return NULL;
    }

    cJSON_Delete(json_ws_info);
    return stringified_json_ws_info;
}

