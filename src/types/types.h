#ifndef RESYNC_TYPES_H
#define RESYNC_TYPES_H

#include "../util/string.h"

typedef enum ConnectionType {
    OTHER_CONNECTION_TYPE,
    SSH,
    SSH_HOST_ALIAS,
    RSYNC_DAEMON
} ConnectionType;

typedef enum ResyncServerCommandType {
    OTHER_RESYNC_SERVER_COMMAND_TYPE,
    ADD_WORKSPACE,
    ADD_REMOTE_SYSTEM,
    REMOVE_WORKSPACE,
    REMOVE_REMOTE_SYSTEM
} ResyncServerCommandType;

typedef struct SshConnectionInformation {
    char *username;
    char *hostname;
    char *path_to_identity_file;
} SshConnectionInformation;

typedef struct RsyncConnectionInformation {
    char *username;
    char *hostname;
    int port;
} RsyncConnectionInformation;

typedef struct RemoteWorkspaceMetadata {
    char *remote_workspace_root_path;

    ConnectionType connection_type;
    union {
        SshConnectionInformation *ssh_connection_information;
        char *ssh_host_alias;
        RsyncConnectionInformation *rsync_connection_information;
    } connection_information;

    struct RemoteWorkspaceMetadata *next;
} RemoteWorkspaceMetadata;

typedef struct WorkspaceInformation {
    char *local_workspace_root_path;
    RemoteWorkspaceMetadata *remote_systems;
} WorkspaceInformation;

typedef struct RemoveRemoteSystemMetadata {
    char *local_workspace_root_path;
    char *remote_workspace_root_path;

    /* Specifying the connection type + connection information allows us to identity the remote system to be removed.
     * This is needed as we can sync a workspace to the same remote workspace path on different remote systems.
     */
    ConnectionType connection_type;
    union {
        /* Remote systems that we connect to via SSH or a rsync daemon can be identified via their hostname */
        char *hostname;
        /* When specifying an ssh host alias to connect via ssh, the alias identifies the remote system */
        char *ssh_host_alias;
    } remote_system_id_information;

} RemoveRemoteSystemMetadata;

typedef struct ResyncServerCommand {
    ResyncServerCommandType command_type;
    union {
        /* Adding a workspace or a remote system */
        WorkspaceInformation *workspace_information;
        /* Removing a workspace */
        char *local_workspace_root_path;
        /* Removing a remote system */
        RemoveRemoteSystemMetadata *rm_remote_system_md;
    } command_metadata;
} ResyncServerCommand;


/*
 * Utility functions to destroy an above defined (dynamically allocated) struct
 */

void destroy_resyncServerCommand(ResyncServerCommand **command);

void destroy_removeRemoteSystemMetadata(RemoveRemoteSystemMetadata **rm_remote_system_md);

void destroy_workspaceInformation(WorkspaceInformation **ws_info);

void destroy_remoteWorkspaceMetadata(RemoteWorkspaceMetadata **remote_ws_md);

void destroy_rsyncConnectionInformation(RsyncConnectionInformation **connection_info);

void destroy_sshConnectionInformation(SshConnectionInformation **connection_info);

#endif //RESYNC_TYPES_H
