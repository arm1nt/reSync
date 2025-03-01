#ifndef RESYNC_CONFIG_H
#define RESYNC_CONFIG_H

#include "../types/types.h"
#include "../types/mappers.h"
#include "../../lib/ulist.h"
#include "../../lib/json/cJSON.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>


#define DEFAULT_RESYNC_CONFIG_FILE_PATH "./resync.json"

typedef struct ConfigFileEntryData {
    const WorkspaceInformation *workspace_information;
    const char *stringified_json_workspace_information;
    struct ConfigFileEntryData *next;
} ConfigFileEntryData;

/*
 * Functions to read, write and manipulate the reSync configuration file.
 */
bool parse_configuration_file(ConfigFileEntryData **config_file_entries, char **error_msg);

bool add_workspace_to_configuration_file(const WorkspaceInformation *ws_info, ConfigFileEntryData **config_entry_data, char **error_msg);

bool remove_workspace_from_configuration_file(const char *workspace_root_path, char **error_msg);

#endif //RESYNC_CONFIG_H
