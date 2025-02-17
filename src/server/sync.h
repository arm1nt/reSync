#ifndef RESYNC_SYNC_H
#define RESYNC_SYNC_H

#include "../types.h"
#include "../util/error.h"
#include "../util/debug.h"

void synchronize_workspace(WorkspaceInformation *workspace_information, const char *relative_path);

#endif //RESYNC_SYNC_H
