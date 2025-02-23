#ifndef RESYNC_FS_UTIL_H
#define RESYNC_FS_UTIL_H

#include "string.h"
#include "../../lib/ulist.h"
#include "memory.h"
#include "error.h"

#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct DirectoryPath {
    const char *workspace_root_path;
    const char *subdir_path_relative_to_ws_root;
} DirectoryPath;

typedef struct DirectoryPathList {
    DirectoryPath *path;
    struct DirectoryPathList *next;
} DirectoryPathList;

DirectoryPath *create_directory_path(const char *workspace_root_path, const char *path_relative_to_ws_root);

DirectoryPathList *create_directory_path_list_entry(DirectoryPath *path);

void destroy_directory_path(DirectoryPath **dir_path);

char *concat_paths(const char *path1, const char *path2);

bool validate_absolute_path(const char *path, char **error_msg);

bool validate_directory_exists(const char *path, char **error_msg);

bool validate_file_exists(const char *path, char **error_msg);

DirectoryPathList *get_paths_of_subdirectories(const DirectoryPath *path);

/**
 * Returns the absolute path to the parent directory, if one exists.
 *
 * @param directory absolute path to the directory for which the parent dir should be obtained
 * @return path to parent directory if it exists, NULL otherwise
 */
char *get_path_to_parent_directory(const char *directory);


#endif //RESYNC_FS_UTIL_H
