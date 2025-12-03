// simplexcp.h - C++11 compatible
#pragma once

#include <windows.h>
#include <cstdint>
#include <atomic>

// Use standard C++11 types
extern volatile uint8_t xcp_brake_raw;
extern volatile uint8_t xcp_throttle_raw;
extern volatile uint16_t xcp_speed;
extern volatile uint8_t xcp_mode;

void xcp_init();
void xcp_cleanup();
void xcp_update_variables(int brake_raw, int throttle_raw, int speed, int mode);