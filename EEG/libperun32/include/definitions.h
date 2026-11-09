//
// Created by Szymon on 02/11/2026.
//

#ifndef libperun32_DEFINITIONS
#define libperun32_DEFINITIONS

#include <cstdint>

constexpr std::uint8_t DEFAULT_TIMEOUT = 50;

enum class ALT_SETTING : std::uint8_t
{
    CONTROL = 0,
    EP6_BULK_OUT_EP8_BULK_IN = 1,
    EP2_BULK_IN_EP6_BULK_OUT_EP8_BULK_IN = 2,
    EP2_ISO1_IN_EP6_BULK_OUT_EP8_BULK_IN = 3, // 1 microframe on iso
    EP2_ISO2_IN_EP6_BULK_OUT_EP8_BULK_IN = 4 // 2 microframes on iso
};

enum class INIT_STEP : std::uint8_t
{
    INIT_LIBUSB = 0,
    OPEN_DEVICE,
    PREPARE_INTERFACE,
    WAIT_READY,
    CHECK_LINK,
    POWER_ON,
    INIT_ADC,
    STOP_ADC,
    DONE,
    FAILED = 255
};

enum class STATE : std::uint8_t
{
    DISCONNECTED = 0,
    IDLE,
    RUNNING,
    FAILED = 255
};

// Vendor Status Protocol Constants
namespace AmpStatus
{
    enum class VC_STATUS : std::uint8_t
    {
        VERSION = 0x10,
        READY = 0xA5,
        STARTUP = 0x51,
    };

    constexpr std::uint8_t COMMAND = 0xB0;
    constexpr std::uint8_t RESPONSE_LENGTH = 9;
    constexpr std::uint8_t CHECK_MAX_ATTEMPTS = 20;
};

namespace ResponseLength
{
    constexpr std::uint16_t BULK = 512; // IN/OUT
    constexpr std::uint16_t ISOCHRONOUS = 450; // IN
}

constexpr std::uint8_t ENDPOINT_2_ADDR = 0x82;
constexpr std::uint8_t ENDPOINT_6_BULK_OUT_ADDR = 0x06;
constexpr std::uint8_t ENDPOINT_8_BULK_IN_ADDR = 0x88;

constexpr std::uint8_t PERUN_INTERFACE_ID = 0;

constexpr std::uint8_t POWER_ON_ADCS_MAX_ATTEMPTS = 3;

#endif // libperun32_DEFINITIONS
