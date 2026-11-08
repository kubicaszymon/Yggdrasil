//
// Created by Szymon on 06/11/2026.
//Perun32::

#include "perun32.h"

#include <chrono>
#include <thread>

#include "definitions.h"

Perun32::pImplPerun32::pImplPerun32(const uint16_t vid, const uint16_t pid)
    : amp_details{.vid_ = vid, .pid_ = pid} {}

Perun32::pImplPerun32::~pImplPerun32()
{
    if (nullptr != device_handle_)
    {
        libusb_close(device_handle_);
    }
    libusb_exit(nullptr);
}

bool Perun32::pImplPerun32::connect()
{
    bool status = false;
    if (const int init_status = libusb_init_context(nullptr, nullptr, 0);
        LIBUSB_SUCCESS == init_status)
    {
        libusb_device **device_list{};

        // enumerate devices
        if (const ssize_t devices_count = libusb_get_device_list(nullptr, &device_list);
            0 < devices_count)
        {
            libusb_device *device{nullptr};
            libusb_device_descriptor device_descriptor{};
            for (int i = 0; i < devices_count; i++)
            {
                // find the correct one
                libusb_get_device_descriptor(device_list[i], &device_descriptor);
                if (amp_details.vid_ == device_descriptor.idVendor && amp_details.pid_ == device_descriptor.idProduct)
                {
                    // save the model (A,B,C,D) - important for channel count and electrode map
                    amp_details.device_model_ = device_descriptor.bcdDevice;
                    device = device_list[i];
                    break;
                }
            }

            // check if device were found
            if (nullptr != device)
            {
                if (true == evaluateDeviceResponse(libusb_open(device, &device_handle_)))
                {
                    status = initialConfigureDevice();
                }
            } else
            {
                last_error_ = "libusb device with provided pid and vid not found";
            }
        }
        libusb_free_device_list(device_list, 1);
    } else
    {
        last_error_ = "libusb error while initialize library";
    }
    return status;
}

bool Perun32::pImplPerun32::setSamplingRate(const Perun32::SamplingRate sampling_rate, const bool impedance_mode)
{
    if (false == powerOnADCs())
    {
        return false;
    }
    if (false == configureSampling(sampling_rate, impedance_mode))
    {
        return false;
    }
    if (false == syncDeviceClock())
    {
        return false;
    }
    return true;
}

bool Perun32::pImplPerun32::sendCommand(const Commands::Command *command_data)
{
    int transferred{0};
    // send command to bulk OUT endpoint
    int status = libusb_bulk_transfer(
        device_handle_,
        BULK_OUT_ENDPOINT,
        const_cast<uint8_t *>(command_data->data_out.data()),
        static_cast<int>(command_data->data_out.size()),
        &transferred,
        50);

    if (false == evaluateDeviceResponse(status) || transferred != static_cast<int>(command_data->data_out.size()))
    {
        return false;
    }

    // read response from bulk IN endpoint
    std::vector<uint8_t> response_buffer(512);
    status = libusb_bulk_transfer(
        device_handle_,
        BULK_IN_ENDPOINT,
        response_buffer.data(),
        static_cast<int>(response_buffer.size()),
        &transferred,
        50);

    if (false == evaluateDeviceResponse(status) || transferred != static_cast<int>(command_data->data_in.size()))
    {
        return false;
    }

    // masked verification
    int mismatches{0};
    for (size_t i = 0; i < command_data->data_in.size(); i++)
    {
        const uint8_t masked_response = response_buffer[i] & command_data->data_in_mask[i];
        if (const uint8_t expected_byte = command_data->data_in[i];
            masked_response != expected_byte)
        {
            mismatches++;
        }
    }

    // according to old python code, protocol allows up to 10 mismatches
    return mismatches <= 10;
}

std::string Perun32::pImplPerun32::getLastError()
{
    return last_error_;
}

bool Perun32::pImplPerun32::initialConfigureDevice()
{
    // perun32 has only one interface
    libusb_detach_kernel_driver(device_handle_, 0);
    if (true == evaluateDeviceResponse(libusb_claim_interface(device_handle_, 0)))
    {
        return resetConfigurations();
    }
    return false;
}

bool Perun32::pImplPerun32::resetConfigurations()
{
    // alt0 opens endpoint0
    libusb_set_interface_alt_setting(device_handle_, 0, 0);
    std::vector<uint8_t> data(9);

    // check if device is ready
    for (int attempt = 0; attempt < VC_ST_SET_MAX_ATTEMPTS; attempt++)
    {
        int response = libusb_control_transfer(
            device_handle_,
            0xC0, // bit 7 - device-to-host, bit 6-5 - vendor specific request, bit 4-0 - usb device
            0xB0, // perun32's own specific command for report vendor status,
            0,
            0,
            data.data(),
            data.size(),
            50);

        if (true == evaluateDeviceResponse(response) && response == 9)
        {
            if (data.at(0) == VC_ST_VERSION)
            {
                if (data.at(1) == VC_ST_READY) // VC_ST_READY
                {
                    // alt1 means bulk endpoints for setting information will be opened
                    response = libusb_set_interface_alt_setting(device_handle_, 0, 1);
                    return evaluateDeviceResponse(response);
                }
                if (data.at(1) == VC_ST_STARTUP) // VS_ST_STARTUP
                {
                    // if chipset is starting then wait and try again
                    std::this_thread::sleep_for(std::chrono::milliseconds(300));
                    continue;
                }
            }
        }
        // if USB transfer failed or returned unknown status, wait briefly before retrying
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    last_error_ = "cannot initialize FPGA if perun amplifier (max attempts exceeded)";
    return false;
}

bool Perun32::pImplPerun32::evaluateDeviceResponse(const int &response)
{
    if (response >= 0)
    {
        return true;
    }
    last_error_ = libusb_strerror(response);
    return false;
}

bool Perun32::pImplPerun32::powerOnADCs()
{
    for (int attempt = 0; attempt < POWER_ON_ADCS_MAX_ATTEMPTS; attempt++)
    {
        // send power on command to activate ADC power rails
        if (true == sendCommand(&Commands::PowerOn))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            // verify stability by check command
            if (true == sendCommand(&Commands::CheckData))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
                return true;
            }
        }
        // recovery - clear all settings and lets fpga set once again
        resetConfigurations();
    }
    last_error_ = "cannot power on ADC controller";
    return false;
}

bool Perun32::pImplPerun32::configureSampling(const Perun32::SamplingRate sampling_rate, const bool impedance_mode)
{
    // reset adc internal registers
    if (false == sendCommand(&Commands::InitData))
    {
        return false;
    }

    // stop any existing adc read loop
    if (false == sendCommand(&Commands::Stop))
    {
        return false;
    }

    // select proper command
    Commands::Command sampling{};
    switch (sampling_rate)
    {
        case Perun32::SamplingRate::Hz500:
            sampling = impedance_mode ? Commands::ConfigImp500 : Commands::Config500;
            break;
        case Perun32::SamplingRate::Hz1000:
            sampling = impedance_mode ? Commands::ConfigImp1000 : Commands::Config1000;
            break;
        case Perun32::SamplingRate::Hz2000:
            sampling = impedance_mode ? Commands::ConfigImp2000 : Commands::Config2000;
            break;
        case Perun32::SamplingRate::Hz4000:
            sampling = impedance_mode ? Commands::ConfigImp4000 : Commands::Config4000;
            break;
        case Perun32::SamplingRate::Hz8000:
            sampling = impedance_mode ? Commands::ConfigImp8000 : Commands::Config8000;
            break;
        case Perun32::SamplingRate::Hz16000:
            sampling = impedance_mode ? Commands::ConfigImp16000 : Commands::Config16000;
            break;
        case Perun32::SamplingRate::Undefined:
            last_error_ = "Undefined sampling rate";
            return false;
    }

    // send sampling configuration spi block
    if (false == sendCommand(&sampling))
    {
        return false;
    }

    communication_details.sampling_rate = sampling_rate;
    communication_details.impedance_mode = impedance_mode;
    return true;
}

bool Perun32::pImplPerun32::syncDeviceClock()
{
    if (false == sendCommand(&Commands::GetTime))
    {
        return false;
    }

    const auto now = std::chrono::high_resolution_clock::now();
    host_sync_timestamp_us_ = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();

    return true;
}

