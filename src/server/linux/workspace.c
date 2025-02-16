#include "workspace.h"

WorkspaceInformation *ws_info = NULL;

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
    value->absolute_directory_path = resync_strdup(absolute_directory_path);
    value->path_relative_to_ws_root = resync_strdup(path_relative_to_ws_root);
    value->watch_descriptors_of_direct_subdirs = NULL;
    return value;
}

static void
destroy_watch_metadata(WatchMetadata **metadata)
{
    DO_FREE((*metadata)->absolute_directory_path);
    DO_FREE((*metadata)->path_relative_to_ws_root);
    DO_FREE(*metadata);
}

static WatchMetadata *
get_metadata_optional(WatchMetadata *table, void *key, const ssize_t size)
{
    WatchMetadata *entry;
    HASH_FIND(hh, table, key, size, entry);
    return entry;
}

static WatchMetadata *
get_metadata_required(WatchMetadata *table, void *key, const ssize_t size, char *error_msg)
{
    WatchMetadata *entry = get_metadata_optional(table, key, size);
    if (entry == NULL) {
        fatal_custom_error(error_msg);
    }
    return entry;
}

static int
register_watches(const int inotify_fd, const char *absolute_workspace_root_path, const char *path_relative_to_ws_root)
{
    char *absolute_directory_path = concat_paths(absolute_workspace_root_path, path_relative_to_ws_root);

    const int watch_fd = inotify_add_watch(inotify_fd, absolute_directory_path, WATCH_EVENT_MASK | MISC_EVENT_MASK);
    if (watch_fd == -1) {
        fatal_error("inotify_add_watch");
    }

    LOG("Registering directory: '%s' with descriptor '%d'", absolute_directory_path, watch_fd);

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
    DirectoryPathList *subdir_list = get_paths_of_subdirectories(path);

    DirectoryPathList *entry;
    LL_FOREACH(subdir_list, entry) {
        register_watches(
                inotify_fd,
                entry->path->workspace_root_path,
                entry->path->subdir_path_relative_to_ws_root
        );
    }

    // Add watch descriptor to parent's list of subdir watch descriptors.
    if (strlen(absolute_directory_path) > strlen(absolute_workspace_root_path)) {
        char *absolute_path_to_parent = get_path_to_parent_directory(absolute_directory_path);
        if (absolute_path_to_parent == NULL) {
            goto skip_adding_descriptor_to_parent;
        }

        WatchMetadata *parent_watch_metadata_abs_path = GET_METADATA_BY_STR_REQUIRED(absolute_path_to_parent);
        WatchMetadata *parent_watch_metadata_descriptor = GET_METADATA_BY_INT_REQUIRED(&parent_watch_metadata_abs_path->watch_fd);

        WatchDescriptorList *abs_path_entry = create_watch_descriptor_list_entry(watch_fd);
        LL_APPEND(parent_watch_metadata_abs_path->watch_descriptors_of_direct_subdirs, abs_path_entry);
        WatchDescriptorList *descriptor_entry = create_watch_descriptor_list_entry(watch_fd);
        LL_APPEND(parent_watch_metadata_descriptor->watch_descriptors_of_direct_subdirs, descriptor_entry);

        DO_FREE(absolute_path_to_parent);
    }

skip_adding_descriptor_to_parent:

    DO_FREE(absolute_directory_path);
    DO_FREE(path);

    DirectoryPathList *elt, *tmp;
    LL_FOREACH_SAFE(subdir_list, elt, tmp) {
        LL_DELETE(subdir_list, elt);
        destroy_directory_path(&(elt->path));
        DO_FREE(elt);
    }

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

    inotify_rm_watch(inotify_fd, watch_metadata->watch_fd);
    LOG("Called 'remove_watches' for dir '%s' with descriptor '%d'.", watch_metadata->absolute_directory_path, watch_metadata->watch_fd);

    // Remove the watch descriptor from the parent's list of subdirectory descriptors.
    if (strlen(watch_metadata->absolute_directory_path) > strlen(ws_info->absolute_ws_root_path)) {
        char *abs_parent_path = get_path_to_parent_directory(watch_metadata->absolute_directory_path);
        if (abs_parent_path == NULL) {
            goto skip_removal_from_parent;
        }

        WatchMetadata *parent_watch_metadata_abs_path;
        HASH_FIND_STR(absolute_path_to_metadata, abs_parent_path, parent_watch_metadata_abs_path);
        if (parent_watch_metadata_abs_path == NULL) {
            fatal_custom_error("No absolute path -> metadata entry stored for parent '%s'.", abs_parent_path);
        }

        WatchMetadata *parent_watch_metadata_descriptor;
        HASH_FIND_INT(watch_descriptor_to_metadata, &(parent_watch_metadata_abs_path->watch_fd), parent_watch_metadata_descriptor);
        if (parent_watch_metadata_descriptor == NULL) {
            fatal_custom_error("No watch descriptor -> metadata entry stored for parent '%s'.", abs_parent_path);
        }

        WatchDescriptorList *entry, *tmp;
        LL_FOREACH_SAFE(parent_watch_metadata_abs_path->watch_descriptors_of_direct_subdirs, entry, tmp) {
            if (entry->descriptor == watch_metadata->watch_fd) {
                LL_DELETE(parent_watch_metadata_abs_path->watch_descriptors_of_direct_subdirs, entry);
                DO_FREE(entry);
                break;
            }
        }

        LL_FOREACH_SAFE(parent_watch_metadata_descriptor->watch_descriptors_of_direct_subdirs, entry, tmp) {
            if (entry->descriptor == watch_metadata->watch_fd) {
                LL_DELETE(parent_watch_metadata_descriptor->watch_descriptors_of_direct_subdirs, entry);
                DO_FREE(entry);
                break;
            }
        }

        DO_FREE(abs_parent_path);
    }

skip_removal_from_parent:
    ; // Adding an empty statement, as a goto label cannot be directly followed by a declaration

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

static char **
create_rsync_command(RemoteSystem *remote_system, char *relative_dir_path)
{
    char **rsync_args = (char **) do_malloc(6 * sizeof(char *));
    char *local_dir_path = concat_paths(ws_info->absolute_ws_root_path, relative_dir_path);
    char *remote_dir_path = concat_paths(remote_system->remote_workspace_root, relative_dir_path);

    rsync_args[0] = resync_strdup("rsync");
    rsync_args[1] = resync_strdup("-az");
    rsync_args[2] = resync_strdup("--delete");
    rsync_args[3] = format_string("%s/", local_dir_path);
    rsync_args[4] = format_string("%s:%s", remote_system->remote_host, remote_dir_path);
    rsync_args[5] = NULL;

    DO_FREE(local_dir_path);
    DO_FREE(remote_dir_path);

    return rsync_args;
}

static void
sync_remote_system(RemoteSystem *remote_system, char *relative_dir_path)
{
    pid_t pid = fork();
    if (pid < 0) {
        fatal_error("fork");
    } else if (pid == 0) {
        char **args = create_rsync_command(remote_system, relative_dir_path);
        execvp("rsync", args);
        fatal_error("execvp");
    }

    int status;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status)) {
        LOG("sync failed");
    }
}

static void
sync_remote_systems(char *relative_dir_path)
{
    RemoteSystem *remote_system;
    LL_FOREACH(ws_info->remote_systems, remote_system) {
        sync_remote_system(remote_system, relative_dir_path);
    }
}

static void
handle_inotify_event(const int inotify_fd, const struct inotify_event *event)
{
    if (event->mask & IN_IGNORED) {
        return;
    }

    WatchMetadata *watch_metadata = GET_METADATA_BY_INT_OPTIONAL(&event->wd);
    if (watch_metadata == NULL) {
        // If we don't store metadata for this watch descriptor (anymore), chances are that this is an old event that is
        //  still enqueued, but we stopped listening for events for this watch.
        return;
    }

    char *absolute_directory_path = concat_paths(ws_info->absolute_ws_root_path, watch_metadata->path_relative_to_ws_root);
    char *resource_absolute_path = concat_paths(absolute_directory_path, event->name);
    char *resource_relative_path = concat_paths(watch_metadata->path_relative_to_ws_root, event->name);

    if ((event->mask & IN_DELETE_SELF) || (event->mask & IN_MOVE_SELF)) {

        // These fs events are only relevant for the workspace root. For all other directories contained in the workspace,
        //  we react to the corresponding event emitted for the parent directory.
        if (strlen(resource_absolute_path) > strlen(ws_info->absolute_ws_root_path)) {
            goto out;
        }

        // TODO: communicate with daemon process to remove this workspace from config file
        return;
    }

    if ((event->mask & IN_CREATE) || (event->mask & IN_MOVED_TO)) {

        struct stat dirstat;
        if (stat(resource_absolute_path, &dirstat) < 0) {
            // This can happen for e.g. swap files, e.g. when using 'vim'.
            goto out;
        }

        if (S_ISDIR(dirstat.st_mode)) {
            register_watches(
                    inotify_fd,
                    ws_info->absolute_ws_root_path,
                    resource_relative_path
            );
        } else {
            if (event->mask & IN_CREATE) {
                // File creation causes both an 'IN_CREATE' and an 'IN_CLOSE_WRITE' event to be emitted. To prevent
                //  unnecessary duplicate syncs with the remote systems, we ignore the 'IN_CREATE' event for files.
                goto out;
            }
        }
    }

    if (((event->mask & IN_DELETE) || (event->mask & IN_MOVED_FROM)) && (event->mask & IN_ISDIR)) {
        WatchMetadata *moved_or_deleted_dir_metadata = GET_METADATA_BY_STR_OPTIONAL(resource_absolute_path);
        if (moved_or_deleted_dir_metadata == NULL) {
            goto out;
        }

        remove_watches(inotify_fd, moved_or_deleted_dir_metadata->watch_fd);
    }

    sync_remote_systems(watch_metadata->path_relative_to_ws_root);

out:
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

    while (1) { // TODO: replace with boolean flag
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
    ws_info = (WorkspaceInformation *) do_malloc(sizeof(WorkspaceInformation*));
    ws_info->absolute_ws_root_path = resync_strdup(argv[1]);

    const int inotify_fd = inotify_init();
    if (inotify_fd == -1) {
        fatal_error("inotify_init");
    }

    // Sync the entire workspace with the workspaces of all remote systems.
    sync_remote_systems(NULL);

    register_watches(inotify_fd, ws_info->absolute_ws_root_path, NULL);

    listen_for_inotify_events(inotify_fd);

    close(inotify_fd);

    return EXIT_SUCCESS;
}
