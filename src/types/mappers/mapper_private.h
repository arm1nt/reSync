#ifndef RESYNC_MAPPER_PRIVATE_H
#define RESYNC_MAPPER_PRIVATE_H

#include "../../util/string.h"

/* Server command JSON object */
#define CMD_KEY_COMMAND_TYPE "type"
#define CMD_KEY_COMMAND_METADATA "command-metadata"

/* Remove remote system JSON object (can be a member of a server command JSON object) */
#define RM_RS_KEY_LOCAL_WORKSPACE_ROOT_PATH "local-workspace-root-path"
#define RM_RS_KEY_REMOTE_WORKSPACE_ROOT_PATH "remote-workspace-root-path"
#define RM_RS_KEY_CONNECTION_TYPE "connection-type"
#define RM_RS_KEY_REMOTE_SYSTEM_ID_INFORMATION "remote-system-id-info"
#define RM_RS_MD_KEY_HOSTNAME "hostname"
#define RM_RS_MD_KEY_SSH_HOST_ALIAS "ssh-host-alias"

/* Workspace information JSON object */
// TODO:


/*
 * Utility macros to e.g. map between enums and their string representation
 */
#define CONNECTION_TYPE_SSH "SSH"
#define CONNECTION_TYPE_SSH_LEN (strlen(CONNECTION_TYPE_SSH))
#define CONNECTION_TYPE_SSH_HOST_ALIAS "SSH_HOST_ALIAS"
#define CONNECTION_TYPE_SSH_HOST_ALIAS_LEN (strlen(CONNECTION_TYPE_SSH_HOST_ALIAS))
#define CONNECTION_TYPE_RSYNC_DAEMON "RSYNC_DAEMON"
#define CONNECTION_TYPE_RSYNC_DAEMON_LEN (strlen(CONNECTION_TYPE_RSYNC_DAEMON))

#define CHECK_CONNECTION_TYPE(x, y, z) ((x) != NULL && strncmp(x, y, z) == 0 && strlen(x) == (z))
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

#define CHECK_CMD_TYPE(x,y,z) ((x) != NULL && strncmp(x,y,z) == 0 && strlen(x) == (z))
#define IS_ADD_WORKSPACE_CMD(x) CHECK_CMD_TYPE(x, CMD_TYPE_ADD_WORKSPACE, CMD_TYPE_ADD_WORKSPACE_LEN)
#define IS_REMOVE_WORKSPACE_CMD(x) CHECK_CMD_TYPE(x, CMD_TYPE_REMOVE_WORKSPACE, CMD_TYPE_REMOVE_WORKSPACE_LEN)
#define IS_ADD_REMOTE_SYSTEM_CMD(x) CHECK_CMD_TYPE(x, CMD_TYPE_ADD_REMOTE_SYSTEM, CMD_TYPE_ADD_REMOTE_SYSTEM_LEN)
#define IS_REMOVE_REMOTE_SYSTEM_CMD(x) CHECK_CMD_TYPE(x, CMD_TYPE_REMOVE_REMOTE_SYSTEM, CMD_TYPE_REMOVE_REMOTE_SYSTEM_LEN)

#endif //RESYNC_MAPPER_PRIVATE_H
