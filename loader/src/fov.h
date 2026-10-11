// Field of view in play: the game's horizontal FOV (70 degrees) replaced by the port's setting.
#pragma once

void install_fov_patch(); // after the XBE is mapped; applies g_cfg.fov
void fov_set(float degrees); // live (Video > Field of view)
