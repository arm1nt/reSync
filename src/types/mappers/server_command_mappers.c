#include "../mappers.h"
#include "util/json_utils.h"
#include "mapper_private.h"

static cJSON *
ssh_id_info_to_cjson_object(RemoveRemoteSystemMetadata *rm_remote_system_md, char **error_msg)
{
    cJSON *remote_system_id_info = create_json_object();

    if (rm_remote_system_md->remote_system_id_information.hostname != NULL) {
        cJSON *hostname = create_json_string(rm_remote_system_md->remote_system_id_information.hostname);
        cJSON_AddItemToObject(remote_system_id_info, RM_RS_MD_KEY_HOSTNAME, hostname);
    } else {
        SET_ERROR_MSG(error_msg, "The hostname name is missing!");
        goto error_out;
    }

    return remote_system_id_info;

error_out:
    cJSON_Delete(remote_system_id_info);
    return NULL;
}

static char *
cjson_object_to_ssh_id_info(const cJSON *json_object, char **error_msg)
{
    cJSON *entry = cJSON_GetObjectItemCaseSensitive(json_object, RM_RS_MD_KEY_HOSTNAME);
    if (!STRING_VAL_EXISTS(entry)) {
        SET_ERROR_MSG_RAW(error_msg, JSON_MEMBER_MISSING(RM_RS_MD_KEY_HOSTNAME, json_object));
        return NULL;
    }

    return resync_strdup(entry->valuestring);
}

static cJSON *
rsync_daemon_id_info_to_cjson_object(RemoveRemoteSystemMetadata *rm_remote_system_md, char **error_msg)
{
    cJSON *remote_system_id_info = create_json_object();

    if (rm_remote_system_md->remote_system_id_information.hostname != NULL) {
        cJSON *hostname = create_json_string(rm_remote_system_md->remote_system_id_information.hostname);
        cJSON_AddItemToObject(remote_system_id_info, RM_RS_MD_KEY_HOSTNAME, hostname);
    } else {
        SET_ERROR_MSG(error_msg, "The hostname name is missing!");
        goto error_out;
    }

    return remote_system_id_info;

error_out:
    cJSON_Delete(remote_system_id_info);
    return NULL;
}

static char *
cjson_object_to_rsync_daemon_id_info(const cJSON *json_object, char **error_msg)
{
    cJSON *entry = cJSON_GetObjectItemCaseSensitive(json_object, RM_RS_MD_KEY_HOSTNAME);
    if (!STRING_VAL_EXISTS(entry)) {
        SET_ERROR_MSG_RAW(error_msg, JSON_MEMBER_MISSING(RM_RS_MD_KEY_HOSTNAME, json_object));
        return NULL;
    }

    return resync_strdup(entry->valuestring);
}

static cJSON *
ssh_host_alias_id_info_to_cjson_object(RemoveRemoteSystemMetadata *rm_remote_system_md, char **error_msg)
{
    cJSON *remote_system_id_info = create_json_object();

    if (rm_remote_system_md->remote_system_id_information.ssh_host_alias != NULL) {
        cJSON *alias = create_json_string(rm_remote_system_md->remote_system_id_information.ssh_host_alias);
        cJSON_AddItemToObject(remote_system_id_info, RM_RS_MD_KEY_SSH_HOST_ALIAS, alias);
    } else {
        SET_ERROR_MSG(error_msg, "The ssh host alias is missing!");
        goto error_out;
    }

    return remote_system_id_info;

error_out:
    cJSON_Delete(remote_system_id_info);
    return NULL;
}

static char *
cjson_object_to_ssh_host_alias_id_info(const cJSON *json_object, char **error_msg)
{
    cJSON *entry = cJSON_GetObjectItemCaseSensitive(json_object, RM_RS_MD_KEY_SSH_HOST_ALIAS);
    if (!STRING_VAL_EXISTS(entry)) {
        SET_ERROR_MSG_RAW(error_msg, JSON_MEMBER_MISSING(RM_RS_MD_KEY_SSH_HOST_ALIAS, json_object));
        return NULL;
    }

    return resync_strdup(entry->valuestring);
}

static cJSON *
removeRemoteSystemMetadata_to_cjson(RemoveRemoteSystemMetadata *rm_remote_system_md, char **error_msg)
{
    if (rm_remote_system_md == NULL) {
        SET_ERROR_MSG(error_msg, "No metadata about the remote system to be removed specified!");
        return NULL;
    }

    cJSON *rm_remote_system_md_json = create_json_object();

    if (rm_remote_system_md->local_workspace_root_path != NULL) {
        cJSON *local_ws_root_path = create_json_string(rm_remote_system_md->local_workspace_root_path);
        cJSON_AddItemToObject(rm_remote_system_md_json, RM_RS_KEY_LOCAL_WORKSPACE_ROOT_PATH, local_ws_root_path);
    } else {
        SET_ERROR_MSG(error_msg, "Local workspace root path is missing!");
        goto error_out;
    }

    if (rm_remote_system_md->remote_workspace_root_path != NULL) {
        cJSON *remote_ws_root_path = create_json_string(rm_remote_system_md->remote_workspace_root_path);
        cJSON_AddItemToObject(rm_remote_system_md_json, RM_RS_KEY_REMOTE_WORKSPACE_ROOT_PATH, remote_ws_root_path);
    } else {
        SET_ERROR_MSG(error_msg, "Remote workspace root path is missing!");
        goto error_out;
    }

    char *stringified_connection_type = connection_type_to_string(rm_remote_system_md->connection_type);
    if (stringified_connection_type == NULL) {
        SET_ERROR_MSG(error_msg, "Remote system metadata specifies unsupported connection type!");
        goto error_out;
    }
    cJSON_AddItemToObject(rm_remote_system_md_json, RM_RS_KEY_CONNECTION_TYPE, create_json_string(stringified_connection_type));

    cJSON *remote_system_id_info_json = NULL;
    switch (rm_remote_system_md->connection_type) {
        case SSH:
            remote_system_id_info_json = ssh_id_info_to_cjson_object(rm_remote_system_md, error_msg);
            break;
        case SSH_HOST_ALIAS:
            remote_system_id_info_json = ssh_host_alias_id_info_to_cjson_object(rm_remote_system_md, error_msg);
            break;
        case RSYNC_DAEMON:
            remote_system_id_info_json = rsync_daemon_id_info_to_cjson_object(rm_remote_system_md, error_msg);
            break;
        case OTHER_CONNECTION_TYPE:
        default:
            SET_ERROR_MSG(error_msg, "Remote system metadata specifies unsupported connection type!");
            goto error_out;
    }

    if (remote_system_id_info_json == NULL) {
        goto error_out;
    }

    cJSON_AddItemToObject(rm_remote_system_md_json, RM_RS_KEY_REMOTE_SYSTEM_ID_INFORMATION, remote_system_id_info_json);

    return rm_remote_system_md_json;

error_out:
    cJSON_Delete(rm_remote_system_md_json);
    return NULL;
}

static RemoveRemoteSystemMetadata *
cjson_to_removeRemoteSystemMetadata(const cJSON *rm_remove_system_md_json, char **error_msg)
{
    cJSON *entry;
    RemoveRemoteSystemMetadata *rm_remote_system_md = (RemoveRemoteSystemMetadata *) do_calloc(1, sizeof(RemoveRemoteSystemMetadata));

    entry = cJSON_GetObjectItemCaseSensitive(rm_remove_system_md_json, RM_RS_KEY_LOCAL_WORKSPACE_ROOT_PATH);
    if (STRING_VAL_EXISTS(entry)) {
        rm_remote_system_md->local_workspace_root_path = resync_strdup(entry->valuestring);
    } else {
        SET_ERROR_MSG_RAW(error_msg, JSON_MEMBER_MISSING(RM_RS_KEY_LOCAL_WORKSPACE_ROOT_PATH, rm_remove_system_md_json));
        goto error_out;
    }

    entry = cJSON_GetObjectItemCaseSensitive(rm_remove_system_md_json, RM_RS_KEY_REMOTE_WORKSPACE_ROOT_PATH);
    if (STRING_VAL_EXISTS(entry)) {
        rm_remote_system_md->remote_workspace_root_path = resync_strdup(entry->valuestring);
    } else {
        SET_ERROR_MSG_RAW(error_msg, JSON_MEMBER_MISSING(RM_RS_KEY_REMOTE_WORKSPACE_ROOT_PATH, rm_remove_system_md_json));
        goto error_out;
    }

    entry = cJSON_GetObjectItemCaseSensitive(rm_remove_system_md_json, RM_RS_KEY_CONNECTION_TYPE);
    if (!STRING_VAL_EXISTS(entry)) {
        SET_ERROR_MSG_RAW(error_msg, JSON_MEMBER_MISSING(RM_RS_KEY_CONNECTION_TYPE, rm_remove_system_md_json));
        goto error_out;
    }

    ConnectionType connection_type = string_to_connection_type(entry->valuestring);
    if (connection_type == OTHER_CONNECTION_TYPE) {
        SET_ERROR_MSG_RAW(
                error_msg,
                format_string("Command metadata specifies unsupported connection type: \n'%s'", cJSON_Print(rm_remove_system_md_json))
        );
        goto error_out;
    }
    rm_remote_system_md->connection_type = connection_type;

    entry = cJSON_GetObjectItemCaseSensitive(rm_remove_system_md_json, RM_RS_KEY_REMOTE_SYSTEM_ID_INFORMATION);
    if (entry == NULL) {
        SET_ERROR_MSG_RAW(error_msg, JSON_MEMBER_MISSING(RM_RS_KEY_REMOTE_SYSTEM_ID_INFORMATION, rm_remove_system_md_json));
        goto error_out;
    }

    void *remote_system_id_info;
    switch (connection_type) {
        case SSH:
            remote_system_id_info = cjson_object_to_ssh_id_info(entry, error_msg);
            rm_remote_system_md->remote_system_id_information.hostname = (char *) remote_system_id_info;
            break;
        case SSH_HOST_ALIAS:
            remote_system_id_info = cjson_object_to_ssh_host_alias_id_info(entry, error_msg);
            rm_remote_system_md->remote_system_id_information.ssh_host_alias = (char *) remote_system_id_info;
            break;
        case RSYNC_DAEMON:
            remote_system_id_info = cjson_object_to_rsync_daemon_id_info(entry, error_msg);
            rm_remote_system_md->remote_system_id_information.hostname = (char *) remote_system_id_info;
            break;
        case OTHER_CONNECTION_TYPE:
        default:
            SET_ERROR_MSG_RAW(
                    error_msg,
                    format_string("Command metadata specifies unsupported connection type: \n'%s'", cJSON_Print(rm_remove_system_md_json))
            );
            goto error_out;
    }

    if (remote_system_id_info == NULL) {
        return NULL;
    }

    return rm_remote_system_md;

error_out:
    destroy_removeRemoteSystemMetadata(&rm_remote_system_md);
    return NULL;
}

ResyncServerCommand *
cjson_to_resyncServerCommand(const cJSON *json_command, char **error_msg)
{
    if (json_command == NULL) {
        SET_ERROR_MSG(error_msg, "No JSON representation of a resync server command specified!");
        return NULL;
    }

    cJSON *entry;
    ResyncServerCommand *command = (ResyncServerCommand *) do_calloc(1, sizeof(ResyncServerCommand));

    entry = cJSON_GetObjectItemCaseSensitive(json_command, CMD_KEY_COMMAND_TYPE);
    if (!STRING_VAL_EXISTS(entry)) {
        SET_ERROR_MSG_RAW(error_msg, JSON_MEMBER_MISSING(CMD_KEY_COMMAND_TYPE, json_command));
        goto error_out;
    }

    ResyncServerCommandType command_type = string_to_resync_server_command_type(entry->valuestring);
    if (command_type == OTHER_RESYNC_SERVER_COMMAND_TYPE) {
        SET_ERROR_MSG_RAW(
                error_msg,
                format_string("JSON representation of command specifies an unsupported command type: \n'%s'", cJSON_Print(json_command))
        );
        goto error_out;
    }
    command->command_type = command_type;

    entry = cJSON_GetObjectItemCaseSensitive(json_command, CMD_KEY_COMMAND_METADATA);
    if (entry == NULL) {
        SET_ERROR_MSG(error_msg, "JSON representation of command does not contain any command information data");
        goto error_out;
    }

    void *res;
    switch (command_type) {
        case ADD_WORKSPACE:
        case ADD_REMOTE_SYSTEM:
            res = cjson_to_workspaceInformation(entry, error_msg);
            command->command_metadata.workspace_information = (WorkspaceInformation *) res;
            break;
        case REMOVE_WORKSPACE:
            res = resync_strdup(entry->valuestring);
            command->command_metadata.local_workspace_root_path = (char *) res;
            break;
        case REMOVE_REMOTE_SYSTEM:
            res = cjson_to_removeRemoteSystemMetadata(entry, error_msg);
            command->command_metadata.rm_remote_system_md = (RemoveRemoteSystemMetadata *) res;
            break;
        case OTHER_RESYNC_SERVER_COMMAND_TYPE:
        default:
            SET_ERROR_MSG_RAW(
                    error_msg,
                    format_string("JSON representation of command specifies an unsupported command type: \n'%s'", cJSON_Print(json_command))
            );
            goto error_out;
    }

    if (res == NULL) {
        goto error_out;
    }

    return command;

error_out:
    destroy_resyncServerCommand(&command);
    return NULL;
}

ResyncServerCommand *
stringified_json_to_resyncServerCommand(const char *stringified_json_command, char **error_msg)
{
    if (stringified_json_command == NULL) {
        SET_ERROR_MSG(error_msg, "No stringified json object representation of a resync server command specified!");
        return NULL;
    }

    cJSON *json_command = stringified_cjson_to_cjson(stringified_json_command, error_msg);
    if (json_command == NULL) {
        return NULL;
    }

    return cjson_to_resyncServerCommand(json_command, error_msg);
}

cJSON *
resyncServerCommand_to_cjson(ResyncServerCommand *command, char **error_msg)
{
    if (command == NULL) {
        SET_ERROR_MSG(error_msg, "No ResyncServerCommand struct specified");
        return NULL;
    }

    cJSON *command_json_object = create_json_object();

    char *stringified_command_type = resync_server_command_type_to_string(command->command_type);
    if (stringified_command_type == NULL) {
        SET_ERROR_MSG(error_msg, "Command specifies an unsupported operation type!");
        goto error_out;
    }

    cJSON_AddItemToObject(command_json_object, CMD_KEY_COMMAND_TYPE, create_json_string(stringified_command_type));

    // Based on the command type, create an appropriate JSON object representing the associated required data.
    cJSON *command_metadata_json;
    switch (command->command_type) {
        case ADD_WORKSPACE:
        case ADD_REMOTE_SYSTEM:
            command_metadata_json = workspaceInformation_to_cjson(command->command_metadata.workspace_information, error_msg);
            break;
        case REMOVE_WORKSPACE:
            command_metadata_json = create_json_string(command->command_metadata.local_workspace_root_path);
            break;
        case REMOVE_REMOTE_SYSTEM:
            command_metadata_json = removeRemoteSystemMetadata_to_cjson(command->command_metadata.rm_remote_system_md, error_msg);
            break;
        case OTHER_RESYNC_SERVER_COMMAND_TYPE:
        default:
            SET_ERROR_MSG(error_msg, "Command specifies an unsupported operation type!");
            goto error_out;
    }

    if (command_metadata_json == NULL) {
        goto error_out;
    }

    cJSON_AddItemToObject(command_json_object, CMD_KEY_COMMAND_METADATA, command_metadata_json);

    return command_json_object;

error_out:
    cJSON_Delete(command_json_object);
    return NULL;
}

char *
resyncServerCommand_to_stringified_json(ResyncServerCommand *command, char **error_msg)
{
    cJSON *json_command = resyncServerCommand_to_cjson(command, error_msg);
    if (json_command == NULL) {
        return NULL;
    }

    char *stringified_json_command = cJSON_Print(json_command);
    if (stringified_json_command == NULL) {
        SET_ERROR_MSG(error_msg, "Unable to stringify the JSON representation of the command");
        cJSON_Delete(json_command);
        return NULL;
    }

    cJSON_Delete(json_command);
    return stringified_json_command;
}
