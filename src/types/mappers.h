#ifndef RESYNC_MAPPERS_H
#define RESYNC_MAPPERS_H

#include "types.h"
#include "../../lib/json/cJSON.h"

/*
 * ResyncServerCommand <-> (stringified) JSON mappers
 */
ResyncServerCommand *stringified_json_to_resyncServerCommand(const char *stringified_json_command, char **error_msg);

ResyncServerCommand *cjson_to_resyncServerCommand(const cJSON *json_command, char **error_msg);

cJSON *resyncServerCommand_to_cjson(ResyncServerCommand *command, char **error_msg);

char *resyncServerCommand_to_stringified_json(ResyncServerCommand *command, char **error_msg);

/*
 * WorkspaceInformation <-> (stringified) JSON mappers
 */
WorkspaceInformation *stringified_json_to_workspaceInformation(const char *stringified_json_ws_info, char **error_msg);

WorkspaceInformation *cjson_to_workspaceInformation(const cJSON *json_ws_info, char **error_msg);

cJSON *workspaceInformation_to_cjson(WorkspaceInformation *ws_info, char **error_msg);

char *workspaceInformation_to_stringified_json(WorkspaceInformation *ws_info, char **error_msg);

/*
 * RemoteWorkspaceMetadata <-> JSON mappers
 */
RemoteWorkspaceMetadata *cjson_to_remoteWorkspaceMetadata(const cJSON *json_remote_ws_metadata, char **error_msg);

cJSON *remoteWorkspaceMetadata_to_cjson(RemoteWorkspaceMetadata *remote_ws_metadata, char **error_msg);

/*
 * Enum <-> string representation mappers
 */
ConnectionType string_to_connection_type(const char *stringified_connection_type);

char *connection_type_to_string(ConnectionType connection_type);

ResyncServerCommandType string_to_resync_server_command_type(const char *stringified_resync_server_command_type);

char *resync_server_command_type_to_string(ResyncServerCommandType command_type);

#endif //RESYNC_MAPPERS_H
