#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/inotify.h>

#include "util/util.h"
#include "util/utash.h"

#define WATCH_EVENT_MASK (IN_MODIFY | IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_DELETE_SELF)
#define MISC_EVENT_MASK (IN_ONLYDIR | IN_MASK_CREATE)

typedef struct WatchMetadata {
    int watch_fd;
    char *relative_path; // Directory's path relative to the workspaces absolute root path
    UT_hash_handle hh;
} WatchMetadata;

WatchMetadata *metadata = NULL;

static char *
get_absolute_dir_path(const char *workspace_root_path, const char *relative_subdir_path)
{
    char *absolute_path = NULL;

    if (relative_subdir_path == NULL) {
        absolute_path = strdup(workspace_root_path);

        if (absolute_path == NULL) {
            fatal_error("strdup");
        }

        return absolute_path;
    }

    if (asprintf(&absolute_path, "%s/%s", workspace_root_path, relative_subdir_path) < 0) {
        fatal_error("asprintf");
    }

    return absolute_path;
}

static void
register_watches(const int inotify_fd, const char *workspace_root_path, const char *relative_subdir_path)
{
    char *absolute_directory_path = get_absolute_dir_path(workspace_root_path, relative_subdir_path);
    LOG("Registering directory '%s'", absolute_directory_path);

    int watch_fd = inotify_add_watch(inotify_fd, absolute_directory_path, WATCH_EVENT_MASK | MISC_EVENT_MASK);
    if (watch_fd == -1) {
        fatal_error("inotify_add_watch");
    }

    WatchMetadata *watch_metadata = (WatchMetadata *) do_malloc(sizeof *watch_metadata);
    watch_metadata->watch_fd = watch_fd;
    watch_metadata->relative_path = (relative_subdir_path == NULL) ? NULL : strdup(relative_subdir_path);

    HASH_ADD_INT(metadata, watch_fd, watch_metadata);

    // Find all subdirectories of this dir and register them with the inotify instance
    DIR *dirp;
    struct dirent *dent;

    dirp = opendir(absolute_directory_path);
    if (dirp == NULL) {
        fatal_error("opendir");
    }
    DO_FREE(absolute_directory_path);

    while ((dent = readdir(dirp)) != NULL) {

        if (strncmp(dent->d_name, ".", 1) == 0 || strncmp(dent->d_name, "..", 2) == 0) {
            continue;
        }

        char *subdir_absolute_path;
        char *subdir_path_relative_to_ws_root;

        if (relative_subdir_path == NULL) {
            subdir_path_relative_to_ws_root = strdup(dent->d_name);
        } else {
            if (asprintf(&subdir_path_relative_to_ws_root, "%s/%s", relative_subdir_path, dent->d_name) < 0) {
                fatal_error("asprintf");
            }
        }

        if (asprintf(&subdir_absolute_path, "%s/%s", workspace_root_path, subdir_path_relative_to_ws_root) < 0) {
            fatal_error("asprintf");
        }

        if (strlen(subdir_absolute_path) > FILENAME_MAX) {
            fatal_custom_error(
                    "Filename '%s' is longer than the max allowed filename length of '%d'",
                    subdir_absolute_path,
                    FILENAME_MAX
            );
        }

        struct stat dirstat;
        if (stat(subdir_absolute_path, &dirstat) == -1) {
            fatal_custom_error("Stat failed for file '%s'", subdir_absolute_path);
        }

        if (!S_ISDIR(dirstat.st_mode)) {
            DO_FREE(subdir_absolute_path);
            DO_FREE(subdir_path_relative_to_ws_root);
            continue;
        }

        register_watches(inotify_fd, workspace_root_path, subdir_path_relative_to_ws_root);

        DO_FREE(subdir_absolute_path);
        DO_FREE(subdir_path_relative_to_ws_root);
    }
}

int
main(const int argc, const char **argv)
{
    // TODO: local ws root as first pos arg and remote ws root as second pos arg!!
    if (argc != 2) {
        fatal_custom_error("Usage: ./workspace ROOT_PATH");
    }

    const char *workspace_root_path = strdup(argv[1]);

    int inotify_fd = inotify_init();
    if (inotify_fd == -1) {
        fatal_error("inotify_init");
    }

    register_watches(inotify_fd, workspace_root_path, NULL);


    close(inotify_fd);

    return EXIT_SUCCESS;
}
