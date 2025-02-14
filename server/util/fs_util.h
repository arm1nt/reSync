#ifndef RESYNC_FS_UTIL_H
#define RESYNC_FS_UTIL_H

#define _GNU_SOURCE

#include <stdio.h>
#include "util.h"

typedef struct DirectoryPath {
    const char *workspace_root_path;
    const char *subdir_path_relative_to_ws_root;
} DirectoryPath;

typedef struct DirectoryPathList {
    DirectoryPath *path;
    struct DirectoryPathList *next;
} DirectoryPathList;

static DirectoryPath *
create_directory_path(const char *workspace_root_path, const char *path_relative_to_ws_root)
{
    DirectoryPath *path = (DirectoryPath *) do_malloc(sizeof(DirectoryPath));
    path->workspace_root_path = resync_strdup(workspace_root_path);
    path->subdir_path_relative_to_ws_root = resync_strdup(path_relative_to_ws_root);
    return path;
}

static DirectoryPathList *
create_directory_path_list_entry(DirectoryPath *path)
{
    DirectoryPathList *entry = (DirectoryPathList *) do_malloc(sizeof(DirectoryPathList));
    entry->path = path;
    return entry;
}

static void
destroy_directory_path(DirectoryPath **dir_path)
{
    DO_FREE((*dir_path)->workspace_root_path);
    DO_FREE((*dir_path)->subdir_path_relative_to_ws_root);
    DO_FREE(*dir_path);
}

// TODO: need an OS-specific implementation
static char *
concat_paths(const char *path1, const char *path2)
{
    if (path1 == NULL && path2 == NULL) {
        fatal_custom_error("No paths to concatenate specified");
    }

    char *concat_path = NULL;
    if (path1 != NULL && path2 != NULL) {
        if (asprintf(&concat_path, "%s/%s", path1, path2) < 0) {
            fatal_error("asprintf");
        }
        return concat_path;
    }

    const char *non_null_path = (path1 == NULL) ? path2 : path1;
    return resync_strdup(non_null_path);
}

static DirectoryPathList *
get_paths_of_subdirectories(const DirectoryPath *path)
{
    DirectoryPathList *head = NULL;
    DIR *dirp;
    struct dirent *dent;

    char *absolute_directory_path = concat_paths(path->workspace_root_path, path->subdir_path_relative_to_ws_root);

    dirp = opendir(absolute_directory_path);
    if (dirp == NULL) {
        fatal_error("opendir");
    }

    while ((dent = readdir(dirp)) != NULL) {

        if (strncmp(dent->d_name, ".", 1) == 0 || strncmp(dent->d_name, "..", 2) == 0) {
            continue;
        }

        char *subdir_absolute_path = concat_paths(absolute_directory_path, dent->d_name);
        char *subdir_path_relative_to_ws_root = concat_paths(path->subdir_path_relative_to_ws_root, dent->d_name);

        struct stat dirstat;
        if (stat(subdir_absolute_path, &dirstat) == -1) {
            fatal_custom_error("'stat' failed for '%s'.", subdir_absolute_path);
        }

        if (!S_ISDIR(dirstat.st_mode)) {
            DO_FREE(subdir_absolute_path);
            DO_FREE(subdir_path_relative_to_ws_root);
            continue;
        }

        DirectoryPath *subdir_path = create_directory_path(path->workspace_root_path, subdir_path_relative_to_ws_root);
        DirectoryPathList *entry = create_directory_path_list_entry(subdir_path);
        LL_APPEND(head, entry);

        DO_FREE(subdir_absolute_path);
        DO_FREE(subdir_path_relative_to_ws_root);
    }

    closedir(dirp);
    DO_FREE(absolute_directory_path);

    return head;
}

/**
 * Returns the absolute path to the parent directory, if one exists.
 *
 * @param directory absolute path to the directory for which the parent dir should be obtained
 * @return path to parent directory if it exists, NULL otherwise
 */
static char *
get_path_to_parent_directory(const char *directory)
{
    if ((directory == NULL) || (strlen(directory) == 1 && strncmp(directory, "/", 1) == 0)) {
        return NULL;
    }

    char *until_last_path_segment = strrchr(directory, '/');
    if (until_last_path_segment == NULL) {
        fatal_custom_error("'%s' is an invalid directory path", directory);
    }

    const ssize_t len = until_last_path_segment - directory;
    const ssize_t parent_path_len = (sizeof(char) * (len+1));

    char *parent_path = (char *) do_malloc(parent_path_len);
    memset(parent_path, '\0', parent_path_len); //Ensures that parent path is null terminated
    memcpy(parent_path, directory, len);

    return parent_path;
}

#endif //RESYNC_FS_UTIL_H
