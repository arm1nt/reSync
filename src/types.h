#ifndef RESYNC_TYPES_H
#define RESYNC_TYPES_H

#include "../lib/json/cJSON.h"
#include "../lib/ulist.h"
#include "util/memory.h"
#include "util/error.h"

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

typedef enum ConnectionType {
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


WorkspaceInformation *stringified_json_to_workspace_information(const char *stringified_json_object);

WorkspaceInformation *json_to_workspace_information(const cJSON *json_object);

cJSON *workspace_information_to_json(WorkspaceInformation *workspace_information);

char *workspace_information_to_stringified_json(WorkspaceInformation *workspace_information);

#endif //RESYNC_TYPES_H
