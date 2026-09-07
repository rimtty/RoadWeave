#pragma once
#include <stdbool.h>
#include <stdint.h>
uint32_t capture_frame_hash(void);
uint32_t capture_outside_wave_hash(void);
bool capture_png(const char *path);
bool capture_diff(const char *reference, const char *output);
