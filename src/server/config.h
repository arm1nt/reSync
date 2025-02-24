#ifndef RESYNC_CONFIG_H
#define RESYNC_CONFIG_H

#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>


#include "../../lib/json/cJSON.h"
#include "../../lib/ulist.h"
#include "../util/error.h"
#include "../util/debug.h"
#include "../types.h"

#define CONFIG_FILE_PATH "./resync.json"

typedef struct ConfigFileEntryInformation {
    WorkspaceInformation *workspace_information;
    char *ws_information_json_string;
    struct ConfigFileEntryInformation *next;
} ConfigFileEntryInformation;

/*
 * Parses the reSync configuration file and returns a structured view of all contained information
 */
ConfigFileEntryInformation *parse_config_file(void); //TODO: update to return NULL + error_msg in error case

bool add_config_file_entry(WorkspaceInformation *ws_info, char **error_msg);

bool remove_config_file_entry(WorkspaceInformation *ws_info, char **error_msg);

bool add_remote_system_to_config_file_entry(WorkspaceInformation *ws_info, char **error_msg);

bool remove_remote_system_from_config_file_entry(WorkspaceInformation *ws_info, char **error_msg);

#endif //RESYNC_CONFIG_H
