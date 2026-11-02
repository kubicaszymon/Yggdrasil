//
// Created by Szymon on 02/11/2026.
//

#ifndef libperun32_DEFINITIONS
#define libperun32_DEFINITIONS

#include <cstdint>

constexpr uint8_t BULK_OUT_ENDPOINT = 0x06;
constexpr uint8_t BULK_IN_ENDPOINT = 0x88;

// Vendor Status Protocol Constants (VC_STATUS 0xB0)
constexpr uint8_t VC_ST_SET_MAX_ATTEMPTS = 20;

constexpr uint8_t VC_ST_VERSION = 0x10; // Expected status header version
constexpr uint8_t VC_ST_READY   = 0xA5; // FPGA fully booted and ready
constexpr uint8_t VC_ST_STARTUP = 0x51; // FPGA warming up / initializing

#endif // libperun32_DEFINITIONS
