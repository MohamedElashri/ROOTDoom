// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// GUI-thread-only library entry points. Reuse the existing ROOT application.
// Launch returns 0 on success, 1 with a diagnostic on failure; never calls exit.
#ifdef __cplusplus
extern "C" {
#endif
int RootDoomLaunch(const char* iwad, const char* data_directory);
int RootDoomIsRunning(void);
void RootDoomShutdown(void);
#ifdef __cplusplus
}
#endif
