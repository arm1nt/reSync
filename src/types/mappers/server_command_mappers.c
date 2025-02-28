#include "../mappers.h"
#include "util/json_utils.h"
#include "mapper_private.h"

ResyncServerCommand *
cjson_to_resyncServerCommand(const cJSON *json_command, char **error_msg)
{
    //TODO
    return NULL;
}

ResyncServerCommand *
stringified_json_to_resyncServerCommand(const char *stringified_json_command, char **error_msg)
{
    if (stringified_json_command == NULL) {
        SET_ERROR_MSG(error_msg, "No stringified json object representation of a resync server command specified!");
        return NULL;
    }

    cJSON *json_command = stringified_cjson_to_cjson(stringified_json_command, error_msg);
    if (json_command == NULL) {
        return NULL;
    }

    return cjson_to_resyncServerCommand(json_command, error_msg);
}

cJSON *
resyncServerCommand_to_cjson(ResyncServerCommand *command, char **error_msg)
{
    //TODO:
    return NULL;
}

char *
resyncServerCommand_to_stringified_json(ResyncServerCommand *command, char **error_msg)
{
    cJSON *json_command = resyncServerCommand_to_cjson(command, error_msg);
    if (json_command == NULL) {
        return NULL;
    }

    char *stringified_json_command = cJSON_Print(json_command);
    if (stringified_json_command == NULL) {
        SET_ERROR_MSG(error_msg, "Unable to stringify the JSON representation of the command");
        cJSON_Delete(json_command);
        return NULL;
    }

    cJSON_Delete(json_command);
    return stringified_json_command;
}
