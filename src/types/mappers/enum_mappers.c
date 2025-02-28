#include "../types.h"
#include "../mappers.h"
#include "mapper_private.h"

ConnectionType
string_to_connection_type(const char *stringified_connection_type)
{
    if (IS_SSH_CONNECTION_TYPE(stringified_connection_type)) {
        return SSH;
    } else if (IS_SSH_HOST_ALIAS_CONNECTION_TYPE(stringified_connection_type)) {
        return SSH_HOST_ALIAS;
    } else if (IS_RSYNC_DAEMON_CONNECTION_TYPE(stringified_connection_type)) {
        return RSYNC_DAEMON;
    }

    return OTHER_CONNECTION_TYPE;
}

char *
connection_type_to_string(ConnectionType connection_type)
{
    switch (connection_type) {
        case SSH:
            return CONNECTION_TYPE_SSH;
        case SSH_HOST_ALIAS:
            return CONNECTION_TYPE_SSH_HOST_ALIAS;
        case RSYNC_DAEMON:
            return CONNECTION_TYPE_RSYNC_DAEMON;
        case OTHER_CONNECTION_TYPE:
        default:
            return NULL;
    }
}

ResyncServerCommandType
string_to_resync_server_command_type(const char *stringified_resync_server_command_type)
{
    if (IS_ADD_WORKSPACE_CMD(stringified_resync_server_command_type)) {
        return ADD_WORKSPACE;
    } else if (IS_ADD_REMOTE_SYSTEM_CMD(stringified_resync_server_command_type)) {
        return ADD_REMOTE_SYSTEM;
    } else if (IS_REMOVE_WORKSPACE_CMD(stringified_resync_server_command_type)) {
        return REMOVE_WORKSPACE;
    } else if (IS_REMOVE_REMOTE_SYSTEM_CMD(stringified_resync_server_command_type)) {
        return REMOVE_REMOTE_SYSTEM;
    }

    return OTHER_RESYNC_SERVER_COMMAND_TYPE;
}

char
*resync_server_command_type_to_string(ResyncServerCommandType command_type)
{
    switch (command_type) {
        case ADD_WORKSPACE:
            return CMD_TYPE_ADD_WORKSPACE;
        case REMOVE_WORKSPACE:
            return CMD_TYPE_REMOVE_WORKSPACE;
        case ADD_REMOTE_SYSTEM:
            return CMD_TYPE_ADD_REMOTE_SYSTEM;
        case REMOVE_REMOTE_SYSTEM:
            return CMD_TYPE_REMOVE_REMOTE_SYSTEM;
        case OTHER_RESYNC_SERVER_COMMAND_TYPE:
        default:
            return NULL;
    }
}
