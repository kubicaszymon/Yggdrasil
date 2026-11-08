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









    bool result{false};

    libusb_init_context(nullptr, nullptr, 0);
    libusb_device **device_list{};
    libusb_device *device{nullptr};

    state_ = STATE::ENUMERATE_DEVICES;
    while (STATE::IDLE != state_)
    {
        switch (state_)
        {
            case STATE::ENUMERATE_DEVICES:
                if (const ssize_t devices_count = libusb_get_device_list(nullptr, &device_list);
                    0 < devices_count)
                {
                    libusb_device_descriptor device_descriptor{};
                    for (int i = 0; i < devices_count; i++)
                    {
                        // find the correct one
                        libusb_get_device_descriptor(device_list[i], &device_descriptor);
                        if (amp_details.vid_ == device_descriptor.idVendor && amp_details.pid_ == device_descriptor.
                            idProduct)
                        {
                            // save the model (A,B,C,D) - important for channel count and electrode map
                            amp_details.device_model_ = device_descriptor.bcdDevice;
                            device = device_list[i];

                            state_ = STATE::OPEN;
                            break;
                        }
                    }
                } else
                {
                    state_ = STATE::ERROR;
                }
                break;
            case STATE::OPEN:
                if (true == evaluateDeviceResponse(libusb_open(device, &device_handle_)))
                {
                    state_ = STATE::INITIAL_CONFIG_START;
                } else
                {
                    state_ = STATE::ERROR;
                }
                break;
            case STATE::INITIAL_CONFIG_START:
                initialConfigureDevice();
                state_ = STATE::FINALIZE;
                break;
            case STATE::FINALIZE:
                libusb_free_device_list(device_list, 1);
                result = true;
                break;
            default:
                state_ = STATE::UNKNOWN;
                return false;
        }
    }
    return result;
}

void Perun32::pImplPerun32::initialConfigureDevice()
{
    bool adc_powered{false};
    while (STATE::IDLE != state_)
    {
        switch (state_)
        {
            case STATE::INITIAL_CONFIG_START:
                state_ = STATE::INITIAL_CONFIG_DETACH_KERNEL_DRIVER;
                break;
            case STATE::INITIAL_CONFIG_DETACH_KERNEL_DRIVER:
                if (true == evaluateDeviceResponse(libusb_detach_kernel_driver(device_handle_, PERUN_INTERFACE_ID)))
                {
                    state_ = STATE::INITIAL_CONFIG_CLAIM_INTERFACE;
                } else
                {
                    state_ = STATE::ERROR;
                }
                break;
            case STATE::INITIAL_CONFIG_CLAIM_INTERFACE:
                if (true == evaluateDeviceResponse(libusb_claim_interface(device_handle_, PERUN_INTERFACE_ID)))
                {
                    libusb_set_interface_alt_setting(device_handle_, PERUN_INTERFACE_ID,
                                                     static_cast<int>(ALT_SETTING::CONTROL));
                    state_ = STATE::INITIAL_CONFIG_CHECK_ADC_READY;
                } else
                {
                    state_ = STATE::ERROR;
                }
                break;
            case STATE::INITIAL_CONFIG_CHECK_ADC_READY:
                if (true == checkIfAmplifierIsReady())
                {
                    // @TODO maybe just alt 3/4 with iso open? check
                    libusb_set_interface_alt_setting(device_handle_, PERUN_INTERFACE_ID,
                                                     static_cast<int>(ALT_SETTING::EP6_BULK_OUT_EP8_BULK_IN));;
                    state_ = STATE::INITIAL_CONFIG_CHECK_DATA;
                } else
                {
                    state_ = STATE::ERROR;
                }
                break;
            case STATE::INITIAL_CONFIG_CHECK_DATA:
                // initial send check data command ------ ???
                if (true == sendCommandByBulk(&Commands::CheckData))
                {
                    if (true == adc_powered)
                    {
                        state_ = STATE::INITIAL_CONFIG_INIT_DATA;
                    } else
                    {
                        state_ = STATE::INITIAL_CONFIG_POWER_ADC;
                    }
                } else
                {
                    state_ = STATE::ERROR;
                }
                break;
            case STATE::INITIAL_CONFIG_POWER_ADC:
                if (true == powerOnADCs())
                {
                    adc_powered = true;
                    state_ = STATE::INITIAL_CONFIG_CHECK_DATA;
                } else
                {
                    state_ = STATE::ERROR;
                }
                break;
            case STATE::INITIAL_CONFIG_INIT_DATA:
                if (true == sendCommandByBulk(&Commands::InitData))
                {
                    state_ = STATE::INITIAL_CONFIG_FINALIZE;
                } else
                {
                    state_ = STATE::ERROR;
                }
                break;
            case STATE::INITIAL_CONFIG_FINALIZE:
                if (true == sendCommandByBulk(&Commands::Stop))
                {
                    state_ = STATE::IDLE;
                } else
                {
                    state_ = STATE::ERROR;
                }
                break;
            case STATE::ERROR:
                //@TODO
                break;
            default:
                state_ = STATE::UNKNOWN;
        }
    }
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
    }

    // MISMATCHES allowed value according to Braintech's driver
    return mismatches <= 10;
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
        // send power on command to activate ADC power rails
        if (true == sendCommandByBulk(&Commands::PowerOn))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            // verify stability by check command
            if (true == sendCommandByBulk(&Commands::CheckData))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
                return true;
            }
        }
    }
    last_error_ = "cannot power on ADC controller";
    return false;
}

bool Perun32::pImplPerun32::configureSampling(const Perun32::SamplingRate sampling_rate, const bool impedance_mode)
{
    // reset adc internal registers
    if (false == sendCommandByBulk(&Commands::InitData))
    {
        return false;
    }

    // stop any existing adc read loop
    if (false == sendCommandByBulk(&Commands::Stop))
    {
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
            last_error_ = "Undefined sampling rate";
            return false;
    }

    // send sampling configuration spi block
    if (false == sendCommandByBulk(&sampling))
    {
        return false;
    }

    communication_details.sampling_rate = sampling_rate;
    communication_details.impedance_mode = impedance_mode;
    return true;
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
