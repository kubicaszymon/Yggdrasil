//
// Created by Szymon on 06/11/2026.
//

#ifndef libperun32_PERUN32_H
#define libperun32_PERUN32_H

#include "libperun32.h"
#include "commands.h"
#include "libusb.h"

class Perun32::pImplPerun32
{
public:
    pImplPerun32(std::uint16_t, std::uint16_t);
    ~pImplPerun32();

    bool connect();
    bool setSamplingRate(Perun32::SamplingRate sampling_rate, bool impedance_mode = false);

    std::string getLastError();
    Perun32::AmplifierDetails amp_details{};
    Perun32::CommunicationDetails communication_details{};

private:
    bool initialConfigureDevice();
    bool resetConfigurations();
    bool evaluateDeviceResponse(const int &response);

    bool powerOnADCs();
    bool configureSampling(Perun32::SamplingRate sampling_rate, bool impedance_mode = false);
    bool syncDeviceClock();

    bool sendCommand(const Commands::Command *command_data);

    libusb_device_handle *device_handle_{nullptr};
    std::string last_error_{};

    int64_t host_sync_timestamp_us_{};
};


#endif // libperun32_PERUN32_H
