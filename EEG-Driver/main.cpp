#include <iostream>
#include <libusb-1.0/libusb.h>

auto hotplug_callback(
    libusb_context *ctx,
    libusb_device *device,
    libusb_hotplug_event event,
    void *user_data
) -> int {
    std::cout<<"New device arrived!\n";
    return 0;
}

auto main() -> int {
    libusb_context *context = nullptr;
    libusb_init(&context);

    libusb_hotplug_callback_handle hotplug_callback_handle;
    libusb_hotplug_register_callback(
          context,
          LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED,
          LIBUSB_HOTPLUG_ENUMERATE,
          0x31cf, 0x5740,
          LIBUSB_HOTPLUG_MATCH_ANY,
          hotplug_callback, nullptr,
          &hotplug_callback_handle
    );

    while (true) {
        if (libusb_handle_events(context) < 0)
            break;
    }

    libusb_hotplug_deregister_callback(context, hotplug_callback_handle);
    libusb_exit(context);

    return 0;
}