// Mouse look: the mouse rotates the player directly (patches in the game's player update).
#pragma once

void install_mouselook_patches();
bool mouselook_installed();
// The mouse angle for the next player update, in degrees (yaw + = left, pitch + = the game's pitch rate
// direction). Called once per pad poll; replaces what the previous frame did not use.
void mouselook_set_frame(float yaw_deg, float pitch_deg);
