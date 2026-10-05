// Host audio output: one 48 kHz stereo float stream fed by the DirectSound HLE mixer.
#pragma once

#include <cstdint>

namespace audio_out {

constexpr uint32_t RATE = 48000;
constexpr uint32_t BLOCK = 480; // frames per submitted block (10 ms)

bool open();                  // false: no host audio (the mixer then runs silent on a timer)
uint32_t blocks_wanted();     // how many more blocks the device can queue now
void submit(const float *lr); // one BLOCK of interleaved L/R samples (copied)

} // namespace audio_out
