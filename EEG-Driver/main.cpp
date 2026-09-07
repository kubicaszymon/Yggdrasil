#include <iostream>
#include <vector>
#include <bitset>
#include <libusb-1.0/libusb.h>

inline constexpr uint16_t kVID = 0x16d0;
inline constexpr uint16_t kPID = 0x0fab;

static void GetConfiguration(libusb_device* device, const size_t num_configurations) {
    std::cout << "\ndevice has " << std::dec << num_configurations << " configuration descriptors\n";
    const std::vector<libusb_config_descriptor*> configuration_descriptors(num_configurations, {});
    for (int i = 0; i < num_configurations; ++i) {
        libusb_config_descriptor* configuration_descriptor = configuration_descriptors.at(i);
        if (const int configuration_status = libusb_get_config_descriptor(device, i, &configuration_descriptor);
            LIBUSB_SUCCESS == configuration_status) {
            std::cout << "configuration descriptor number " << std::dec << i << " found \n";
            std::cout << "configuration has " << std::dec << static_cast<unsigned>(configuration_descriptor->bNumInterfaces) << " interfaces (altsettings)\n";
            for (int j = 0; j < configuration_descriptor->bNumInterfaces; j++) {
                const libusb_interface* interface = &configuration_descriptor->interface[j];
                std::cout << "   interface (altsetting) number " << std::dec << j << " found \n";
                std::cout << "   interface (altsetting) has " << std::dec << interface->num_altsetting << " interface descriptors\n";
                for (int k = 0; k < interface->num_altsetting; ++k) {
                    const libusb_interface_descriptor* interface_descriptor = &interface->altsetting[k];
                    std::cout << "      interface descriptor number " << std::dec << k << " found \n";
                    std::cout << "      interface descriptor has " << std::hex << static_cast<unsigned>(interface_descriptor->bNumEndpoints) << " endpoints\n";
                    for (int m = 0; m < interface_descriptor->bNumEndpoints; ++m) {
                        const libusb_endpoint_descriptor* endpoint_descriptor = &interface_descriptor->endpoint[m];
                        std::cout << "         endpoint descriptor number " << std::dec << m << " found \n";
                        std::cout << "         endpoint address " << std::hex <<static_cast<unsigned>(endpoint_descriptor->bEndpointAddress) << "\n";
                        std::cout << "         endpoint max packet size " << std::hex << endpoint_descriptor->wMaxPacketSize << "\n";
                        std::cout << "         endpoint polling interval " << std::hex << static_cast<unsigned>(endpoint_descriptor->bInterval) << "\n";
                        std::cout << "         endpoint attribute "  << std::bitset<8>(endpoint_descriptor->bmAttributes) << "\n";
                    }
                }
            }
        } else if ( configuration_status == LIBUSB_ERROR_NOT_FOUND) {
            std::cout << "no configuration found for conf: " << i << "\n";
        } else {
            std::cout << "failed to get configuration descriptor for conf: " << i << "\n";
        }

        for (libusb_config_descriptor* desc : configuration_descriptors) {
            libusb_free_config_descriptor(desc);
        }
    }
    //
}
static void Init()
{
    libusb_device** device_list{};
    if (const ssize_t devices_count = libusb_get_device_list(nullptr, &device_list);
        devices_count > 0) {
        std::cout << "libusb found " << devices_count << " devices\n";

        libusb_device* device{nullptr};
        libusb_device_descriptor device_descriptor{};
        for (int i = 0; i < devices_count; ++i) {
            libusb_get_device_descriptor(device_list[i], &device_descriptor);
            std::cout << "device: " << i << " VID: " <<  std::showbase << std::hex << device_descriptor.idVendor << " PID: " << std::hex << device_descriptor.idProduct << "\n";

            if (kVID == device_descriptor.idVendor && kPID == device_descriptor.idProduct) {
                std::cout << "found perun32\n";
                device = device_list[i];
                break;
            }
        }

        if (nullptr != device) {
            libusb_device_handle* device_handle{};
            if (const int open_status = libusb_open(device, &device_handle);
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
            GetConfiguration(device, device_descriptor.bNumConfigurations);
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