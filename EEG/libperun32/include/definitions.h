//
// Created by Szymon on 02/11/2026.
//

#ifndef libperun32_DEFINITIONS
#define libperun32_DEFINITIONS

#include <cstdint>
#include <atomic>
#include <array>

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
    static constexpr int ISO_TRANSFERS = 8;
    // How many transfers we support (more transfers = greater confidence that no data is missed)
    static constexpr int ISO_PACKETS = 32;
    // How many packet each transfer can hold == how much data before OS notification that transfer is completed
    static constexpr int ISO_PACKET_LEN = 450; // Length of single packet

    static constexpr unsigned ISO_TIMEOUT = 100; // ms

    static constexpr size_t MAX_QUEUED_CHUNKS = 4096;
    static constexpr size_t FRAME_SIZE = 4 + 4 * 9 * 3;
    // (timestamp + events) + ((4 ADCS * 9 words - 1 status and 8 channels) * 3 bytes each)

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

namespace EegData
{
    struct ChannelLayout
    {
        std::array<std::uint8_t, 32> raw_index; // logical channel -> raw ADC index
        std::array<const char *, 32> label;
    };

    /* TAKEN FROM BRAINTECH braintech-drivers-perun32 IMPLEMENTATION!!!! - SK
     *
     *
     This table might explain everything. ADC channel is the raw ordering of sample packet.
        Svarog order    Label on cap	Driver label	ADC_channel	    DB25 Pin number	    Label on db25 adapter
        0               Fp1             Exg_1           0               1                   1
        1               Fp2             Exg_2           1               14                  2
        2               F7              Exg_3           18              6                   3
        3               F3              Exg_4           2               2                   4
        4               Fz              Exg_5           9               10                  5
        5               F4              Exg_6           3               15                  6
        6               F8              Exg_7           19              19                  7
        7               M1              Exg_8           13              12                  8
        8               T3              Exg_9           20              7                   9
        9               C3              Exg_10          4               3                   10
        10              Cz              Exg_11          8               22                  11
        11              C4              Exg_12          5               16                  12
        12              T4              Exg_13          21              20                  13
        13              M2              Exg_14          15              13                  14
        14              T5              Exg_15          22              8                   15
        15              P3              Exg_16          6               4                   16
        16              Pz              Exg_17          10              23                  17
        17              P4              Exg_18          7               17                  18
        18              T6              Exg_19          23              21                  19
        19              O1              Exg_20          16              5                   20
        20              O2              Exg_21          17              18                  21
        21              A1              Exg_22          11              11                  22
        22              A2              Exg_23          12              24                  23
        23              AFz/GND         Exg_24          14              25                  24/GND
                        REF                             REF             9                   REF
    */
    inline constexpr ChannelLayout layout32{
        {
            0, 1, 18, 2, 9, 3, 19, 13, 20, 4, 8, 5, 21, 15, 22, 6, 10, 7, 23, 16, 17, 11, 12, 14,
            24, 25, 26, 27, 28, 29, 30, 31
        },
        {
            "Fp1", "Fp2", "F7", "F3", "Fz", "F4", "F8", "M1", "T3", "C3", "Cz", "C4", "T4", "M2",
            "T5", "P3", "Pz", "P4", "T6", "O1", "O2", "A1", "A2", "AFz/GND",
            "Aux1", "Aux2", "Aux3", "Aux4", "Aux5", "Aux6", "Aux7", "Aux8"
        }
    };
}

#endif // libperun32_DEFINITIONS
