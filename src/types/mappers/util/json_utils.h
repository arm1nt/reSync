#ifndef RESYNC_JSON_UTILS_H
#define RESYNC_JSON_UTILS_H

#include "../../../util/string.h"
#include "../../../util/error.h"
#include "../../../../lib/json/cJSON.h"

/*
 * Convenience macros for parsing JSON objects
 */
#define STRING_VAL_EXISTS(x) ((x) != NULL && (x)->valuestring != NULL)
#define JSON_MEMBER_MISSING(x,y) format_string("Key '%s' is missing from \n'%s'", x, cJSON_Print(y))

cJSON *create_json_object(void);

cJSON *create_json_array(void);

cJSON *create_json_string(const char *string);

cJSON *create_json_number(const int number);

cJSON *stringified_cjson_to_cjson(const char *stringified_json_object, char **error_msg);

#endif //RESYNC_JSON_UTILS_H
