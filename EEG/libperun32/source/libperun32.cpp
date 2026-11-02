//
// Created by Szymon on 21/10/2026.
//

#include "libperun32.h"
#include "libusb.h"
#include "definitions.h"

#include <iostream>
#include <thread>
#include <vector>

struct Perun32::pImplPerun32
{
    pImplPerun32(uint16_t, uint16_t);
    ~pImplPerun32();

    bool connect();
    std::string getLastError();
    AmplifierDetails details_{};

private:
    bool initialConfigureDevice();
    bool evaluateDeviceResponse(const int &response);

    struct CommandData
    {
        std::vector<uint8_t> data_out{};
        std::vector<uint8_t> data_in{};
        std::vector<uint8_t> data_in_mask{};
    };

    static CommandData getCommandDataFromCommandId(const CommandId &command_id);
    bool sendCommand(const CommandId& command_id);

    libusb_device_handle *device_handle_{nullptr};
    std::string last_error_{};
};

Perun32::pImplPerun32::pImplPerun32(const uint16_t vid, const uint16_t pid)
    : details_{.vid_ = vid, .pid_ = pid} {}

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
                if (details_.vid_ == device_descriptor.idVendor && details_.pid_ == device_descriptor.idProduct)
                {
                    // save the model (A,B,C,D) - important for channel count and electrode map
                    details_.device_model_ = device_descriptor.bcdDevice;
                    device = device_list[i];
                    break;
                }
            }

            // check if device were found
            if (nullptr != device)
            {
                const int open_status = libusb_open(device, &device_handle_);
                libusb_free_device_list(device_list, 1);

                if (true == evaluateDeviceResponse(open_status))
                {
                    return initialConfigureDevice();
                }
                return false;
            }
            last_error_ = "libusb device with provided pid and vid not found";
        }
    } else
    {
        last_error_ = "libusb error while initialize library";
        return false;
    }
    return false;
}

bool Perun32::pImplPerun32::sendCommand(const CommandId& command_id)
{
    const auto command_data = getCommandDataFromCommandId(command_id);
    int transferred{0};
    // send command to bulk OUT endpoint
    int status = libusb_bulk_transfer(
        device_handle_,
        BULK_OUT_ENDPOINT,
        const_cast<uint8_t *>(command_data.data_out.data()),
        static_cast<int>(command_data.data_out.size()),
        &transferred,
        50);

    if (false == evaluateDeviceResponse(status) || transferred != static_cast<int>(command_data.data_out.size()))
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

    if (false == evaluateDeviceResponse(status) || transferred != static_cast<int>(command_data.data_in.size()))
    {
        return false;
    }

    // masked verification
    int mismatches{0};
    for (size_t i = 0; i < command_data.data_in.size(); i++)
    {
        const uint8_t masked_response = response_buffer[i] & command_data.data_in_mask[i];
        if (const uint8_t expected_byte = command_data.data_in[i];
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
    if (int response = libusb_claim_interface(device_handle_, 0);
        true == evaluateDeviceResponse(response))
    {
        // alt0 opens endpoint0
        libusb_set_interface_alt_setting(device_handle_, 0, 0);
        std::vector<uint8_t> data(9);

        // check if device is ready
        for (int attempt = 0; attempt < VC_ST_SET_MAX_ATTEMPTS; attempt++)
        {
            response = libusb_control_transfer(
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
        last_error_ = "cannot initialize FPGA if perun amplifier (max attemps exceeded)";
    }
    return false;
}

bool Perun32::pImplPerun32::evaluateDeviceResponse(const int &response)
{
    switch (response)
    {
        case LIBUSB_SUCCESS:
            last_error_ = "Success";
        case LIBUSB_ERROR_IO:
            last_error_ = "libusb error io";
        case LIBUSB_ERROR_INVALID_PARAM:
            last_error_ = "Invalid parameter";
        case LIBUSB_ERROR_ACCESS:
            last_error_ = "Access denied (insufficient permissions)";
        case LIBUSB_ERROR_NO_DEVICE:
            last_error_ = "No such device (it may have been disconnected)";
        case LIBUSB_ERROR_NOT_FOUND:
            last_error_ = "No such device found";
        case LIBUSB_ERROR_BUSY:
            last_error_ = "Device is already in use";
        case LIBUSB_ERROR_TIMEOUT:
            last_error_ = "Timed out";
        case LIBUSB_ERROR_OVERFLOW:
            last_error_ = "Overflow";
        case LIBUSB_ERROR_PIPE:
            last_error_ = "Pipe is already in use";
        case LIBUSB_ERROR_INTERRUPTED:
            last_error_ = "System call interrupted";
        case LIBUSB_ERROR_NO_MEM:
            last_error_ = "Not enough memory";
        case LIBUSB_ERROR_NOT_SUPPORTED:
            last_error_ = "Operation not supported or unimplemented on this platform";
        case LIBUSB_ERROR_OTHER:
            last_error_ = "Other error";
        default:
            last_error_ = "Unknown response";
    }
    return response >= 0;;
}

Perun32::pImplPerun32::CommandData Perun32::pImplPerun32::getCommandDataFromCommandId(const CommandId &command_id)
{
    switch (command_id)
    {
        case CommandId::Check:
            return CommandData{
                .data_out = {0x40, 0x00, 0x30, 0x40, 0x63, 0x40},
                .data_in = {0x47, 0x00, 0x38, 0x47, 0x63, 0x47},
                .data_in_mask = {0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF}
            };
        case CommandId::PowerOn:
            return CommandData{
                .data_out = {0x40, 0x00, 0x30, 0x14, 0x00, 0x30, 0x40},
                .data_in = {0x47, 0x00, 0x38, 0x14, 0x00, 0x38, 0x47},
                .data_in_mask = {0xFF, 0xFE, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF}
            };
        case CommandId::PowerOff:
            return CommandData{
                .data_out = {0x40, 0x00, 0x30, 0x14, 0x01, 0x30, 0x40},
                .data_in = {0x47, 0x00, 0x38, 0x14, 0x01, 0x38, 0x47},
                .data_in_mask = {0xFF, 0xFE, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF}
            };
        case CommandId::Init:
            return CommandData{
                .data_out = {0x40, 0x00, 0x30, 0x11, 0x00, 0x30, 0x40},
                .data_in = {0x47, 0x00, 0x38, 0x11, 0x00, 0x38, 0x47},
                .data_in_mask = {0xFF, 0xFE, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF}
            };
        case CommandId::Stop:
            return CommandData{
                .data_out = {0x40, 0x00, 0x30, 0x12, 0x00, 0x30, 0x40},
                .data_in = {0x47, 0x00, 0x38, 0x12, 0x00, 0x38, 0x47},
                .data_in_mask = {0xFF, 0xFE, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF}
            };
        case CommandId::Start:
            return CommandData{
                .data_out = {
                    0x40, 0x17, 0x50, 0x50, 0x5A, 0x50, 0x69, 0x14, 0x66, 0x17, 0x50, 0x55, 0x50, 0x50, 0x69, 0x14, 0x40
                },
                .data_in = {
                    0x47, 0x17, 0x58, 0x58, 0x5A, 0x58, 0x69, 0x14, 0x66, 0x17, 0x58, 0x55, 0x58, 0x58, 0x69, 0x14, 0x47
                },
                .data_in_mask = {
                    0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
                }
            };
        case CommandId::GetTime:
            return CommandData{
                .data_out = {0x40, 0x00, 0x30, 0x20, 0x00, 0x30, 0x40},
                .data_in = {0x47, 0x00, 0x38, 0x20, 0x00, 0x38, 0x47},
                .data_in_mask = {0xFF, 0xFE, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF}
            };
        case CommandId::Config500Hz:
            return CommandData{
                .data_out = {0x40, 0x17, 0x50, 0x41, 0x00, 0x90, 0x06, 0x50, 0x69, 0x14, 0x40},
                .data_in = {0x47, 0x17, 0x58, 0x41, 0x00, 0x90, 0x06, 0x58, 0x69, 0x14, 0x47},
                .data_in_mask = {0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF}
            };
        case CommandId::Config1000Hz:
            return CommandData{
                .data_out = {0x40, 0x17, 0x50, 0x41, 0x00, 0x90, 0x05, 0x50, 0x69, 0x14, 0x40},
                .data_in = {0x47, 0x17, 0x58, 0x41, 0x00, 0x90, 0x05, 0x58, 0x69, 0x14, 0x47},
                .data_in_mask = {0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF}
            };
        case CommandId::Config2000Hz:
            return CommandData{
                .data_out = {0x40, 0x17, 0x50, 0x41, 0x00, 0x90, 0x04, 0x50, 0x69, 0x14, 0x40},
                .data_in = {0x47, 0x17, 0x58, 0x41, 0x00, 0x90, 0x04, 0x58, 0x69, 0x14, 0x47},
                .data_in_mask = {0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF}
            };
        case CommandId::Config4000Hz:
            return CommandData{
                .data_out = {0x40, 0x17, 0x50, 0x41, 0x00, 0x90, 0x03, 0x50, 0x69, 0x14, 0x40},
                .data_in = {0x47, 0x17, 0x58, 0x41, 0x00, 0x90, 0x03, 0x58, 0x69, 0x14, 0x47},
                .data_in_mask = {0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF}
            };
        case CommandId::Config8000Hz:
            return CommandData{
                .data_out = {0x40, 0x17, 0x50, 0x41, 0x00, 0x90, 0x02, 0x50, 0x69, 0x14, 0x40},
                .data_in = {0x47, 0x17, 0x58, 0x41, 0x00, 0x90, 0x02, 0x58, 0x69, 0x14, 0x47},
                .data_in_mask = {0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF}
            };
        case CommandId::Config16000Hz:
            return CommandData{
                .data_out = {0x40, 0x17, 0x50, 0x41, 0x00, 0x90, 0x01, 0x50, 0x69, 0x14, 0x40},
                .data_in = {0x47, 0x17, 0x58, 0x41, 0x00, 0x90, 0x01, 0x58, 0x69, 0x14, 0x47},
                .data_in_mask = {0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF}
            };
        case CommandId::ConfigImp500Hz:
            return CommandData{
                .data_out = {0x40, 0x17, 0x50, 0x41, 0x00, 0xD0, 0x06, 0x50, 0x69, 0x14, 0x40},
                .data_in = {0x47, 0x17, 0x58, 0x41, 0x00, 0xD0, 0x06, 0x58, 0x69, 0x14, 0x47},
                .data_in_mask = {0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF}
            };
        case CommandId::ConfigImp1000Hz:
            return CommandData{
                .data_out = {0x40, 0x17, 0x50, 0x41, 0x00, 0xD0, 0x05, 0x50, 0x69, 0x14, 0x40},
                .data_in = {0x47, 0x17, 0x58, 0x41, 0x00, 0xD0, 0x05, 0x58, 0x69, 0x14, 0x47},
                .data_in_mask = {0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF}
            };
        case CommandId::ConfigImp2000Hz:
            return CommandData{
                .data_out = {0x40, 0x17, 0x50, 0x41, 0x00, 0xD0, 0x04, 0x50, 0x69, 0x14, 0x40},
                .data_in = {0x47, 0x17, 0x58, 0x41, 0x00, 0xD0, 0x04, 0x58, 0x69, 0x14, 0x47},
                .data_in_mask = {0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF}
            };
        case CommandId::ConfigImp4000Hz:
            return CommandData{
                .data_out = {0x40, 0x17, 0x50, 0x41, 0x00, 0xD0, 0x03, 0x50, 0x69, 0x14, 0x40},
                .data_in = {0x47, 0x17, 0x58, 0x41, 0x00, 0xD0, 0x03, 0x58, 0x69, 0x14, 0x47},
                .data_in_mask = {0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF}
            };
        case CommandId::ConfigImp8000Hz:
            return CommandData{
                .data_out = {0x40, 0x17, 0x50, 0x41, 0x00, 0xD0, 0x02, 0x50, 0x69, 0x14, 0x40},
                .data_in = {0x47, 0x17, 0x58, 0x41, 0x00, 0xD0, 0x02, 0x58, 0x69, 0x14, 0x47},
                .data_in_mask = {0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF}
            };
        case CommandId::ConfigImp16000Hz:
            return CommandData{
                .data_out = {0x40, 0x17, 0x50, 0x41, 0x00, 0xD0, 0x01, 0x50, 0x69, 0x14, 0x40},
                .data_in = {0x47, 0x17, 0x58, 0x41, 0x00, 0xD0, 0x01, 0x58, 0x69, 0x14, 0x47},
                .data_in_mask = {0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF}
            };
        default:
            return CommandData{};
    }
}

/*
 ***************************** INTERFACE *****************************
*/

Perun32::Perun32(const uint16_t vid, const uint16_t pid)
    : pImpl(std::make_unique<Perun32::pImplPerun32>(vid, pid)) {}

Perun32::AmplifierDetails Perun32::getAmplifierDetails() const noexcept
{
    return pImpl->details_;
}

bool Perun32::connect() const noexcept
{
    return pImpl->connect();
}

std::string Perun32::getLastError() const noexcept
{
    return pImpl->getLastError();
}

/*
* why here? because if i would not put it there, the compiler would generate default destructor in the class declaration (.h file),
* and such destructor will try to delete pImpl unique_ptr and because i made it as incomplete struct declaration (PImpl),
* the destructor in .h file would not determine the overall size of object thus it needs to be done here where the PImpl type is completed.
*/
Perun32::~Perun32() = default;
