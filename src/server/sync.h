#ifndef RESYNC_SYNC_H
#define RESYNC_SYNC_H

#include "../util/string.h"
#include "../util/fs_util.h"
#include "../util/error.h"
#include "../util/debug.h"
#include "../types.h"

#include <unistd.h>
#include <sys/types.h>

void synchronize_workspace(WorkspaceInformation *workspace_information, const char *relative_path);

#endif //RESYNC_SYNC_H
