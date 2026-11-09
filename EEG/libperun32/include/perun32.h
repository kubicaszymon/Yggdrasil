//
// Created by Szymon on 06/11/2026.
//

#ifndef libperun32_PERUN32_H
#define libperun32_PERUN32_H

#include "libperun32.h"
#include "commands.h"
#include "definitions.h"
#include "libusb.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <thread>

class Perun32::pImplPerun32
{
public:
    pImplPerun32(std::uint16_t, std::uint16_t);
    ~pImplPerun32();

    bool connect();
    void disconnect();

    bool readRaw(std::vector<uint8_t> &out, std::chrono::milliseconds timeout);

    bool start();
    bool stop();

    bool setSamplingRate(Perun32::SamplingRate sampling_rate, bool impedance_mode = false);

    std::string getLastError();
    Perun32::AmplifierDetails amp_details{};
    Perun32::CommunicationDetails communication_details{};

private:
    bool checkIfAmplifierIsReady();
    bool evaluateDeviceResponse(const int &response);

    bool powerOnADCs();
    bool syncDeviceClock();

    bool setAltSetting(AltSetting alt);
    bool sendCommandByBulk(const Commands::Command *command_data);

    bool submitTransfers();
    void abortStreaming();
    void eventLoop();
    static void LIBUSB_CALL onIsoTransfer(libusb_transfer* transfer);
    void handleIsoTransfer(libusb_transfer* transfer);

    State state_{State::DISCONNECTED};
    AltSetting current_alt_setting_{AltSetting::CONTROL};

    libusb_device_handle *device_handle_{nullptr};
    std::string last_error_{};

    int64_t host_sync_timestamp_us_{};

////////////////////////////////

    std::atomic<std::uint64_t> bytes_received_{0};
    std::atomic<std::uint64_t> dropped_chunks_{0};
    std::atomic<std::uint64_t> iso_packet_errors_{0};

    std::atomic<bool> streaming_{false};

    std::thread event_thread_;

    std::atomic<int> transfers_in_flight_{0};
    std::vector<libusb_transfer*> transfers_{};
    std::vector<std::vector<std::uint8_t>> transfer_buffers_{};

    std::mutex queue_mutex_{};
    std::condition_variable queue_cv_{};
    std::deque<std::vector<std::uint8_t>> queue_{};

    static constexpr int ISO_TRANSFERS = 8, ISO_PACKETS = 32, ISO_PACKET_LEN = 450;
    static constexpr unsigned ISO_TIMEOUT = 100;       // ms
    static constexpr size_t MAX_QUEUED_CHUNKS = 4096;
    static constexpr size_t FRAME_SIZE = 4 + 4 * 9 * 3;  // 112
};


#endif // libperun32_PERUN32_H
