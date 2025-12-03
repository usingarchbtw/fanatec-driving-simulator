// simplexcp.cpp - XCP implementation
#include "simplexcp.h"
#include <iostream>
#include <thread>

// Your XCP variables
volatile unsigned char xcp_brake_raw = 0;
volatile unsigned char xcp_throttle_raw = 0;
volatile unsigned short xcp_speed = 0;
volatile unsigned char xcp_mode = 0;

// Thread control
static std::atomic<bool> xcp_running{ false };
static std::thread xcp_thread;

void xcp_background_worker() {
    while (xcp_running.load()) {
        // TODO: Actual XCP communication
        Sleep(10); // 100Hz
    }
}

void xcp_init() {
    if (xcp_running.load()) return;

    xcp_running.store(true);
    xcp_thread = std::thread(xcp_background_worker);

    std::cout << "XCP thread started" << std::endl;
}

void xcp_cleanup() {
    xcp_running.store(false);
    if (xcp_thread.joinable()) {
        xcp_thread.join();
    }
    std::cout << "XCP cleaned up" << std::endl;
}

void xcp_update_variables(int brake_raw, int throttle_raw, int speed, int mode) {
    xcp_brake_raw = static_cast<unsigned char>(brake_raw);
    xcp_throttle_raw = static_cast<unsigned char>(throttle_raw);
    xcp_speed = static_cast<unsigned short>(speed);
    xcp_mode = static_cast<unsigned char>(mode);
}