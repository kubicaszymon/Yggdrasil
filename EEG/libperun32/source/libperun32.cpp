//
// Created by Szymon on 21/10/2026.
//

#include "../include/libperun32.h"
#include <libusb-1.0/libusb.h>

#include <iostream>
#include <thread>
#include <vector>

struct Perun32::pImplPerun32 {
    pImplPerun32(uint16_t, uint16_t);

    ~pImplPerun32();

    bool connect();

    AmplifierDetails details_{};

private:
    libusb_device_handle *device_handle_{nullptr};
};

Perun32::pImplPerun32::pImplPerun32(const uint16_t vid, const uint16_t pid)
    : details_{.vid_ = vid, .pid_ = pid} {
}

Perun32::pImplPerun32::~pImplPerun32() {
    if (nullptr != device_handle_) {
        libusb_close(device_handle_);
    }
    libusb_exit(nullptr);
}

bool Perun32::pImplPerun32::connect() {
    bool success{false};
    if (const int init_status = libusb_init_context(nullptr, nullptr, 0);
        LIBUSB_SUCCESS == init_status) {
        libusb_device **device_list{};

        // enumerate devices
        if (const ssize_t devices_count = libusb_get_device_list(nullptr, &device_list);
            0 < devices_count) {
            libusb_device *device{nullptr};
            libusb_device_descriptor device_descriptor{};
            for (int i = 0; i < devices_count; i++) {
                libusb_get_device_descriptor(device_list[i], &device_descriptor);
                if (details_.vid_ == device_descriptor.idVendor && details_.pid_ == device_descriptor.idProduct) {
                    // save the model (A,B,C,D) - important for channel count and electrode map
                    details_.device_model_ = device_descriptor.bcdDevice;
                    device = device_list[i];
                    break;
                }
            }

            // connect to founded one
            if (nullptr != device) {
                if (const int open_status = libusb_open(device, &device_handle_);
                    LIBUSB_SUCCESS == open_status) {
                    libusb_detach_kernel_driver(device_handle_, 0);
                    if (LIBUSB_SUCCESS == libusb_claim_interface(device_handle_, 0)) {
                        libusb_set_interface_alt_setting(device_handle_, 0, 0);
                        std::vector<uint8_t> data(9);
                        while (1 < libusb_control_transfer(
                            device_handle_,
                            0xC0, // bit 7 - device-to-host, bit 6-5 - vendor specific request, bit 4-0 - usb device
                            0xB0, // perun32's own specific command for report vendor status,
                            0,
                            0,
                            data.data(),
                            data.size(),
                            50)) {
                            if (data.at(0) == 0x10) {
                                if (data.at(1) == 0xA5) { // VC_ST_READY
                                    libusb_set_interface_alt_setting(device_handle_, 0, 1);
                                    success = true;
                                    break;
                                }
                                if (data.at(1) == 0x51) { // VS_ST_STARTUP
                                    std::this_thread::sleep_for(std::chrono::milliseconds(300));
                                    continue;
                                }
                                success = false;
                                break;
                            }
                        }
                    } else {
                        // @TODO
                        success = false;
                    }
                } else if (LIBUSB_ERROR_ACCESS == open_status) {
                    // @TODO
                    success = false;
                } else if (LIBUSB_ERROR_NOT_FOUND == open_status) {
                    // @TODO
                    success = false;
                } else {
                    // @TODO
                    success = false;
                }
            }

            libusb_free_device_list(device_list, 1);
        }
    } else {
        // @TODO handle somehow warnings/errors by for example getlastError functionality of somehow!
        success = false;
    }
    return success;
}

/*
 ***************************** INTERFACE *****************************
*/

Perun32::Perun32(const uint16_t vid, const uint16_t pid)
    : pImpl(std::make_unique<Perun32::pImplPerun32>(vid, pid)) {
}

Perun32::AmplifierDetails Perun32::getAmplifierDetails() const {
    return pImpl->details_;
}

bool Perun32::connect() const {
    return pImpl->connect();
}

/*
* why here? because if i would not put it there, the compiler would generate default destructor in the class declaration (.h file),
* and such destructor will try to delete pImpl unique_ptr and because i made it as incomplete struct declaration (PImpl),
* the destructor in .h file would not determine the overall size of object thus it needs to be done here where the PImpl type is completed.
*/
Perun32::~Perun32() = default;
