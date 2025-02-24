#include "config.h"

#define READ_CHUNK_SIZE ((ssize_t)(2048 * sizeof(char)))

#define CLEAR_ERROR_MSG(x) (*(x) = NULL)

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
config_file_is_empty_array(const char *buffer) {
    int opening_array_bracket_counter = 0;
    int closing_array_bracket_counter = 0;
    int non_white_space_characters_counter = 0;

    for (ssize_t i = 0; buffer[i] != '\0'; i++) {
        const char c = buffer[i];

        if (c == '[') {
            opening_array_bracket_counter++;
        } else if (c == ']') {
            closing_array_bracket_counter++;
        } else if (isspace(c) != 0) {
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
        *error_msg = format_string(
                "Unable to open reSync config file, expected file to be located at '%s': %s",
                CONFIG_FILE_PATH,
                strerror(errno)
        );
        return false;
    }

    if (write(config_file_fd, buffer, strlen(buffer)) == -1) {
        *error_msg = format_string("Unable to update reSync config file: %s", strerror(errno));
        return false;
    }

    close(config_file_fd);

    return true;
}

static char *
read_config_file_into_buffer(char **error_msg)
{
    CLEAR_ERROR_MSG(error_msg);

    int config_file_fd = open(CONFIG_FILE_PATH, O_RDONLY);
    if (config_file_fd == -1) {
        *error_msg = format_string(
                "Unable to open reSync config file, expected file to be located at '%s': %s",
                CONFIG_FILE_PATH,
                strerror(errno)
        );
        return NULL;
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

    close(config_file_fd);
    return config_file_buffer;
}

bool
add_config_file_entry(WorkspaceInformation *ws_info, char **error_msg)
{
    CLEAR_ERROR_MSG(error_msg);

    char *config_file_buffer = read_config_file_into_buffer(error_msg);
    if (config_file_buffer == NULL && *error_msg != NULL) {
        return false;
    }


    cJSON *config_file_object_array;
    if (config_file_buffer == NULL || is_blank(config_file_buffer) || config_file_is_empty_array(config_file_buffer)) {
        config_file_object_array = cJSON_CreateArray();

        if (config_file_object_array == NULL) {
            *error_msg = resync_strdup("Unable to create a JSON array to populate the empty config file!");
            return false;
        }
    } else {
        config_file_object_array = cJSON_Parse(config_file_buffer);

        if (config_file_object_array == NULL) {
            char *base_error_msg = "Unable to parse config file, perhaps the JSON file is not properly formatted!";
            if (cJSON_GetErrorPtr() != NULL) {
                *error_msg = format_string("%s\n Error in config file before: %s", cJSON_GetErrorPtr());
            } else {
                *error_msg = resync_strdup(base_error_msg);
            }
            return false;
        }
    }

    cJSON *json_ws_info = workspace_information_to_json(ws_info, error_msg);
    if (json_ws_info == NULL) {
        if (*error_msg == NULL) {
            *error_msg = resync_strdup("Unable to add config file entry, as transformatoin from workspace info struct to JSON failed");
        }
        return false;
    }

    cJSON_AddItemToArray(config_file_object_array, json_ws_info);

    bool res = write_config_file_from_buffer(cJSON_Print(config_file_object_array), error_msg);
    if (res == false) {
        return res;
    }

    DO_FREE(config_file_buffer);
    return true;
}


ConfigFileEntryInformation *
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
}
