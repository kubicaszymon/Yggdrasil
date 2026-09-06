#include <iostream>
#include <libusb-1.0/libusb.h>

inline constexpr uint16_t kVID = 0x16d0;
inline constexpr uint16_t kPID = 0x0fab;

static void Init()
{
    libusb_device** device_list{};
    if (const ssize_t devices_count = libusb_get_device_list(nullptr, &device_list);
        devices_count > 0) {
        std::cout << "libusb found " << devices_count << " devices\n";

        int found_index{-1};
        for (int i = 0; i < devices_count; ++i) {
            libusb_device_descriptor device_descriptor{};
            libusb_get_device_descriptor(device_list[i], &device_descriptor);
            std::cout << "device: " << i << " VID: " <<  std::hex << device_descriptor.idVendor << " PID: " << std::hex << device_descriptor.idProduct << "\n";

            if (device_descriptor.idVendor == kVID && device_descriptor.idProduct == kPID) {
                std::cout << "found perun32\n";
                found_index = i;
                break;
            }
        }

        if (-1 != found_index) {
            libusb_device_handle* device_handle{};
            if (const int open_status = libusb_open(device_list[found_index], &device_handle);
                LIBUSB_SUCCESS == open_status) {
                std::cout << "device opened\n";
                }
            else if (LIBUSB_ERROR_ACCESS == open_status) {
                std::cout << "can't open device, insufficient permissions\n";
            }
            else if (LIBUSB_ERROR_NOT_SUPPORTED == open_status) {
                std::cout << "can't open device, device not connected\n";
            }
            else {
                std::cout << "can't open device, other failure\n";
            }

            libusb_free_device_list(device_list,1);

            //--------------

            libusb_close(device_handle);
        } else {
            std::cout << "libusb failed to find device\n";
        }
    } else {
        std::cout << "no devices found.\n";
    }
}

int main() {
    const libusb_version* version = libusb_get_version();
    std::cout << "libusb version: " << version->major << "." << version->minor << "." << version->micro << "." << version->nano << "\n";

    if (const int init_status = libusb_init_context(nullptr, nullptr, 0);
        init_status == LIBUSB_SUCCESS) {
        std::cout << "libusb initialized\n";
        Init();
    } else {
        std::cout << "libusb init failed: " << libusb_error_name(init_status) << "\n";
    }

    libusb_exit(nullptr);
    return 0;
}