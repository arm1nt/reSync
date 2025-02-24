#include "config.h"

#define READ_CHUNK_SIZE ((ssize_t)(2048 * sizeof(char)))

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

static bool
config_file_is_empty_array(const char *buffer)
{
    if (buffer == NULL) {
        return true;
    }

    int opening_array_bracket_counter = 0;
    int closing_array_bracket_counter = 0;
    int non_white_space_characters_counter = 0;

    for (ssize_t i = 0; buffer[i] != '\0'; i++) {
        const char c = buffer[i];

        if (c == '[') {
            opening_array_bracket_counter++;
        } else if (c == ']') {
            closing_array_bracket_counter++;
        } else if (isspace(c) == 0) {
            non_white_space_characters_counter++;
        }
    }

    return opening_array_bracket_counter == 1 && closing_array_bracket_counter == 1 && non_white_space_characters_counter == 0;
}

static bool
write_config_file_from_buffer(char *buffer, char **error_msg)
{
    CLEAR_ERROR_MSG(error_msg);

    int config_file_fd = open(CONFIG_FILE_PATH, O_WRONLY);
    if (config_file_fd == -1) {
        SET_ERROR_MSG_RAW(
                error_msg,
                format_string(
                        "Unable to open reSync configuration file, expected file to be located at '%s': %s",
                        CONFIG_FILE_PATH,
                        strerror(errno)
                )
        );
        return false;
    }

    if (write(config_file_fd, buffer, strlen(buffer)) == -1) {
        SET_ERROR_MSG_RAW(error_msg, format_string("Unable to update reSync configuration file: %s", strerror(errno)));
        return false;
    }

    close(config_file_fd);

    return true;
}

static bool
read_config_file_into_buffer(char **buf, char **error_msg) //TODO: pre-condition: buf != NULL
{
    CLEAR_ERROR_MSG(error_msg);

    int config_file_fd = open(CONFIG_FILE_PATH, O_RDONLY);
    if (config_file_fd == -1) {
        SET_ERROR_MSG_RAW(
                error_msg,
                format_string(
                        "Unable to open reSync configuration file, expected file to be located at '%s': %s",
                        CONFIG_FILE_PATH,
                        strerror(errno)
                )
        );
        return false;
    }

    ssize_t bytes_read = 0;
    ssize_t read_chunks = 0;

    char *config_file_buffer = NULL;
    char buffer[READ_CHUNK_SIZE];

    while ((bytes_read = read(config_file_fd, buffer, READ_CHUNK_SIZE -1)) > 0) {
        read_chunks++;

        config_file_buffer = do_realloc(config_file_buffer, read_chunks * READ_CHUNK_SIZE);
        const ssize_t  offset = READ_CHUNK_SIZE * (read_chunks -1);
        memset(config_file_buffer + offset, '\0', READ_CHUNK_SIZE);
        strncat(config_file_buffer, buffer, bytes_read);
    }

    if (bytes_read < 0) {
        DO_FREE(config_file_buffer);
        close(config_file_fd);

        SET_ERROR_MSG_RAW(
                error_msg,
                format_string("An error occurred while reading the reSync config file: %s", strerror(errno))
        );

        *buf = NULL;
        return false;
    }

    close(config_file_fd);
    *buf = config_file_buffer;
    return true;
}

static int
config_file_contains_workspace(cJSON *config_entries_array, WorkspaceInformation *ws_info, char **error_msg)
{
    CLEAR_ERROR_MSG(error_msg);

    if (config_entries_array == NULL || ws_info == NULL) {
        SET_ERROR_MSG(error_msg, "Config entries array and workspace info must be specified!");
        return -1;
    }

    cJSON *config_array_entry;
    cJSON_ArrayForEach(config_array_entry, config_entries_array) {
        WorkspaceInformation *ws_info_entry = json_to_workspace_information(config_array_entry, error_msg);
        if (ws_info_entry == NULL) {
            char *underlying_error_msg = (error_msg != NULL) ? *error_msg : NULL;

            if (underlying_error_msg != NULL) {
                SET_ERROR_MSG_RAW(
                        error_msg,
                        format_string(
                                "Configuration file entry does not contain all required members (%s): \n%s",
                                underlying_error_msg,
                                JSON_PRINT(config_array_entry)
                        )
                );
            } else {
                SET_ERROR_MSG(error_msg, "An error occurred while parsing the configuration file!");
            }

            return -1;
        }

        // For now: don't allow subdirectories of a workspace to be registered as their own workspace
        if (strncmp(ws_info->local_workspace_root_path, ws_info_entry->local_workspace_root_path, strlen(ws_info->local_workspace_root_path)) == 0
        || strncmp(ws_info_entry->local_workspace_root_path, ws_info->local_workspace_root_path, strlen(ws_info_entry->local_workspace_root_path)) == 0) {
            return 1;
        }
    }

    return 0;
}

bool
add_config_file_entry(WorkspaceInformation *ws_info, char **error_msg)
{
    CLEAR_ERROR_MSG(error_msg);

    char *config_file_buffer;
    if (read_config_file_into_buffer(&config_file_buffer, error_msg) == false) {
        SET_ERROR_MSG_IF_EMPTY(error_msg, "Unable to add config file entry as reading the configuration file failed");
        return false;
    }

    cJSON *config_file_object_array;
    if (config_file_buffer == NULL || is_blank(config_file_buffer) || config_file_is_empty_array(config_file_buffer)) {
        config_file_object_array = cJSON_CreateArray();

        if (config_file_object_array == NULL) {
            SET_ERROR_MSG(error_msg, "Unable to create a JSON array to populate the empty config file!");
            goto error_out_1;
        }
    } else {
        config_file_object_array = cJSON_Parse(config_file_buffer);

        if (config_file_object_array == NULL) {
            char *base_error_msg = "Unable to parse config file, perhaps the JSON file is not properly formatted!";

            if (cJSON_GetErrorPtr() != NULL) {
                SET_ERROR_MSG_RAW(error_msg, format_string("%s\n. Error in config file before: %s", base_error_msg, cJSON_GetErrorPtr()));
            } else {
                SET_ERROR_MSG(error_msg, base_error_msg);
            }

            goto error_out_1;
        }
    }

    int res = config_file_contains_workspace(config_file_object_array, ws_info, error_msg);
    if (res == -1) {
        SET_ERROR_MSG_IF_EMPTY(error_msg, "Unable to add config file entry as checking if the workspace is already managed failed");
        goto error_out_2;
    } else if (res == 1) {
        SET_ERROR_MSG(
                error_msg,
                "The workspace or one of its subdirectories is already managed by reSync."
                "If you want to add a new remote system for the workspace, please use the appropriate command!"
        );
        goto error_out_2;
    }


    cJSON *json_ws_info = workspace_information_to_json(ws_info, error_msg);
    if (json_ws_info == NULL) {
        SET_ERROR_MSG_IF_EMPTY(
                error_msg,
                "Unable to add config file entry as the workspace information received could not be mapped to JSON"
        );
        goto error_out_2;
    }

    cJSON_AddItemToArray(config_file_object_array, json_ws_info);

    if (write_config_file_from_buffer(cJSON_Print(config_file_object_array), error_msg) == false) {
        SET_ERROR_MSG_IF_EMPTY(error_msg, "Unable to add config file entry as persisting the updated config file failed!");
        goto error_out_2;
    }

    cJSON_Delete(config_file_object_array);
    DO_FREE(config_file_buffer);
    return true;

error_out_2:
    cJSON_Delete(config_file_object_array);
error_out_1:
    DO_FREE(config_file_buffer);
    return false;
}


/*ConfigFileEntryInformation *
parse_config_file(void)
{
    ConfigFileEntryInformation *config_entry_list_head = NULL;

    char *config_file_buffer = read_config_file_into_buffer(NULL);
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
        char *error_msg;
        WorkspaceInformation *ws_information = json_to_workspace_information(config_entry, &error_msg);
        if (ws_information == NULL) {
            fatal_custom_error("Unable to parse ws_info json");
        }

        char *ws_info_json_string = workspace_information_to_stringified_json(ws_information, &error_msg);

        ConfigFileEntryInformation *entry = (ConfigFileEntryInformation *) do_malloc(sizeof(ConfigFileEntryInformation));
        entry->workspace_information = ws_information;
        entry->ws_information_json_string = ws_info_json_string;

        LL_APPEND(config_entry_list_head, entry);
    }

out:
    DO_FREE(config_file_buffer);
    cJSON_Delete(config_object);

    return config_entry_list_head;
}*/
