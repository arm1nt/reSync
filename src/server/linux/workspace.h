#ifndef RESYNC_WORKSPACE_H
#define RESYNC_WORKSPACE_H

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/inotify.h>

#include "../../../lib/ulist.h"
#include "../../../lib/utash.h"
#include "../util/memory.h"
#include "../util/string.h"
#include "../util/debug.h"
#include "../util/error.h"
#include "../util/fs_util.h"

#define WATCH_EVENT_MASK (IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE | IN_MOVE_SELF)
#define MISC_EVENT_MASK (IN_ONLYDIR)

#define GET_METADATA_BY_INT_OPTIONAL(x) get_metadata_optional(watch_descriptor_to_metadata, (void *) (x), sizeof(int))
#define GET_METADATA_BY_INT_REQUIRED(x) get_metadata_required( \
    watch_descriptor_to_metadata,                              \
    (void *) (x),                                              \
    sizeof(int),                                               \
    "Missing entry in 'descriptor -> metadata' table" \
    )

#define GET_METADATA_BY_STR_OPTIONAL(x) get_metadata_optional(absolute_path_to_metadata, (void *) (x), (unsigned)uthash_strlen(x))
#define GET_METADATA_BY_STR_REQUIRED(x) get_metadata_required( \
    absolute_path_to_metadata,                                 \
    (void *) (x),                                              \
    (unsigned)uthash_strlen(x),                                \
    "Missing entry in 'absolute path -> metadata' table" \
    )

typedef struct WatchDescriptorList {
    int descriptor;
    struct WatchDescriptorList *next;
} WatchDescriptorList;

typedef struct WatchMetadata {
    int watch_fd;
    char *absolute_directory_path;
    char *path_relative_to_ws_root;
    WatchDescriptorList *watch_descriptors_of_direct_subdirs;
    UT_hash_handle hh;
} WatchMetadata;

// Note: The below two struct are conceptually different from the 'RemoteSystem' and 'WorkspaceMetadata' structs
//      defined in the 'config.h' file.
typedef struct RemoteSystem {
    char *remote_host;
    char *remote_workspace_root;
    struct RemoteSystem *next;
} RemoteSystem;

typedef struct WorkspaceInformation {
    char *absolute_ws_root_path;
    RemoteSystem *remote_systems;
} WorkspaceInformation;

#endif //RESYNC_WORKSPACE_H
