#ifndef RESYNC_OPTIONS_H
#define RESYNC_OPTIONS_H

#include "../util/fs_util.h"
#include "../types.h"

#include <stdbool.h>
#include <getopt.h>

/*
 * Short and long form options recognized by the client with their description.
 */

#define OPT_LONG_NAME_COMMAND "command"
#define OPT_SHORT_NAME_COMMAND 'c'
#define OPT_LONG_VAL_COMMAND 1
#define OPT_USAGE_COMMAND (format_string("-%c / --%s", OPT_SHORT_NAME_COMMAND, OPT_LONG_NAME_COMMAND))
#define OPT_DESCRIPTION_COMMAND "TODO"

#define OPT_LONG_NAME_LOCAL_WORKSPACE_PATH "local-ws-path"
#define OPT_LONG_VAL_LOCAL_WORKSPACE_PATH 2
#define OPT_USAGE_LOCAL_WS_PATH (format_string("--%s", OPT_LONG_NAME_LOCAL_WORKSPACE_PATH))
#define OPT_DESCRIPTION_LOCAL_WS_PATH "TODO"

#define OPT_LONG_NAME_REMOTE_WORKSPACE_PATH "remote-ws-path"
#define OPT_LONG_VAL_REMOTE_WORKSPACE_PATH 3
#define OPT_USAGE_REMOTE_WS_PATH (format_string("--%s", OPT_LONG_NAME_REMOTE_WORKSPACE_PATH))
#define OPT_DESCRIPTION_REMOTE_WS_PATH "TODO"

#define OPT_LONG_NAME_CONNECTION_TYPE "connection-type"
#define OPT_LONG_VAL_CONNECTION_TYPE 4
#define OPT_USAGE_CONNECTION_TYPE (format_string("--%s", OPT_LONG_NAME_CONNECTION_TYPE))
#define OPT_DESCRIPTION_CONNECTION_TYPE "TODO"

#define OPT_LONG_NAME_SSH_HOST_ALIAS "ssh-host-alias"
#define OPT_LONG_VAL_SSH_HOST_ALIAS 5
#define OPT_USAGE_SSH_HOST_ALIAS (format_string("--%s", OPT_LONG_NAME_SSH_HOST_ALIAS))
#define OPT_DESCRIPTION_SSH_HOST_ALIAS "TODO"

#define OPT_LONG_NAME_HOSTNAME "hostname"
#define OPT_SHORT_NAME_HOSTNAME 'h'
#define OPT_LONG_VAL_HOSTNAME 6
#define OPT_USAGE_HOSTNAME (format_string("-%c / --%s", OPT_SHORT_NAME_HOSTNAME, OPT_LONG_NAME_HOSTNAME))
#define OPT_DESCRIPTION_HOSTNAME "TODO"

#define OPT_LONG_NAME_USERNAME "username"
#define OPT_SHORT_NAME_USERNAME 'u'
#define OPT_LONG_VAL_USERNAME 7
#define OPT_USAGE_USERNAME (format_string("-%c / --%s", OPT_SHORT_NAME_USERNAME, OPT_LONG_NAME_USERNAME))
#define OPT_DESCRIPTION_USERNAME "TODO"

#define OPT_LONG_NAME_PORT "port"
#define OPT_SHORT_NAME_PORT 'p'
#define OPT_LONG_VAL_PORT 8
#define OPT_USAGE_PORT (format_string("-%c / --%s", OPT_SHORT_NAME_PORT, OPT_LONG_NAME_PORT))
#define OPT_DESCRIPTION_PORT "TODO"

#define OPT_LONG_NAME_IDENTITY_FILE "identity-file"
#define OPT_LONG_VAL_IDENTITY_FILE 9
#define OPT_USAGE_IDENTITY_FILE (format_string("--%s", OPT_LONG_NAME_IDENTITY_FILE))
#define OPT_DESCRIPTION_IDENTITY_FILE "TODO"

#define ALLOWED_SHORT_OPTIONS "p:h:u:c:"


ResyncDaemonCommand *parse_options(int argc, char **argv);

#endif //RESYNC_OPTIONS_H
