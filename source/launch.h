#ifndef LAUNCH_H
#define LAUNCH_H

#include "config.h"

/**
 * Launch RetroArch with the given ROM path.
 * Returns 1 on success, 0 on failure.
 */
int launch_retroarch(const char* rom_path);

#define PPSSPP_PATH "sdmc:/switch/ppsspp/PPSSPP_GL.nro"

#endif // LAUNCH_H
