//
// Created by Szymon on 09/11/2026.
//

#ifndef libperun32_ISOSTREAM_H
#define libperun32_ISOSTREAM_H

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "definitions.h"
#include "libusb.h"

class IsoStream
{
public:
    IsoStream() = default;
    IsoStream(const IsoStream &) = delete;

    void start();
    void stop();

    bool prepareAndSubmitTransfers(libusb_device_handle *handle);

    bool read(std::vector<uint8_t> &out, std::chrono::milliseconds timeout);

    void reset();
    [[nodiscard]] IsoCommunication::Details stats() const;

private:
    static void LIBUSB_CALL onIsoTransfer(libusb_transfer *transfer);
    void handleIsoTransfer(libusb_transfer *transfer);

    void eventLoop() const;

    IsoCommunication::Details details_{};

    std::atomic<int> transfers_in_flight_{0};
    std::vector<libusb_transfer *> transfers_{};
    std::vector<std::vector<std::uint8_t> > transfer_buffers_{};

    std::atomic<bool> streaming_{false};
    std::thread event_thread_{};

    std::mutex queue_mutex_{};
    std::condition_variable queue_cv_{};
    std::deque<std::vector<std::uint8_t> > queue_{};
};


#endif // libperun32_ISOSTREAM_H
