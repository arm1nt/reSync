#ifndef RESYNC_TYPES_H
#define RESYNC_TYPES_H

#include "../lib/json/cJSON.h"
#include "../lib/ulist.h"
#include "util/memory.h"
#include "util/error.h"
#include "util/debug.h"

/*
 * Convenience macros for parsing JSON objects
 */
#define STRING_VAL_EXISTS(x) ((x) != NULL && (x)->valuestring != NULL)

/*
 * Recognized JSON object members
 */
#define KEY_LOCAL_WORKSPACE_ROOT_PATH "local_workspace_root_path"
#define KEY_REMOTE_SYSTEMS "remote_systems"
#define KEY_REMOTE_WORKSPACE_ROOT_PATH "remote_workspace_root_path"
#define KEY_CONNECTION_TYPE "connection_type"
#define KEY_CONNECTION_INFORMATION "connection_information"
#define KEY_SSH_CONNECTION_INFO_USERNAME "username"
#define KEY_SSH_CONNECTION_INFO_HOSTNAME "hostname"
#define KEY_SSH_CONNECTION_INFO_IDENTITY_FILE "identity_file_path"
#define KEY_RSYNC_DAEMON_CONNECTION_INFO_USERNAME "username"
#define KEY_RSYNC_DAEMON_CONNECTION_INFO_HOSTNAME "hostname"
#define KEY_RSYNC_DAEMON_CONNECTION_INFO_PORT "port"
#define KEY_SSH_HOST_ALIAS_CONNECTION_INFO_NAME "ssh_host_alias"

#define KEY_DAEMON_CMD_TYPE "type"
#define KEY_DAEMON_CMD_WORKSPACE_INFO "workspace_info"

#define CONNECTION_TYPE_SSH "SSH"
#define CONNECTION_TYPE_SSH_LEN 3
#define CONNECTION_TYPE_SSH_HOST_ALIAS "SSH_HOST_ALIAS"
#define CONNECTION_TYPE_SSH_HOST_ALIAS_LEN 14
#define CONNECTION_TYPE_RSYNC_DAEMON "RSYNC_DAEMON"
#define CONNECTION_TYPE_RSYNC_DAEMON_LEN 12

#define CHECK_CONNECTION_TYPE(x, y, z) (strncmp(x, y, z) == 0 && strlen(x) == (z))
#define IS_SSH_CONNECTION_TYPE(x) CHECK_CONNECTION_TYPE(x, CONNECTION_TYPE_SSH, CONNECTION_TYPE_SSH_LEN)
#define IS_SSH_HOST_ALIAS_CONNECTION_TYPE(x) CHECK_CONNECTION_TYPE(x, CONNECTION_TYPE_SSH_HOST_ALIAS, CONNECTION_TYPE_SSH_HOST_ALIAS_LEN)
#define IS_RSYNC_DAEMON_CONNECTION_TYPE(x) CHECK_CONNECTION_TYPE(x, CONNECTION_TYPE_RSYNC_DAEMON, CONNECTION_TYPE_RSYNC_DAEMON_LEN)

#define CMD_TYPE_ADD_WORKSPACE "add-workspace"
#define CMD_TYPE_ADD_WORKSPACE_LEN (strlen("add-workspace"))
#define CMD_TYPE_REMOVE_WORKSPACE "remove-workspace"
#define CMD_TYPE_REMOVE_WORKSPACE_LEN (strlen("remove-workspace"))
#define CMD_TYPE_ADD_REMOTE_SYSTEM "add-remote-system"
#define CMD_TYPE_ADD_REMOTE_SYSTEM_LEN (strlen("add-remote-system"))
#define CMD_TYPE_REMOVE_REMOTE_SYSTEM "remove-remote-system"
#define CMD_TYPE_REMOVE_REMOTE_SYSTEM_LEN (strlen("remove-remote-system"))

#define CHECK_CMD_TYPE(x,y,z) (strncmp(x,y,z) == 0 && strlen(x) == (z))
#define IS_ADD_WORKSPACE_CMD(x) CHECK_CMD_TYPE(x, CMD_TYPE_ADD_WORKSPACE, CMD_TYPE_ADD_WORKSPACE_LEN)
#define IS_REMOVE_WORKSPACE_CMD(x) CHECK_CMD_TYPE(x, CMD_TYPE_REMOVE_WORKSPACE, CMD_TYPE_REMOVE_WORKSPACE_LEN)
#define IS_ADD_REMOTE_SYSTEM_CMD(x) CHECK_CMD_TYPE(x, CMD_TYPE_ADD_REMOTE_SYSTEM, CMD_TYPE_ADD_REMOTE_SYSTEM_LEN)
#define IS_REMOVE_REMOTE_SYSTEM_CMD(x) CHECK_CMD_TYPE(x, CMD_TYPE_REMOVE_REMOTE_SYSTEM, CMD_TYPE_REMOVE_REMOTE_SYSTEM_LEN)

/*
 * Error handling macros
 */
#define JSON_MEMBER_MISSING(x,y) format_string("Key '%s' is missing from \n'%s'", x, JSON_PRINT(y))
#define JSON_PRINT(x) (cJSON_Print(x))

#define MIN_PORT_NUMBER 0
#define MAX_PORT_NUMBER 65535

typedef enum ConnectionType {
    OTHER_CONNECTION_TYPE,
    SSH,
    SSH_HOST_ALIAS,
    RSYNC_DAEMON
} ConnectionType;

typedef struct RsyncConnectionInformation {
    char *username;
    char *hostname;
    int port;
} RsyncConnectionInformation;

typedef struct SshConnectionInformation {
    char *username;
    char *hostname;
    char *path_to_identity_file;
} SshConnectionInformation;

typedef struct RemoteWorkspaceMetadata {
    char *remote_workspace_root_path;
    ConnectionType connection_type;

    union {
        SshConnectionInformation *sshConnectionInformation;
        char *ssh_host_alias;
        RsyncConnectionInformation *rsyncConnectionInformation;
    } connection_information;

    struct RemoteWorkspaceMetadata *next;
} RemoteWorkspaceMetadata;

typedef struct WorkspaceInformation {
    char *local_workspace_root_path;
    RemoteWorkspaceMetadata *remote_systems;
} WorkspaceInformation;

typedef enum ResyncCommandType {
    OTHER_RESYNC_COMMAND_TYPE,
    ADD_WORKSPACE,
    REMOVE_WORKSPACE,
    ADD_REMOTE_SYSTEM,
    REMOVE_REMOTE_SYSTEM
} ResyncCommandType;

typedef struct ResyncDaemonCommand {
    ResyncCommandType command_type;
    WorkspaceInformation *workspace_information;
} ResyncDaemonCommand;

/*
 * Applies to all exposed functions:
 *      On success, the expected result is returned, upon failure, NULL is returned and the error msg argument
 *      is populated with a descriptive error message.
 */

WorkspaceInformation *stringified_json_to_workspace_information(const char *stringified_json_object, char **error_msg);

WorkspaceInformation *json_to_workspace_information(const cJSON *json_object, char **error_msg);

cJSON *workspace_information_to_json(WorkspaceInformation *workspace_information, char **error_msg);

char *workspace_information_to_stringified_json(WorkspaceInformation *workspace_information, char **error_msg);

cJSON *remote_workspace_metadata_to_json(RemoteWorkspaceMetadata *remote_workspace_metadata, char **error_msg);

ResyncDaemonCommand *stringified_json_to_resync_daemon_command(const char *stringified_json_object, char **error_msg);

ResyncDaemonCommand *json_to_resync_daemon_command(const cJSON *json_object, char **error_msg);

cJSON *resync_daemon_command_to_json(ResyncDaemonCommand *resync_daemon_command, char **error_msg);

char *resync_daemon_command_to_stringified_json(ResyncDaemonCommand *resync_daemon_command, char **error_msg);

ResyncCommandType string_to_resync_command_type(const char *stringified_command_type);

char *resync_command_type_to_string(ResyncCommandType command_type);

ConnectionType string_to_connection_type(const char *stringified_connection_type);

char *connection_type_to_string(ConnectionType connection_type);

#endif //RESYNC_TYPES_H
