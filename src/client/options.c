#include "options.h"

/*
 * Internal struct that stores the values for the given options.
 */
typedef struct OptionValues {
    char *command;
    char *local_ws_path;
    char *remote_ws_path;
    char *connection_type;
    char *ssh_host_alias;
    char *hostname;
    char *username;
    char *port;
    char *identity_file;
} OptionValues;

/*
 * Utility macros when parsing the given options
 */
#define HANDLE_MAX_OCCURRENCE(x,y,z) do { \
    if (x > y) {                           \
        LOG_ERROR("Option '%s' must be specified at most %d time(s)!", z, y); \
        goto error_out;                    \
        }                                  \
    } while (0)

/*
 * Utility macros when validating the parsed input options.
 */
#define PRINT_OPTION_VAL_MISSING(x) (LOG_ERROR("Required option '%s' is missing!", x))
#define PRINT_OPTION_VAL_BLANK(x) (LOG_ERROR("The value for option '%s' must not be blank!", x))

#define PRINT_REQUIRED_OPTION_FOR_CONNECTION_TYPE(x,y) (LOG_ERROR("When using the '%s' connection type, the option '%s' must be specified!", y, x))
#define PRINT_IGNORE_OPTION_FOR_CONNECTION_TYPE(x, y) (LOG_ERROR("Specified option '%s' is ignored when using connection type '%s'", x, y))

/*
 * The long options accepted by this client.
 */
static struct option long_options[] = {
        {OPT_LONG_NAME_COMMAND, required_argument, 0, OPT_LONG_VAL_COMMAND},
        {OPT_LONG_NAME_LOCAL_WORKSPACE_PATH, required_argument, NULL, OPT_LONG_VAL_LOCAL_WORKSPACE_PATH},
        {OPT_LONG_NAME_REMOTE_WORKSPACE_PATH, required_argument, NULL, OPT_LONG_VAL_REMOTE_WORKSPACE_PATH},
        {OPT_LONG_NAME_CONNECTION_TYPE, required_argument, NULL, OPT_LONG_VAL_CONNECTION_TYPE},
        {OPT_LONG_NAME_SSH_HOST_ALIAS, required_argument, NULL, OPT_LONG_VAL_SSH_HOST_ALIAS},
        {OPT_LONG_NAME_HOSTNAME, required_argument, NULL, OPT_LONG_VAL_HOSTNAME},
        {OPT_LONG_NAME_USERNAME, required_argument, NULL, OPT_LONG_VAL_USERNAME},
        {OPT_LONG_NAME_PORT, required_argument, NULL, OPT_LONG_VAL_PORT},
        {OPT_LONG_NAME_IDENTITY_FILE, required_argument, NULL, OPT_LONG_VAL_IDENTITY_FILE},
        {NULL, 0, NULL, 0}
};

static void
free_optionValues(OptionValues **options_values)
{
    DO_FREE((*options_values)->command);
    DO_FREE((*options_values)->local_ws_path);
    DO_FREE((*options_values)->remote_ws_path);
    DO_FREE((*options_values)->connection_type);
    DO_FREE((*options_values)->ssh_host_alias);
    DO_FREE((*options_values)->hostname);
    DO_FREE((*options_values)->username);
    DO_FREE((*options_values)->port);
    DO_FREE((*options_values)->identity_file);
    DO_FREE(*options_values);
}

static bool
validate_simple_string_option(const char *value, const char *option_usage)
{
    if (value == NULL) {
        PRINT_OPTION_VAL_MISSING(option_usage);
        return false;
    }

    if (is_blank(value)) {
        PRINT_OPTION_VAL_BLANK(option_usage);
        return false;
    }

    return true;
}

static bool
validate_identity_file(const char *value)
{
    bool res;

    res = validate_simple_string_option(value, OPT_USAGE_IDENTITY_FILE);
    if (res == false) {
        return false;
    }

    char *error_msg;
    if (!validate_is_absolute_path(value, &error_msg)) {
        LOG_ERROR("The given path '%s' for the identity file is not an absolute path!", value);
        return false;
    }

    if (!validate_file_exists(value, &error_msg)) {
        LOG("The specified identity file ('%s') does not exist!", value);
        return false;
    }

    return true;
}

static bool
validate_port_number(const char *port)
{
    if (port == NULL) {
        return false;
    }

    // Check if actually a number
    char *endptr;
    long val = strtol(port, &endptr, 10);

    char *base_error_msg = format_string("The port number must be a positive integer in the range [%d, %d", MIN_PORT_NUMBER, MAX_PORT_NUMBER);

    if (endptr == port) {
        LOG_ERROR("The given value for the port number does not contain any digits!\n%s", base_error_msg);
        return false;
    }

    if (*endptr != '\0') {
        LOG_ERROR("The given value for the port number does not only contain digits!\n%s", base_error_msg);
        return false;
    }

    if (val < MIN_PORT_NUMBER || val > MAX_PORT_NUMBER) {
        LOG_ERROR("%s", base_error_msg);
        return false;
    }

    return true;
}

static bool
validate_rsync_daemon_connection_type(OptionValues *option_values)
{
    bool res;

    if (option_values->hostname == NULL) {
        PRINT_REQUIRED_OPTION_FOR_CONNECTION_TYPE(OPT_USAGE_HOSTNAME, CONNECTION_TYPE_RSYNC_DAEMON);
        return false;
    }

    res = validate_simple_string_option(option_values->hostname, OPT_USAGE_HOSTNAME);
    if (res == false) {
        return false;
    }

    if (option_values->username != NULL) {
        res = validate_simple_string_option(option_values->username, OPT_USAGE_USERNAME);
        if (res == false) {
            return false;
        }
    }

    if (option_values->port != NULL) {
        res = validate_port_number(option_values->port);
        if (res == false) {
            return false;
        }
    }

    if (option_values->ssh_host_alias != NULL) {
        PRINT_IGNORE_OPTION_FOR_CONNECTION_TYPE(OPT_USAGE_SSH_HOST_ALIAS, CONNECTION_TYPE_RSYNC_DAEMON);
    }

    if (option_values->identity_file != NULL) {
        PRINT_IGNORE_OPTION_FOR_CONNECTION_TYPE(OPT_USAGE_IDENTITY_FILE, CONNECTION_TYPE_RSYNC_DAEMON);
    }

    return true;
}

static bool
validate_ssh_connection_type(OptionValues *option_values)
{
    bool res;

    if (option_values->hostname == NULL) {
        PRINT_REQUIRED_OPTION_FOR_CONNECTION_TYPE(OPT_USAGE_HOSTNAME, CONNECTION_TYPE_SSH);
        return false;
    }

    res = validate_simple_string_option(option_values->hostname, OPT_USAGE_HOSTNAME);
    if (res == false) {
        return false;
    }

    if (option_values->username != NULL) {
        res = validate_simple_string_option(option_values->username, OPT_USAGE_USERNAME);
        if (res == false) {
            return false;
        }
    }

    if (option_values->identity_file != NULL) {
        res = validate_identity_file(option_values->identity_file);
        if (res == false) {
            return false;
        }
    }

    if (option_values->port != NULL) {
        PRINT_IGNORE_OPTION_FOR_CONNECTION_TYPE(OPT_USAGE_PORT, CONNECTION_TYPE_SSH);
    }

    if (option_values->ssh_host_alias != NULL) {
        PRINT_IGNORE_OPTION_FOR_CONNECTION_TYPE(OPT_USAGE_SSH_HOST_ALIAS, CONNECTION_TYPE_SSH);
    }

    return true;
}

static bool
validate_ssh_host_alias_connection_type(OptionValues *option_values)
{
    bool res;

    if (option_values->ssh_host_alias == NULL) {
        LOG_ERROR("When using the '%s' connection type, the option '%s' must be specified!", CONNECTION_TYPE_SSH_HOST_ALIAS, OPT_USAGE_SSH_HOST_ALIAS);
        return false;
    }

    res = validate_simple_string_option(option_values->ssh_host_alias, OPT_USAGE_SSH_HOST_ALIAS);
    if (res == false) {
        return false;
    }

    if (option_values->hostname != NULL) {
        PRINT_IGNORE_OPTION_FOR_CONNECTION_TYPE(OPT_USAGE_HOSTNAME, CONNECTION_TYPE_SSH_HOST_ALIAS);
    }

    if (option_values->username != NULL) {
        PRINT_IGNORE_OPTION_FOR_CONNECTION_TYPE(OPT_USAGE_USERNAME, CONNECTION_TYPE_SSH_HOST_ALIAS);
    }

    if (option_values->port != NULL) {
        PRINT_IGNORE_OPTION_FOR_CONNECTION_TYPE(OPT_USAGE_PORT, CONNECTION_TYPE_SSH_HOST_ALIAS);
    }

    if (option_values->identity_file != NULL) {
        PRINT_IGNORE_OPTION_FOR_CONNECTION_TYPE(OPT_USAGE_IDENTITY_FILE, CONNECTION_TYPE_SSH_HOST_ALIAS);
    }

    return true;
}

static bool
validate_connection_type(const char *connection_type)
{
    bool res;

    res = validate_simple_string_option(connection_type, OPT_USAGE_CONNECTION_TYPE);
    if (res == false) {
        return false;
    }

    if (string_to_connection_type(connection_type) == OTHER_CONNECTION_TYPE) {
        LOG_ERROR("Value '%s' for option '%s' is not a supported connection type!", connection_type, OPT_USAGE_CONNECTION_TYPE);
        return false;
    }

    return true;
}

static bool
validate_remote_workspace_path(const char *remote_ws_path)
{
    bool res;

    res = validate_simple_string_option(remote_ws_path, OPT_USAGE_REMOTE_WS_PATH);
    if (res == false) {
        return false;
    }

    char *error_msg;
    if (!validate_is_absolute_path(remote_ws_path, &error_msg)) {
        LOG("The given path for the remote workspace ('%s') is not an absolute path!");
        return false;
    }

    return true;
}

static bool
validate_local_workspace_path(const char *local_ws_path)
{
    bool res;

    res = validate_simple_string_option(local_ws_path, OPT_USAGE_LOCAL_WS_PATH);
    if (res == false) {
        return false;
    }

    char *error_msg;
    if (!validate_is_absolute_path(local_ws_path, &error_msg)) {
        LOG("The given path for the local workspace ('%s') is not an absolute path!", local_ws_path);
        return false;
    }

    if (!validate_directory_exists(local_ws_path, &error_msg)) {
        LOG("The specified local workspace directory ('%s') does not exist!", local_ws_path);
        return false;
    }

    return true;
}

static bool
validate_command(const char *command)
{
    bool res;

    res = validate_simple_string_option(command, OPT_USAGE_COMMAND);
    if (res == false) {
        return false;
    }

    if (string_to_resync_command_type(command) == OTHER_RESYNC_COMMAND_TYPE) {
        LOG_ERROR("Value '%s' for option '%s' is not a valid command!", command, OPT_USAGE_COMMAND);
        return false;
    }

    return true;
}

static bool
validate_optionValues(OptionValues *option_values)
{
    bool res;

    res = validate_command(option_values->command);
    if (res == false) {
        return false;
    }

    res = validate_local_workspace_path(option_values->local_ws_path);
    if (res == false) {
        return false;
    }

    res = validate_remote_workspace_path(option_values->remote_ws_path);
    if (res == false) {
        return false;
    }

    res = validate_connection_type(option_values->connection_type);
    if (res == false) {
        return false;
    }

    switch (string_to_connection_type(option_values->connection_type)) {
        case SSH:
            res = validate_ssh_connection_type(option_values);
            break;
        case SSH_HOST_ALIAS:
            res = validate_ssh_host_alias_connection_type(option_values);
            break;
        case RSYNC_DAEMON:
            res = validate_rsync_daemon_connection_type(option_values);
            break;
        default:
            LOG_ERROR("Value '%s' for option '%s' is not a supported connection type!", option_values->connection_type, OPT_USAGE_CONNECTION_TYPE);
            return false;
    }

    if (res == false) {
        return false;
    }

    return true;
}

static ResyncDaemonCommand *
optionValues_to_ResyncDaemonCommand(OptionValues *option_values)
{
    ResyncDaemonCommand *command = (ResyncDaemonCommand *) do_malloc(sizeof(ResyncDaemonCommand));
    command->command_type = string_to_resync_command_type(option_values->command);

    WorkspaceInformation *ws_info = (WorkspaceInformation *) do_malloc(sizeof(WorkspaceInformation));
    ws_info->local_workspace_root_path = resync_strdup(option_values->local_ws_path);

    RemoteWorkspaceMetadata *remote_system = (RemoteWorkspaceMetadata *) do_malloc(sizeof(RemoteWorkspaceMetadata));
    remote_system->remote_workspace_root_path = resync_strdup(option_values->remote_ws_path);
    remote_system->connection_type = string_to_connection_type(option_values->connection_type);

    switch (remote_system->connection_type) {
        case SSH:
            remote_system->connection_information.sshConnectionInformation = (SshConnectionInformation *) do_malloc(
                    sizeof(SshConnectionInformation)
            );
            remote_system->connection_information.sshConnectionInformation->username = resync_strdup(option_values->username);
            remote_system->connection_information.sshConnectionInformation->hostname = resync_strdup(option_values->hostname);
            remote_system->connection_information.sshConnectionInformation->path_to_identity_file = resync_strdup(option_values->identity_file);
            break;
        case SSH_HOST_ALIAS:
            remote_system->connection_information.ssh_host_alias = resync_strdup(option_values->ssh_host_alias);
            break;
        case RSYNC_DAEMON:
            remote_system->connection_information.rsyncConnectionInformation = (RsyncConnectionInformation *) do_malloc(
                    sizeof(RsyncConnectionInformation)
            );
            remote_system->connection_information.rsyncConnectionInformation->hostname = resync_strdup(option_values->hostname);
            remote_system->connection_information.rsyncConnectionInformation->username = resync_strdup(option_values->username);
            if (option_values->port != NULL) {
                remote_system->connection_information.rsyncConnectionInformation->port = strtol(option_values->port, NULL, 10);
            }
        default:
            LOG_ERROR("Encountered unsupported connection type '%s' while parsing user input", option_values->connection_type);
            return NULL;
    }

    ws_info->remote_systems = remote_system;
    command->workspace_information = ws_info;

    return command;
}

ResyncDaemonCommand *
parse_options(int argc, char **argv)
{
    OptionValues *option_values = (OptionValues *) do_calloc(1, sizeof(OptionValues));

    int c;
    int option_index = 0;

    int opt_counter_command = 0;
    int opt_counter_local_ws_path = 0;
    int opt_counter_remote_ws_path = 0;
    int opt_counter_connection_type = 0;
    int opt_counter_ssh_host_alias = 0;
    int opt_counter_hostname = 0;
    int opt_counter_username = 0;
    int opt_counter_port = 0;
    int opt_counter_identity_file = 0;

    while ((c = getopt_long(argc, argv, ALLOWED_SHORT_OPTIONS, long_options, &option_index)) != -1) {

        switch (c) {
            case OPT_LONG_VAL_COMMAND:
            case OPT_SHORT_NAME_COMMAND:
                HANDLE_MAX_OCCURRENCE(++opt_counter_command, 1, OPT_USAGE_COMMAND);
                option_values->command = resync_strdup(optarg);
                break;
            case OPT_LONG_VAL_LOCAL_WORKSPACE_PATH:
                HANDLE_MAX_OCCURRENCE(++opt_counter_local_ws_path, 1, OPT_USAGE_LOCAL_WS_PATH);
                option_values->local_ws_path = resync_strdup(optarg);
                break;
            case OPT_LONG_VAL_REMOTE_WORKSPACE_PATH:
                HANDLE_MAX_OCCURRENCE(++opt_counter_remote_ws_path, 1, OPT_USAGE_REMOTE_WS_PATH);
                option_values->remote_ws_path = resync_strdup(optarg);
                break;
            case OPT_LONG_VAL_CONNECTION_TYPE:
                HANDLE_MAX_OCCURRENCE(++opt_counter_connection_type, 1, OPT_USAGE_CONNECTION_TYPE);
                option_values->connection_type = resync_strdup(optarg);
                break;
            case OPT_LONG_VAL_SSH_HOST_ALIAS:
                HANDLE_MAX_OCCURRENCE(++opt_counter_ssh_host_alias, 1, OPT_USAGE_SSH_HOST_ALIAS);
                option_values->ssh_host_alias = resync_strdup(optarg);
                break;
            case OPT_LONG_VAL_HOSTNAME:
            case OPT_SHORT_NAME_HOSTNAME:
                HANDLE_MAX_OCCURRENCE(++opt_counter_hostname, 1, OPT_USAGE_HOSTNAME);
                option_values->hostname = resync_strdup(optarg);
                break;
            case OPT_LONG_VAL_USERNAME:
            case OPT_SHORT_NAME_USERNAME:
                HANDLE_MAX_OCCURRENCE(++opt_counter_username, 1, OPT_USAGE_USERNAME);
                option_values->username = resync_strdup(optarg);
                break;
            case OPT_LONG_VAL_PORT:
            case OPT_SHORT_NAME_PORT:
                HANDLE_MAX_OCCURRENCE(++opt_counter_port, 1, OPT_USAGE_PORT);
                option_values->port = resync_strdup(optarg);
                break;
            case OPT_LONG_VAL_IDENTITY_FILE:
                HANDLE_MAX_OCCURRENCE(++opt_counter_identity_file, 1, OPT_USAGE_IDENTITY_FILE);
                option_values->identity_file = resync_strdup(optarg);
                break;
            case '?':
                goto error_out;
            default:
                LOG_ERROR("Encountered an unknown error while parsing the options");
                goto error_out;
        }
    }

    if (optind != argc) {
        LOG_ERROR("The client does not accept propositional arguments!");
        goto error_out;
    }

    if (!validate_optionValues(option_values)) {
        goto error_out;
    }

    ResyncDaemonCommand *command = optionValues_to_ResyncDaemonCommand(option_values);
    free_optionValues(&option_values);

    return command;

error_out:
    free_optionValues(&option_values);
    return NULL;
}
