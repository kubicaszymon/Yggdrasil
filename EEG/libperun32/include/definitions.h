//
// Created by Szymon on 02/11/2026.
//

#ifndef libperun32_DEFINITIONS
#define libperun32_DEFINITIONS

#include <cstdint>
#include <atomic>

constexpr std::uint8_t DEFAULT_TIMEOUT = 50;

enum class AltSetting : std::uint8_t
{
    CONTROL = 0,
    EP6_BULK_OUT_EP8_BULK_IN = 1,
    EP2_BULK_IN_EP6_BULK_OUT_EP8_BULK_IN = 2,
    EP2_ISO1_IN_EP6_BULK_OUT_EP8_BULK_IN = 3, // 1 microframe on iso interval
    EP2_ISO2_IN_EP6_BULK_OUT_EP8_BULK_IN = 4 // 2 microframes on iso interval
};

enum class InitStep : std::uint8_t
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

enum class State : std::uint8_t
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

constexpr std::uint8_t ENDPOINT_2_ADDR = 0x82; // Address of endpoint 2 = ISOCHRONOUS IN
constexpr std::uint8_t ENDPOINT_6_BULK_OUT_ADDR = 0x06; // Address of endpoint 6 = BULK OUT
constexpr std::uint8_t ENDPOINT_8_BULK_IN_ADDR = 0x88; // Address of endpoint 8 = BULK OUT

constexpr std::uint8_t PERUN_INTERFACE_ID = 0; // Default (right now single) interface of perun32 amplifier

constexpr std::uint8_t POWER_ON_ADCS_MAX_ATTEMPTS = 3; // Max attempts that we support for powering up the ADCs

// STREAMING
namespace IsoCommunication
{
    // 1 packet of PACKET_LEN is transferred each microframe (125us at high speed and 1ms and full speed) */
    static constexpr int ISO_TRANSFERS = 8; // How many transfers we support (more transfers = greater confidence that no data is missed)
    static constexpr int ISO_PACKETS = 32; // How many packet each transfer can hold == how much data before OS notification that transfer is completed
    static constexpr int ISO_PACKET_LEN = 450; // Length of single packet

    static constexpr unsigned ISO_TIMEOUT = 100; // ms

    static constexpr size_t MAX_QUEUED_CHUNKS = 4096;
    static constexpr size_t SAMPLE_SIZE = 4 + 4 * 9 * 3; // (timestamp + events) + ((4 ADCS * 9 words - 1 status and 8 channels) * 3 bytes each)

    struct Details
    {
        std::atomic<std::uint64_t> bytes_received_{0};
        std::atomic<std::uint64_t> dropped_chunks_{0};
        std::atomic<std::uint64_t> iso_packet_errors_{0};

        Details() = default;
        Details(const Details &other)
            : bytes_received_(other.bytes_received_.load()),
              dropped_chunks_(other.dropped_chunks_.load()),
              iso_packet_errors_(other.iso_packet_errors_.load()) {}

        Details &operator=(const Details &other)
        {
            if (this != &other)
            {
                bytes_received_ = other.bytes_received_.load();
                dropped_chunks_ = other.dropped_chunks_.load();
                iso_packet_errors_ = other.iso_packet_errors_.load();
            }
            return *this;
        }
    };
}

#endif // libperun32_DEFINITIONS
