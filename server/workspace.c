#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
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
char *absolute_workspace_root_path = NULL;

static char *
concat_paths(const char *path1, const char *path2)
{
    char *concat_path = NULL;
    if (path1 != NULL && path2 != NULL) {
        if (asprintf(&concat_path, "%s/%s", path1, path2) < 0) {
            fatal_error("asprintf");
        }
        return concat_path;
    }

    const char *non_null_path = (path1 == NULL) ? path2 : path1;

    concat_path = strdup(non_null_path);
    if (concat_path == NULL) {
        fatal_error("strdup");
    }

    return concat_path;
}

static void
register_watches(const int inotify_fd, const char *workspace_root_path, const char *relative_subdir_path)
{
    char *absolute_directory_path = concat_paths(workspace_root_path, relative_subdir_path);
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

static void
listen_for_inotify_events(const int inotify_fd)
{
    char buf[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));
    const struct inotify_event *event;
    ssize_t len;

    while (1) {
        len = read(inotify_fd, buf, sizeof(buf));
        if (len == -1 && errno != EAGAIN) {
            fatal_error("read");
        }

        if (len <= 0)
            break;

        for (char *ptr = buf; ptr < buf + len; ptr += sizeof(struct inotify_event) + event->len) {
            event = (const struct inotify_event *) ptr;

            //handle_inotify_event(inotify_fd, event);
        }
    }
}

int
main(const int argc, const char **argv)
{
    // TODO: local ws root as first pos arg and remote ws root as second pos arg!!
    if (argc != 2) {
        fatal_custom_error("Usage: ./workspace ROOT_PATH");
    }

    absolute_workspace_root_path = strdup(argv[1]);

    int inotify_fd = inotify_init();
    if (inotify_fd == -1) {
        fatal_error("inotify_init");
    }

    register_watches(inotify_fd, absolute_workspace_root_path, NULL);

    listen_for_inotify_events(inotify_fd);

    close(inotify_fd);
    DO_FREE(absolute_workspace_root_path);

    return EXIT_SUCCESS;
}
