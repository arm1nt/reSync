#include "config.h"

#include "../util/debug.h"

#define CONFIG_READ_CHUNK_SIZE ((ssize_t)(2048 * sizeof(char)))

static bool
write_to_configuration_file_from_buffer(const char *config_file_buffer, char **error_msg)
{
    if (config_file_buffer == NULL) {
        SET_ERROR_MSG(error_msg, "Unable to write to reSync configuration file as no data to write was specified!");
        return false;
    }

    int config_file_fd = open(DEFAULT_RESYNC_CONFIG_FILE_PATH, O_WRONLY | O_TRUNC);
    if (config_file_fd == -1) {
        SET_ERROR_MSG_RAW(
                error_msg,
                format_string("Unable to open reSync configuration file for writing: %s", strerror(errno))
        );
        return false;
    }

    if (write(config_file_fd, config_file_buffer, strlen(config_file_buffer)) == -1) {
        close(config_file_fd);
        SET_ERROR_MSG_RAW(
                error_msg,
                format_string("Unable to write to reSync configuration file: %s", strerror(errno))
        );
        return false;
    }

    close(config_file_fd);
    return true;
}

static bool
is_config_file_empty(const char *config_file_buffer)
{
    if (config_file_buffer == NULL) {
        return true;
    }

    int array_start_brackets_counter = 0;
    int array_end_brackets_counter = 0;
    int non_whitespace_chars_counter = 0;

    for (ssize_t i = 0; config_file_buffer[i] != '\0'; i++) {
        const char c = config_file_buffer[i];

        switch (c) {
            case '[':
                if (non_whitespace_chars_counter != 0 || array_end_brackets_counter != 0) {
                    return false;
                }
                array_start_brackets_counter++;
                break;
            case ']':
                if (array_start_brackets_counter != 1 && non_whitespace_chars_counter != 0) {
                    return false;
                }
                array_end_brackets_counter++;
                break;
            default:
                non_whitespace_chars_counter = (isspace(c) == 0)
                        ? non_whitespace_chars_counter+1
                        : non_whitespace_chars_counter;
        }
    }

    return array_start_brackets_counter == 1 && array_end_brackets_counter == 1 && non_whitespace_chars_counter == 0;
}

static bool
read_configuration_file_into_buffer(char **buf, char **error_msg)
{
    int config_file_fd = open(DEFAULT_RESYNC_CONFIG_FILE_PATH, O_RDONLY);
    if (config_file_fd == -1) {
        SET_ERROR_MSG_RAW(
                error_msg,
                format_string("Unable to open reSync configuration file: %s", strerror(errno))
        );
        return false;
    }

    ssize_t bytes_read;
    ssize_t read_chunks = 0;
    char *config_file_buffer = NULL;
    char buffer[CONFIG_READ_CHUNK_SIZE];

    while ((bytes_read = read(config_file_fd, buffer, CONFIG_READ_CHUNK_SIZE - 1)) > 0) {
        read_chunks++;

        config_file_buffer = (char *) do_realloc(config_file_buffer, read_chunks * CONFIG_READ_CHUNK_SIZE);
        const ssize_t offset = CONFIG_READ_CHUNK_SIZE * (read_chunks - 1);
        memset(config_file_buffer + offset, '\0', CONFIG_READ_CHUNK_SIZE);
        strncat(config_file_buffer, buffer, bytes_read);
    }

    if (bytes_read < 0) {
        *buf = NULL;
        DO_FREE(config_file_buffer);
        close(config_file_fd);

        SET_ERROR_MSG_RAW(
                error_msg,
                format_string("An error occurred while reading the reSync configuration file: %s", strerror(errno))
        );
        return false;
    }

    close(config_file_fd);
    *buf = config_file_buffer;
    return true;
}

static cJSON *
get_json_array_from_config_file_buffer(const char *config_file_buffer, char **error_msg)
{
    cJSON *json_config_file_array = NULL;

    if (config_file_buffer == NULL || is_blank(config_file_buffer) || is_config_file_empty(config_file_buffer)) {
        json_config_file_array = cJSON_CreateArray();

        if (json_config_file_array == NULL) {
            SET_ERROR_MSG(error_msg, "Unable to create an empty JSON array while parsing the empty configuration file");
            return NULL;
        }
    } else {
        json_config_file_array = cJSON_Parse(config_file_buffer);

        if (json_config_file_array == NULL) {
            char *base_error_msg = "Unable to parse configuration file, perhaps the JSON file is not properly formatted!";

            if (cJSON_GetErrorPtr() != NULL) {
                SET_ERROR_MSG_RAW(error_msg, format_string("%s.\nError in config file before: \n'%s'", base_error_msg, cJSON_GetErrorPtr()));
            } else {
                SET_ERROR_MSG(error_msg, base_error_msg);
            }

            return NULL;
        }

        if (!cJSON_IsArray(json_config_file_array)) {
            SET_ERROR_MSG(error_msg, "Configuration file is not formatted as array of objects!");
            return NULL;
        }
    }

    return json_config_file_array;
}

/*
 * 1: Config file contains the workspace
 * 0: The config file does not contain an entry for the workspace
 * -1: An error occurred while searching the config file
 */
static int
config_file_contains_workspace(cJSON *config_entries_array, const WorkspaceInformation *ws_info, char **error_msg)
{
    if (config_entries_array == NULL || ws_info == NULL) {
        SET_ERROR_MSG(
                error_msg,
                "Config entries array and WorkspaceInformation struct must be specified when checking if a workspace is "
                "already managed by reSync"
        );
        return -1;
    }

    cJSON *config_array_entry;
    cJSON_ArrayForEach(config_array_entry, config_entries_array) {
        WorkspaceInformation *ws_info_entry = cjson_to_workspaceInformation(config_array_entry, error_msg);
        if (ws_info_entry == NULL) {
            SET_ERROR_MSG_WITH_CAUSE(error_msg, "An error occurred while checking if the workspace is already managed by reSync", error_msg);
            return -1;
        }

        if (strncmp(ws_info->local_workspace_root_path, ws_info_entry->local_workspace_root_path, strlen(ws_info->local_workspace_root_path)) == 0
            || strncmp(ws_info_entry->local_workspace_root_path, ws_info->local_workspace_root_path, strlen(ws_info_entry->local_workspace_root_path)) == 0) {
            // If a subdirectory or a parent directory is already managed by reSync, the requested workspace cannot also
            //  be managed by it.
            return 1;
        }
    }

    return 0;
}

/*
 * >= 0: The index of the matching workspace in the array
 * -1: no exact match was found
 * -2: An error occurred while searching the array for an exact match
 */
static int
get_index_of_exact_match_workspace(cJSON *config_entries_array, const char *path, char **error_msg)
{
    if (config_entries_array == NULL || path == NULL) {
        SET_ERROR_MSG(
                error_msg,
                "The config entry array and the workspace path must be specified when removing a workspace from "
                "being managed by reSync!"
        );
        return -2;
    }

    int loop_index = 0;
    cJSON *config_array_entry;
    cJSON_ArrayForEach(config_array_entry, config_entries_array) {

        WorkspaceInformation *ws_info_entry = cjson_to_workspaceInformation(config_array_entry, error_msg);
        if (ws_info_entry == NULL) {
            SET_ERROR_MSG_WITH_CAUSE(error_msg, "An error occurred while searching the workspaces contained in the configuration file", error_msg);
            return -2;
        }

        if (is_equal(ws_info_entry->local_workspace_root_path, path)) {
            return loop_index;
        }

        loop_index++;
    }

    return -1;
}

static bool
matches_ssh_remote_system(const SshConnectionInformation *connection_info, const RemoveRemoteSystemMetadata *rm_rsys_data)
{
    if (rm_rsys_data->connection_type != SSH) {
        return false;
    }

    if (!is_equal(connection_info->hostname,rm_rsys_data->remote_system_id_information.hostname)) {
        return false;
    }

    return true;
}

static bool
matches_ssh_host_alias_remote_system(const char *ssh_host_alias, const RemoveRemoteSystemMetadata *rm_rsys_data)
{
    if (rm_rsys_data->connection_type != SSH_HOST_ALIAS) {
        return false;
    }

    if (!is_equal(ssh_host_alias, rm_rsys_data->remote_system_id_information.ssh_host_alias)) {
        return false;
    }

    return true;
}

static bool
matches_rsync_remote_system(const RsyncConnectionInformation *connection_info, const RemoveRemoteSystemMetadata *rm_rsys_data)
{
    if (rm_rsys_data->connection_type != RSYNC_DAEMON) {
        return false;
    }

    if (!is_equal(connection_info->hostname, rm_rsys_data->remote_system_id_information.hostname)) {
        return false;
    }

    return true;
}

/*
 * >= 0: index of the remote system in the array
 * -1: remote system was not found
 * -2: An error occurred while searching the array for the remote system
 */
static int
get_index_of_remote_system_based_on_identifying_information(const cJSON *remote_systems_array, const RemoveRemoteSystemMetadata *rm_rsys_data, char **error_msg)
{
    if (remote_systems_array == NULL || rm_rsys_data == NULL) {
        SET_ERROR_MSG(
                error_msg,
                "The workspaces remote systems array and the data identifying the remote system to be removed must be specified"
        );
        return -2;
    }

    int loop_index = 0;
    cJSON *array_entry;
    cJSON_ArrayForEach(array_entry, remote_systems_array) {

        RemoteWorkspaceMetadata *remote_system = cjson_to_remoteWorkspaceMetadata(array_entry, error_msg);
        if (remote_system == NULL) {
            SET_ERROR_MSG_WITH_CAUSE_RAW(
                    error_msg,
                    format_string("An error occurred while searching the array of remote systems of workspace '%s'", rm_rsys_data->local_workspace_root_path),
                    error_msg
            );
            return -2;
        }

        if (!is_equal(remote_system->remote_workspace_root_path, rm_rsys_data->remote_workspace_root_path)) {
            loop_index++;
            continue;
        }

        bool res_connection_info_matches = false;
        switch (remote_system->connection_type) {
            case SSH:
                res_connection_info_matches = matches_ssh_remote_system(remote_system->connection_information.ssh_connection_information, rm_rsys_data);
                break;
            case SSH_HOST_ALIAS:
                res_connection_info_matches = matches_ssh_host_alias_remote_system(remote_system->connection_information.ssh_host_alias, rm_rsys_data);
                break;
            case RSYNC_DAEMON:
                res_connection_info_matches = matches_rsync_remote_system(remote_system->connection_information.rsync_connection_information, rm_rsys_data);
                break;
            case OTHER_CONNECTION_TYPE:
            default:
                SET_ERROR_MSG_RAW(
                        error_msg,
                        format_string(
                                "Encountered unsupported connection type while searching the remote systems of "
                                "workspace '%s'",
                                rm_rsys_data->local_workspace_root_path
                        )
                );
                return -2;
        }

        if (res_connection_info_matches == true) {
            return loop_index;
        }

        loop_index++;
    }

    return -1;
}

bool
add_workspace_to_configuration_file(const WorkspaceInformation *ws_info, ConfigFileEntryData **config_entry_data, char **error_msg)
{
    if (config_entry_data == NULL) {
        SET_ERROR_MSG(error_msg, "config_entry_data is NULL");
        return false;
    }

    *config_entry_data = NULL;

    if (ws_info == NULL) {
        SET_ERROR_MSG(error_msg, "Unable to add workspace to configuration file as specified WorkspaceInformation struct is NULL");
        return false;
    }

    char *config_file_buffer = NULL;
    if (read_configuration_file_into_buffer(&config_file_buffer, error_msg) == false) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to add workspace to configuration file", error_msg);
        return false;
    }

    cJSON *json_config_file_entry_array = get_json_array_from_config_file_buffer(config_file_buffer, error_msg);
    if (json_config_file_entry_array == NULL) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to add workspace to configuration file", error_msg);
        goto error_out;
    }

    int res = config_file_contains_workspace(json_config_file_entry_array, ws_info, error_msg);
    if (res == -1) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to add workspace to configuration file", error_msg);
        goto error_out;
    } else if (res == 1) {
        SET_ERROR_MSG(
                error_msg,
                "The workspace, a parent directory of this workspace or a child directory is already managed by reSync. "
                "If you want to add a new remote system for the workspace, please use the appropriate command."
        );
        goto error_out;
    }


    cJSON *json_ws_info = workspaceInformation_to_cjson(ws_info, error_msg);
    if (json_ws_info == NULL) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to add workspace to configuration file", error_msg);
        goto error_out;
    }

    cJSON_AddItemToArray(json_config_file_entry_array, json_ws_info);

    if (write_to_configuration_file_from_buffer(cJSON_Print(json_config_file_entry_array), error_msg) == false) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to add workspace to configuration file", error_msg);
        goto error_out_ext;
    }

    ConfigFileEntryData *config_entry = (ConfigFileEntryData *) do_calloc(1, sizeof(ConfigFileEntryData));
    config_entry->workspace_information = ws_info;
    config_entry->stringified_json_workspace_information = cJSON_Print(json_ws_info);

    DO_FREE(config_file_buffer);
    cJSON_Delete(json_config_file_entry_array);
    *config_entry_data = config_entry;
    return true;

error_out_ext:

    cJSON_Delete(json_ws_info);

error_out:

    cJSON_Delete(json_config_file_entry_array);

    DO_FREE(config_file_buffer);
    return false;
}

bool
remove_workspace_from_configuration_file(const char *workspace_root_path, char **error_msg)
{
    if (workspace_root_path == NULL) {
        SET_ERROR_MSG(error_msg, "The path of the workspace that should no longer be managed by reSync is missing!");
        return false;
    }

    char *config_file_buffer = NULL;
    if (read_configuration_file_into_buffer(&config_file_buffer, error_msg) == false) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to remove the workspace from the configuration file", error_msg);
        return false;
    }

    cJSON *json_config_file_entry_array = get_json_array_from_config_file_buffer(config_file_buffer, error_msg);
    if (json_config_file_entry_array == NULL) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to remove the workspace from the configuration file", error_msg);
        goto error_out;
    }

    int index = get_index_of_exact_match_workspace(json_config_file_entry_array, workspace_root_path, error_msg);
    if (index == -2) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to remove the workspace from the configuration file", error_msg);
        goto error_out;
    } else if (index == -1) {
        SET_ERROR_MSG_RAW(
                error_msg,
                format_string("The specified workspace ('%s') is not being managed by reSync", workspace_root_path)
        );
        goto error_out;
    }

    cJSON_DeleteItemFromArray(json_config_file_entry_array, index);

    if (write_to_configuration_file_from_buffer(cJSON_Print(json_config_file_entry_array), error_msg) == false) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to remove the workspace from the configuration file", error_msg);
        goto error_out;
    }

    cJSON_Delete(json_config_file_entry_array);
    DO_FREE(config_file_buffer);
    return true;

error_out:
    cJSON_Delete(json_config_file_entry_array);
    DO_FREE(config_file_buffer);
    return false;
}

bool
add_remote_system_to_workspace_config_entry(const WorkspaceInformation *ws_info, ConfigFileEntryData **config_entry_data, char **error_msg)
{
    if (config_entry_data == NULL) {
        SET_ERROR_MSG(error_msg, "config_entry_data is NULL");
        return false;
    }

    *config_entry_data = NULL;

    if (ws_info == NULL) {
        SET_ERROR_MSG(
                error_msg,
                "Unable to add a remote system definition to the workspace as the WorkspaceInformation struct, holding "
                "the data required to perform this operation, is NULL!"
        );
        return false;
    }

    char *config_file_buffer = NULL;
    if (read_configuration_file_into_buffer(&config_file_buffer, error_msg) == false) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to add remote system to workspaces config file entry", error_msg);
        return false;
    }

    cJSON *json_config_file_entry_array = get_json_array_from_config_file_buffer(config_file_buffer, error_msg);
    if (json_config_file_entry_array == NULL) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to add remote system to workspaces config file entry", error_msg);
        goto error_out;
    }

    int entry_index = get_index_of_exact_match_workspace(json_config_file_entry_array, ws_info->local_workspace_root_path, error_msg);
    if (entry_index == -2) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to add remote system to workspaces configuration file entry", error_msg);
        goto error_out;
    } else if (entry_index == -1) {
        SET_ERROR_MSG_RAW(
                error_msg,
                format_string("The specified workspace ('%s') is not being managed by reSync", ws_info->local_workspace_root_path)
        );
        goto error_out;
    }

    cJSON *json_ws_info_entry = cJSON_GetArrayItem(json_config_file_entry_array, entry_index);

    cJSON *remote_systems_array = cJSON_GetObjectItemCaseSensitive(json_ws_info_entry, "remote-systems");
    if (remote_systems_array == NULL) {
        SET_ERROR_MSG(
                error_msg,
                "Unable to add remote system to workspaces config file entry, as the config file entry does not contain a 'remote-systems' key"
        );
        goto error_out;
    } else if (!cJSON_IsArray(remote_systems_array)) {
        SET_ERROR_MSG(
                error_msg,
                "The current value of the workspaces 'remote-systems' member is not an array, but it must be an "
                "(possibly empty) array of remote system objects!"
        );
        goto error_out;
    }

    cJSON *remote_system_to_add = remoteWorkspaceMetadata_to_cjson(ws_info->remote_systems, error_msg);
    if (remote_system_to_add == NULL) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to add remote system to workspaces config file entry", error_msg);
        goto error_out;
    }

    cJSON_AddItemToArray(remote_systems_array, remote_system_to_add);

    if (write_to_configuration_file_from_buffer(cJSON_Print(json_config_file_entry_array), error_msg) == false) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to add remote system to workspaces config file entry", error_msg);
        goto error_out;
    }

    ConfigFileEntryData *updated_config_entry = (ConfigFileEntryData *) do_calloc(1, sizeof(ConfigFileEntryData));
    updated_config_entry->workspace_information = ws_info;
    updated_config_entry->stringified_json_workspace_information = cJSON_Print(json_ws_info_entry);
    *config_entry_data = updated_config_entry;

    DO_FREE(config_file_buffer);
    cJSON_Delete(json_config_file_entry_array);
    return true;

error_out:
    cJSON_Delete(json_config_file_entry_array);
    DO_FREE(config_file_buffer);
    return false;
}

bool
remove_remote_system_from_workspace_config_entry(const RemoveRemoteSystemMetadata *rm_rsys, ConfigFileEntryData **config_entry_data, char **error_msg)
{
    if (config_entry_data == NULL) {
        SET_ERROR_MSG(error_msg, "config_entry_data is NULL");
        return false;
    }

    *config_entry_data = NULL;

    if (rm_rsys == NULL) {
        SET_ERROR_MSG(
                error_msg,
                "Unable to remove remote system definition from workspaces config file entry as the RemoveRemoteSystemMetadata "
                "struct, holding the information required to remove the remote system, is NULL!"
        );
        return false;
    }

    char *config_file_buffer = NULL;
    if (read_configuration_file_into_buffer(&config_file_buffer, error_msg) == false) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to remove remote system from workspaces config file entry", error_msg);
        return false;
    }

    cJSON *json_config_file_entry_array = get_json_array_from_config_file_buffer(config_file_buffer, error_msg);
    if (json_config_file_entry_array == NULL) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to remove remote system from workspaces config file entry", error_msg);
        goto error_out;
    }

    int entry_index = get_index_of_exact_match_workspace(json_config_file_entry_array, rm_rsys->local_workspace_root_path, error_msg);
    if (entry_index == -2) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to remove remote system from workspaces config file entry", error_msg);
        goto error_out;
    } else if (entry_index == -1) {
        SET_ERROR_MSG_RAW(
                error_msg,
                format_string("The specified workspace ('%s') is not being managed by reSync!", rm_rsys->local_workspace_root_path)
        );
        goto error_out;
    }

    cJSON *json_ws_info_entry = cJSON_GetArrayItem(json_config_file_entry_array, entry_index);

    cJSON *remote_systems_array = cJSON_GetObjectItemCaseSensitive(json_ws_info_entry, "remote-systems");
    if (remote_systems_array == NULL) {
        SET_ERROR_MSG(
                error_msg,
                "Unable to remove the remote system from the workspaces config file entry, as the config file entry does not contain a 'remote-systems' key!"
        );
        goto error_out;
    } else if (!cJSON_IsArray(remote_systems_array)) {
        SET_ERROR_MSG(
                error_msg,
                "The current value of the workspaces 'remote-systems' member is not an array, but it must be an "
                "(possibly empty) array of remote system objects!"
        );
        goto error_out;
    }

    int remote_system_index = get_index_of_remote_system_based_on_identifying_information(remote_systems_array, rm_rsys, error_msg);
    if (remote_system_index == -2) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to remove remote system from workspaces config file entry", error_msg);
        goto error_out;
    } else if (remote_system_index == -1) {
        SET_ERROR_MSG(error_msg, "The workspace does not synchronize with the requested remote system");
        goto error_out;
    }

    cJSON_DeleteItemFromArray(remote_systems_array, remote_system_index);

    if (write_to_configuration_file_from_buffer(cJSON_Print(json_config_file_entry_array), error_msg) == false) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to remove remote system from workspaces config file entry", error_msg);
        goto error_out;
    }

    ConfigFileEntryData *updated_ws_entry = (ConfigFileEntryData *) do_calloc(1, sizeof(ConfigFileEntryData));
    updated_ws_entry->workspace_information = cjson_to_workspaceInformation(json_ws_info_entry, error_msg);
    updated_ws_entry->stringified_json_workspace_information = cJSON_Print(json_ws_info_entry);
    *config_entry_data = updated_ws_entry;

    cJSON_Delete(json_config_file_entry_array);
    DO_FREE(config_file_buffer);
    return true;

error_out:
    cJSON_Delete(json_config_file_entry_array);
    DO_FREE(config_file_buffer);
    return false;
}

bool
parse_configuration_file(ConfigFileEntryData **config_file_entries, char **error_msg)
{
    *config_file_entries = NULL;
    ConfigFileEntryData *config_file_entry_list_head = NULL;

    char *config_file_buffer = NULL;
    if (read_configuration_file_into_buffer(&config_file_buffer, error_msg) == false) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to parse reSync configuration file", error_msg);
        return false;
    }

    cJSON *json_config_file_entry_array = get_json_array_from_config_file_buffer(config_file_buffer, error_msg);
    if (json_config_file_entry_array == NULL) {
        SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to parse reSync configuration file", error_msg);
        goto error_out;
    }

    cJSON *config_array_entry;
    cJSON_ArrayForEach(config_array_entry, json_config_file_entry_array) {
        WorkspaceInformation *ws_info = cjson_to_workspaceInformation(config_array_entry, error_msg);
        if (ws_info == NULL) {
            SET_ERROR_MSG_WITH_CAUSE(error_msg, "Unable to parse reSync configuration file", error_msg);
            goto error_out;
        }

        ConfigFileEntryData *entry = (ConfigFileEntryData *) do_calloc(1, sizeof(ConfigFileEntryData));
        entry->workspace_information = ws_info;
        entry->stringified_json_workspace_information = cJSON_Print(config_array_entry);

        LL_APPEND(config_file_entry_list_head, entry);
    }

    cJSON_Delete(json_config_file_entry_array);
    DO_FREE(config_file_buffer);
    *config_file_entries = config_file_entry_list_head;
    return true;

error_out:

    cJSON_Delete(json_config_file_entry_array);

    ConfigFileEntryData *entry, *temp;
    LL_FOREACH_SAFE(config_file_entry_list_head, entry, temp) {
        LL_DELETE(config_file_entry_list_head, entry);
        destroy_workspaceInformation((WorkspaceInformation **) &(entry->workspace_information));
        DO_FREE(entry->stringified_json_workspace_information);
        DO_FREE(entry);
    }

    DO_FREE(config_file_buffer);
    return false;
}
