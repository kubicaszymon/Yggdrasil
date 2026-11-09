//
// Created by Szymon on 06/11/2026.
//Perun32::

#include "perun32.h"

#include <chrono>
#include <filesystem>
#include <thread>

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
    if (STATE::DISCONNECTED != state_)
    {
        last_error_ = "already connected";
        return false;
    }

    auto step{INIT_STEP::INIT_LIBUSB};
    auto next = [&step](const bool ok, const INIT_STEP on_ok)
    {
        step = ok ? on_ok : INIT_STEP::FAILED;
    };

    while (INIT_STEP::DONE != step && INIT_STEP::FAILED != step)
    {
        switch (step)
        {
            case INIT_STEP::INIT_LIBUSB:
                next(evaluateDeviceResponse(libusb_init_context(nullptr, nullptr, 0)), INIT_STEP::OPEN_DEVICE);
                break;

            case INIT_STEP::OPEN_DEVICE:
                device_handle_ = libusb_open_device_with_vid_pid(nullptr, amp_details.vid_, amp_details.pid_);
                if (nullptr == device_handle_)
                {
                    last_error_ = "amplifier not found or cannot be opened";
                    step = INIT_STEP::FAILED;
                    break;
                }
                {
                    // model (A,B,C,D) from bcdDevice
                    libusb_device_descriptor descriptor{};
                    libusb_get_device_descriptor(libusb_get_device(device_handle_), &descriptor);
                    amp_details.device_model_ = descriptor.bcdDevice;
                }
                step = INIT_STEP::PREPARE_INTERFACE;
                break;

            case INIT_STEP::PREPARE_INTERFACE:
                libusb_set_auto_detach_kernel_driver(device_handle_, 1);
                next(evaluateDeviceResponse(libusb_claim_interface(device_handle_, PERUN_INTERFACE_ID))
                     && setAltSetting(ALT_SETTING::CONTROL),
                     INIT_STEP::WAIT_READY);
                break;

            case INIT_STEP::WAIT_READY:
                next(setAltSetting(ALT_SETTING::CONTROL) && checkIfAmplifierIsReady(), INIT_STEP::CHECK_LINK);
                break;

            case INIT_STEP::CHECK_LINK:
                next(setAltSetting(ALT_SETTING::EP6_BULK_OUT_EP8_BULK_IN) && sendCommandByBulk(&Commands::CheckData),
                     INIT_STEP::POWER_ON);
                break;

            case INIT_STEP::POWER_ON:
                next(powerOnADCs(), INIT_STEP::INIT_ADC);
                break;

            case INIT_STEP::INIT_ADC:
                next(sendCommandByBulk(&Commands::InitData), INIT_STEP::STOP_ADC);
                break;

            case INIT_STEP::STOP_ADC:
                next(sendCommandByBulk(&Commands::Stop), INIT_STEP::DONE);
                break;

            default:
                last_error_ = "invalid connect step";
                step = INIT_STEP::FAILED;
                break;
        }
    }
    if (INIT_STEP::FAILED == step)
    {
        disconnect(); // the only cleanup place; keeps last_error_
        return false;
    }
    state_ = STATE::IDLE;
    return true;
}

void Perun32::pImplPerun32::disconnect()
{
    if (nullptr != device_handle_)
    {
        sendCommandByBulk(&Commands::PowerOff);
        libusb_release_interface(device_handle_, PERUN_INTERFACE_ID);
        libusb_close(device_handle_);
        device_handle_ = nullptr;
    }
    libusb_exit(nullptr);
    state_ = STATE::DISCONNECTED;
}

bool Perun32::pImplPerun32::setSamplingRate(const SamplingRate sampling_rate, const bool impedance_mode)
{
    if (STATE::IDLE != state_)
    {
        last_error_ = "sampling rate can be set only when idle";
        return false;
    }

    // select proper command
    Commands::Command sampling{};
    switch (sampling_rate)
    {
        case SamplingRate::Hz500:
            sampling = impedance_mode ? Commands::ConfigImp500 : Commands::Config500;
            break;
        case SamplingRate::Hz1000:
            sampling = impedance_mode ? Commands::ConfigImp1000 : Commands::Config1000;
            break;
        case SamplingRate::Hz2000:
            sampling = impedance_mode ? Commands::ConfigImp2000 : Commands::Config2000;
            break;
        case SamplingRate::Hz4000:
            sampling = impedance_mode ? Commands::ConfigImp4000 : Commands::Config4000;
            break;
        case SamplingRate::Hz8000:
            sampling = impedance_mode ? Commands::ConfigImp8000 : Commands::Config8000;
            break;
        case SamplingRate::Hz16000:
            sampling = impedance_mode ? Commands::ConfigImp16000 : Commands::Config16000;
            break;
        case SamplingRate::Undefined:
            last_error_ = "undefined sampling rate";
            return false;
    }

    communication_details.sampling_rate = SamplingRate::Undefined; // invalid until fully configured

//    const bool ok = powerOnADCs() // pwr_on, check (retry + alt toggle)
//                    && sendCommandByBulk(&Commands::InitData) // reset ADC registers
//                    && sendCommandByBulk(&Commands::Stop) // leave continuous mode
//                    && sendCommandByBulk(&sampling);

    if (false == sendCommandByBulk(&sampling))
    {
        return false;
    }

    communication_details.sampling_rate = sampling_rate;
    communication_details.impedance_mode = impedance_mode;
    return true;
}

bool Perun32::pImplPerun32::sendCommandByBulk(const Commands::Command *command_data)
{
    int transferred{0};
    // send command to bulk OUT endpoint
    int status = libusb_bulk_transfer(
        device_handle_,
        ENDPOINT_6_BULK_OUT_ADDR,
        const_cast<uint8_t *>(command_data->data_out.data()),
        static_cast<int>(command_data->data_out.size()),
        &transferred,
        DEFAULT_TIMEOUT);

    if (false == evaluateDeviceResponse(status) || transferred != static_cast<int>(command_data->data_out.size()))
    {
        return false;
    }

    // read response from bulk IN endpoint
    std::vector<uint8_t> response_buffer(ResponseLength::BULK);
    status = libusb_bulk_transfer(
        device_handle_,
        ENDPOINT_8_BULK_IN_ADDR,
        response_buffer.data(),
        static_cast<int>(response_buffer.size()),
        &transferred,
        DEFAULT_TIMEOUT);

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
        if (mismatches > 10)
        {
            last_error_ = ">10 mismatches in response";
            break;
        }
    }

    // MISMATCHES allowed value according to Braintech's driver
    return mismatches == 0;
}

std::string Perun32::pImplPerun32::getLastError()
{
    return last_error_;
}

bool Perun32::pImplPerun32::checkIfAmplifierIsReady()
{
    std::vector<std::uint8_t> data(AmpStatus::RESPONSE_LENGTH);
    for (int attempt = 0; attempt < AmpStatus::CHECK_MAX_ATTEMPTS; attempt++)
    {
        int response = libusb_control_transfer(
            device_handle_,
            0xC0, // bit 7 - device-to-host, bit 6-5 - vendor specific request, bit 4-0 - usb device
            AmpStatus::COMMAND, // perun32's own specific command for report vendor status,
            0,
            0,
            data.data(),
            data.size(),
            DEFAULT_TIMEOUT);

        if (true == evaluateDeviceResponse(response) && response == 9)
        {
            if (data.at(0) == static_cast<std::uint8_t>(AmpStatus::VC_STATUS::VERSION))
            {
                if (data.at(1) == static_cast<std::uint8_t>(AmpStatus::VC_STATUS::READY)) // VC_ST_READY
                {
                    return evaluateDeviceResponse(response);
                }
                if (data.at(1) == static_cast<std::uint8_t>(AmpStatus::VC_STATUS::STARTUP)) // VS_ST_STARTUP
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
        if (sendCommandByBulk(&Commands::PowerOn))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if (sendCommandByBulk(&Commands::CheckData))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
                return true;
            }
        }
        setAltSetting(ALT_SETTING::CONTROL);
        setAltSetting(ALT_SETTING::EP6_BULK_OUT_EP8_BULK_IN);
    }
    last_error_ = "cannot power on ADC controller";
    return false;
}

bool Perun32::pImplPerun32::syncDeviceClock()
{
    if (false == sendCommandByBulk(&Commands::GetTime))
    {
        return false;
    }

    const auto now = std::chrono::high_resolution_clock::now();
    host_sync_timestamp_us_ = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();

    return true;
}

bool Perun32::pImplPerun32::setAltSetting(const ALT_SETTING alt)
{
    if (true == evaluateDeviceResponse(
            libusb_set_interface_alt_setting(device_handle_, PERUN_INTERFACE_ID, static_cast<uint8_t>(alt))))
    {
        current_alt_setting_ = alt;
        return true;
    }
    return false;
}
