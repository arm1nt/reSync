#define _GNU_SOURCE

#include "workspace.h"

WorkspaceMetadata *ws_metadata = NULL;

WatchMetadata *absolute_path_to_metadata = NULL;
WatchMetadata *watch_descriptor_to_metadata = NULL;

static WatchDescriptorList *
create_watch_descriptor_list_entry(const int watch_descriptor)
{
    WatchDescriptorList *entry = (WatchDescriptorList *) do_malloc(sizeof(WatchDescriptorList));
    entry->descriptor = watch_descriptor;
    return entry;
}

static WatchMetadata *
create_watch_metadata(const int watch_fd, const char *absolute_directory_path, const char *path_relative_to_ws_root)
{
    WatchMetadata *value = (WatchMetadata *) do_malloc(sizeof(WatchMetadata));
    value->watch_fd = watch_fd;
    value->absolute_directory_path = strdup(absolute_directory_path);
    value->path_relative_to_ws_root = (path_relative_to_ws_root == NULL) ? NULL : strdup(path_relative_to_ws_root);
    value->watch_descriptors_of_direct_subdirs = NULL;
    return value;
}

static void
destroy_watch_metadata(WatchMetadata **metadata) {
    //TODO: free fields as well
    DO_FREE(*metadata);
}

static int
register_watches(const int inotify_fd, const char *absolute_workspace_root_path, const char *path_relative_to_ws_root)
{
    char *absolute_directory_path = concat_paths(absolute_workspace_root_path, path_relative_to_ws_root);
    LOG("Registering directory: '%s'", absolute_directory_path);

    const int watch_fd = inotify_add_watch(inotify_fd, absolute_directory_path, WATCH_EVENT_MASK | MISC_EVENT_MASK);
    if (watch_fd == -1) {
        fatal_error("inotify_add_watch");
    }

    WatchMetadata *val_descriptor_to_metadata = create_watch_metadata(
            watch_fd,
            absolute_directory_path,
            path_relative_to_ws_root
    );
    HASH_ADD_INT(watch_descriptor_to_metadata, watch_fd, val_descriptor_to_metadata);

    WatchMetadata *val_path_to_metadata = create_watch_metadata(
            watch_fd,
            absolute_directory_path,
            path_relative_to_ws_root
    );
    HASH_ADD_STR(absolute_path_to_metadata, absolute_directory_path, val_path_to_metadata);

    // Register all subdirectories of the current directory with the inotify instance.
    const DirectoryPath *path = create_directory_path(absolute_workspace_root_path, path_relative_to_ws_root);
    const DirectoryPathList *subdir_list = get_paths_of_subdirectories(path);

    const DirectoryPathList *entry;
    LL_FOREACH(subdir_list, entry) {
        register_watches(
                inotify_fd,
                absolute_workspace_root_path,
                entry->path->subdir_path_relative_to_ws_root
        );
    }

    // Add watch descriptor to parent's list of subdir watch descriptors.
    if (strlen(absolute_directory_path) > strlen(absolute_workspace_root_path)) {
        char *absolute_path_to_parent = get_path_to_parent_directory(absolute_directory_path);
        if (absolute_path_to_parent == NULL) {
            goto skip_adding_descriptor_to_parent;
        }

        WatchMetadata *parent_watch_metadata;
        HASH_FIND_STR(absolute_path_to_metadata, absolute_path_to_parent, parent_watch_metadata);
        if (parent_watch_metadata == NULL) {
            fatal_custom_error("No metadata stored for directory '%s'.", absolute_path_to_parent);
        }

        WatchDescriptorList *current_descriptor = create_watch_descriptor_list_entry(watch_fd);
        LL_APPEND(parent_watch_metadata->watch_descriptors_of_direct_subdirs, current_descriptor);
    }

skip_adding_descriptor_to_parent:

    // TODO: free resources

    return watch_fd;
}

static void
remove_watches(const int inotify_fd, const int watch_descriptor)
{
    WatchMetadata *watch_metadata;
    HASH_FIND_INT(watch_descriptor_to_metadata, &watch_descriptor, watch_metadata);

    if (watch_metadata == NULL) {
        fatal_custom_error("no metadata is stored for watch descriptor '%d'.", watch_descriptor);
    }

    int res = inotify_rm_watch(inotify_fd, watch_metadata->watch_fd);
    if (res == -1) {
        fatal_error("inotify_rm_watch");
    }

    // Remove the watch descriptor from the parent's list of subdirectory descriptors.
    if (strlen(watch_metadata->absolute_directory_path) > strlen(ws_metadata->absolute_ws_root_path)) {
        char *abs_parent_path = get_path_to_parent_directory(watch_metadata->absolute_directory_path);
        if (abs_parent_path == NULL) {
            goto skip_removal_from_parent;
        }

        WatchMetadata *parent_watch_metadata;
        HASH_FIND_STR(absolute_path_to_metadata, abs_parent_path, parent_watch_metadata);
        if (parent_watch_metadata == NULL) {
            fatal_custom_error("No metadata stored for parent descriptor '%s'.", abs_parent_path);
        }

        WatchDescriptorList *entry, *tmp;
        LL_FOREACH_SAFE(parent_watch_metadata->watch_descriptors_of_direct_subdirs, entry, tmp) {
            if (entry->descriptor == watch_metadata->watch_fd) {
                LL_DELETE(parent_watch_metadata->watch_descriptors_of_direct_subdirs, entry);
                DO_FREE(entry);
                break;
            }
        }
    }

skip_removal_from_parent:

    // Remove watches of all subdirectories
    WatchDescriptorList *entry;
    LL_FOREACH(watch_metadata->watch_descriptors_of_direct_subdirs, entry) {
        remove_watches(inotify_fd, entry->descriptor);
    }

    // Remove stored watch metadata from the hash tables
    WatchMetadata *metadata_from_path_to_metadata_table;
    HASH_FIND_STR(absolute_path_to_metadata, watch_metadata->absolute_directory_path, metadata_from_path_to_metadata_table);
    if (metadata_from_path_to_metadata_table == NULL) {
        fatal_custom_error("No metadata stored for '%s'", watch_metadata->absolute_directory_path);
    }

    HASH_DEL(absolute_path_to_metadata, metadata_from_path_to_metadata_table);
    HASH_DEL(watch_descriptor_to_metadata, watch_metadata);
    destroy_watch_metadata(&metadata_from_path_to_metadata_table);
    destroy_watch_metadata(&watch_metadata);
}

static void
create_rsync_command(void)
{
    // TODO
}

static void
execute_rsync_command(void)
{
    // TODO
}

static void
handle_inotify_event(const int inotify_fd, const struct inotify_event *event)
{
    if (event->mask & IN_IGNORED) {
        return;
    }

    WatchMetadata *watch_metadata;
    HASH_FIND_INT(watch_descriptor_to_metadata, &event->wd, watch_metadata);

    if (watch_metadata == NULL) {
        fatal_custom_error("No metadata stored for watch descriptor '%d'.", event->wd);
    }

    char *absolute_directory_path = concat_paths(ws_metadata->absolute_ws_root_path, watch_metadata->path_relative_to_ws_root);
    char *resource_absolute_path = concat_paths(absolute_directory_path, event->name);
    char *resource_relative_path = concat_paths(watch_metadata->path_relative_to_ws_root, event->name);

    if ((event->mask & IN_DELETE_SELF) || (event->mask & IN_MOVE_SELF)) {
        // TODO: communicate with daemon process to remove this workspace from config file
    }

    if ((event->mask & IN_CREATE) || (event->mask & IN_MOVED_TO)) {

        struct stat dirstat;
        if (stat(resource_absolute_path, &dirstat) < 0) {
            fatal_custom_error("'stat' failed for resource '%s': %s", resource_absolute_path, strerror(errno));
        }

        if (S_ISDIR(dirstat.st_mode)) {
            register_watches(
                    inotify_fd,
                    ws_metadata->absolute_ws_root_path,
                    resource_relative_path
            );
        } else {
            if (event->mask & IN_CREATE) {
                // File creation causes both an 'IN_CREATE' and an 'IN_CLOSE_WRITE' event to be emitted. To prevent
                //  unnecessary duplicate syncs with the remote systems, we ignore the 'IN_CREATE' event for files.
                DO_FREE(absolute_directory_path);
                DO_FREE(resource_absolute_path);
                DO_FREE(resource_relative_path);

                return;
            }
        }
    }

    if (((event->mask & IN_DELETE) || (event->mask & IN_MOVED_FROM)) && (event->mask & IN_ISDIR)) {
        WatchMetadata *moved_or_deleted_dir_metadata;
        HASH_FIND_STR(absolute_path_to_metadata, resource_absolute_path, moved_or_deleted_dir_metadata);

        if (moved_or_deleted_dir_metadata == NULL) {
            fatal_custom_error("No metadata stored for '%s'", absolute_directory_path);
        }

        remove_watches(inotify_fd, moved_or_deleted_dir_metadata->watch_fd);
    }


    DO_FREE(resource_absolute_path);
    DO_FREE(resource_relative_path);
    DO_FREE(absolute_directory_path);
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

            handle_inotify_event(inotify_fd, event);
        }
    }
}

int
main(const int argc, const char **argv)
{
    /*if (argc < 3) {
        fatal_custom_error("Usage: ./workspace WS_ROOT_PATH REMOTE_SYSTEM...");
    }*/

    //ws_metadata = argv_to_workspace_metadata(argv);
    // TODO: Implement argv -> workspace_metadata struct transformation
    ws_metadata = (WorkspaceMetadata *) do_malloc(sizeof(WorkspaceMetadata));
    ws_metadata->absolute_ws_root_path = resync_strdup(argv[1]);

    const int inotify_fd = inotify_init();
    if (inotify_fd == -1) {
        fatal_error("inotify_init");
    }

    // TODO: Sync the directory with all remote systems

    register_watches(inotify_fd, ws_metadata->absolute_ws_root_path, NULL);

    listen_for_inotify_events(inotify_fd);

    close(inotify_fd);

    return EXIT_SUCCESS;
}

