#include "config.h"

#include "../util/debug.h"

#define CONFIG_READ_CHUNK_SIZE ((ssize_t)(2048 * sizeof(char)))

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

    *config_file_entries = config_file_entry_list_head;
    return true;

error_out:

    if (json_config_file_entry_array != NULL) {
        cJSON_Delete(json_config_file_entry_array);
    }

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
