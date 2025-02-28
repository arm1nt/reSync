#include "../mappers.h"
#include "util/json_utils.h"

WorkspaceInformation *
cjson_to_workspaceInformation(const cJSON *json_ws_info, char **error_msg)
{
    //TODO:
    return NULL;
}

WorkspaceInformation *
stringified_json_to_workspaceInformation(const char *stringified_json_ws_info, char **error_msg)
{
    if (stringified_json_ws_info == NULL) {
        SET_ERROR_MSG(error_msg, "No stringified json object representation of a workspaces information specified");
        return NULL;
    }

    cJSON *json_ws_info = stringified_cjson_to_cjson(stringified_json_ws_info, error_msg);
    if (json_ws_info == NULL) {
        return NULL;
    }

    return cjson_to_workspaceInformation(json_ws_info, error_msg);
}

cJSON *
workspaceInformation_to_cjson(WorkspaceInformation *ws_info, char **error_msg)
{
    // TODO:
    return NULL;
}

char *
workspaceInformation_to_stringified_json(WorkspaceInformation *ws_info, char **error_msg)
{
    cJSON *json_ws_info = workspaceInformation_to_cjson(ws_info, error_msg);
    if (json_ws_info == NULL) {
        return NULL;
    }

    char *stringified_json_ws_info = cJSON_Print(json_ws_info);
    if (stringified_json_ws_info == NULL) {
        SET_ERROR_MSG(error_msg, "Unable to stringify the JSON representation of the workspace information");
        cJSON_Delete(json_ws_info);
        return NULL;
    }

    cJSON_Delete(json_ws_info);
    return stringified_json_ws_info;
}
