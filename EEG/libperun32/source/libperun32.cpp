//
// Created by Szymon on 21/10/2026.
//

#include "../include/libperun32.h"
#include <libusb-1.0/libusb.h>

struct Perun32::pImplPerun32 {

    // VID and PID of perun32 amp
    static constexpr uint16_t kVID = 0x16d0;
    static constexpr uint16_t kPID = 0x0fab;

    pImplPerun32();
    ~pImplPerun32();

    private:
    libusb_device_handle *device_handle_{nullptr};
    uint16_t device_model_{0xFFFF};
};

Perun32::pImplPerun32::pImplPerun32() {
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
                if (kVID == device_descriptor.idVendor && kPID == device_descriptor.idProduct) {
                    // save the model (A,B,C,D) - important for channel count and electrode map
                    device_model_ = device_descriptor.bcdDevice;
                    device = device_list[i];
                    break;
                }
            }

            // connect to founded one
            if (nullptr != device) {
                if (const int open_status = libusb_open(device, &device_handle_);
                    LIBUSB_SUCCESS == open_status) {

                } else if (LIBUSB_ERROR_ACCESS == open_status) {
                    // @TODO
                }
                else if (LIBUSB_ERROR_NOT_FOUND == open_status) {
                    // @TODO
                } else {
                    // @TODO
                }
            }

            libusb_free_device_list(device_list,1);
        }
    } else {
        // @TODO handle somehow warnings/errors by for example getlastError functionality of somehow!
    }
}

Perun32::pImplPerun32::~pImplPerun32() {
    if (nullptr != device_handle_) {
        libusb_close(device_handle_);
    }
    libusb_exit(nullptr);
}

Perun32::Perun32()
    : pImpl(std::make_unique<Perun32::pImplPerun32>()) {
}

