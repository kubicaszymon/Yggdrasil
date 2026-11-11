//
// Created by Szymon on 06/11/2026.
//

#ifndef libperun32_PERUN32_H
#define libperun32_PERUN32_H

#include "libperun32.h"
#include "commands.h"
#include "definitions.h"
#include "libusb.h"
#include "IsoStream.h"
#include "EegParser.h"

#include <condition_variable>

class Perun32::pImplPerun32
{
public:
    pImplPerun32(std::uint16_t, std::uint16_t);
    ~pImplPerun32();

    bool connect();
    void disconnect();

    bool readSamples(std::vector<Sample> &out, std::chrono::milliseconds timeout);

    bool start();
    bool stop();

    bool setSamplingRate(SamplingRate sampling_rate, bool impedance_mode = false);

    std::string getLastError();
    AmplifierDetails amp_details{};
    CommunicationDetails communication_details{};

private:
    bool checkIfAmplifierIsReady();
    bool evaluateDeviceResponse(const int &response);

    bool powerOnADCs();
    bool syncDeviceClock();

    bool setAltSetting(AltSetting alt);
    bool sendCommandByBulk(const Commands::Command *command_data);

    bool abortStreaming();

    State state_{State::DISCONNECTED};
    AltSetting current_alt_setting_{AltSetting::CONTROL};

    libusb_device_handle *device_handle_{nullptr};
    std::string last_error_{};

    int64_t host_sync_timestamp_us_{};

    IsoStream stream_{};
    EegParser parser_{};
};


#endif // libperun32_PERUN32_H
