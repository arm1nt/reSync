#ifndef RESYNC_WORKSPACE_H
#define RESYNC_WORKSPACE_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/inotify.h>

#include "util/util.h"
#include "util/ulist.h"
#include "util/utash.h"
#include "util/fs_util.h"
#include "config/config.h"

#define WATCH_EVENT_MASK (IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE | IN_MOVE_SELF)
#define MISC_EVENT_MASK (IN_ONLYDIR)

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

#endif //RESYNC_WORKSPACE_H
