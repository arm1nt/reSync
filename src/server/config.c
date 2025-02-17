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

ConfigFileEntryInformation *
parse_config_file(void)
{
    ConfigFileEntryInformation *config_entry_list_head = NULL;

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
        WorkspaceInformation *ws_information = json_to_workspace_information(config_entry);
        char *ws_info_json_string = workspace_information_to_stringified_json(ws_information);

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
