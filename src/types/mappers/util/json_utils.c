#include "json_utils.h"

cJSON *
create_json_object(void)
{
    cJSON *json_object = cJSON_CreateObject();
    if (json_object == NULL) {
        fatal_custom_error("cJSON_CreateObject failed");
    }

    return json_object;
}

cJSON *
create_json_array(void)
{
    cJSON *json_array = cJSON_CreateArray();
    if (json_array == NULL) {
        fatal_custom_error("cJSON_CreateArray failed");
    }

    return json_array;
}

cJSON *
create_json_string(const char *string)
{
    cJSON *json_string = cJSON_CreateString(string);
    if (json_string == NULL) {
        fatal_custom_error("cJSON_CreateString failed for string '%s'", string);
    }

    return json_string;
}

cJSON *
create_json_number(const int number)
{
    cJSON *json_number = cJSON_CreateNumber(number);
    if (json_number == NULL) {
        fatal_custom_error("cJSON_CreateNumber failed for number '%d'", number);
    }

    return json_number;
}

cJSON *
stringified_cjson_to_cjson(const char *stringified_json_object, char **error_msg)
{
    if (stringified_json_object == NULL) {
        SET_ERROR_MSG(error_msg, "No stringified json object specified!");
        return NULL;
    }

    cJSON *json_object = cJSON_Parse(stringified_json_object);
    if (json_object == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            SET_ERROR_MSG_RAW(error_msg, format_string("Error parsing JSON object before: \n'%s'", error_ptr));
        } else {
            SET_ERROR_MSG_RAW(error_msg, format_string("Error parsing stringified JSON object: \n'%s'", stringified_json_object));
        }
    }

    return json_object;
}
