#ifndef LAUNCH_H
#define LAUNCH_H

#include "config.h"

/**
 * Launch RetroArch with the given ROM path.
 * Returns 1 on success, 0 on failure.
 */
int launch_retroarch(const char* rom_path);

#endif // LAUNCH_H
