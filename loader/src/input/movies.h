// Skip any movie with A or START, also ones the game would not let you skip yet (input/movies.cpp).
#pragma once

#include <cstdint>

// Once per pad poll on the game thread, with the Xbox pad state the game gets.
void movies_poll(uint16_t buttons, const uint8_t analog[8]);
