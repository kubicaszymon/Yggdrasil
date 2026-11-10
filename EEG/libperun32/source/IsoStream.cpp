//
// Created by Szymon on 09/11/2026.
//

#include "../include/IsoStream.h"
#include "definitions.h"
#include <iostream>

void IsoStream::start()
{
    event_thread_ = std::thread(&IsoStream::eventLoop, this);
}

void IsoStream::stop()
{
    streaming_ = false;
    if (event_thread_.joinable())
    {
        event_thread_.join();
    } else
    {
        eventLoop();
    }

    for (auto *transfer: transfers_)
    {
        libusb_free_transfer(transfer);
    }
    transfers_.clear();
    transfer_buffers_.clear();
}

bool IsoStream::prepareAndSubmitTransfers(libusb_device_handle *handle)
{
    transfer_buffers_.assign(IsoCommunication::ISO_TRANSFERS,
                             std::vector<
                                 std::uint8_t>(IsoCommunication::ISO_PACKETS * IsoCommunication::ISO_PACKET_LEN));
    for (int i = 0; i < IsoCommunication::ISO_TRANSFERS; i++)
    {
        libusb_transfer *transfer = libusb_alloc_transfer(IsoCommunication::ISO_PACKETS);
        if (nullptr == transfer)
        {
            return false;
        }
        transfers_.push_back(transfer); // owned from now

        libusb_fill_iso_transfer(transfer, handle, ENDPOINT_2_ADDR, transfer_buffers_[i].data(),
                                 static_cast<int>(transfer_buffers_[i].size()), IsoCommunication::ISO_PACKETS,
                                 &IsoStream::onIsoTransfer, this, IsoCommunication::ISO_TIMEOUT);
        libusb_set_iso_packet_lengths(transfer, IsoCommunication::ISO_PACKET_LEN);

        if (0 != libusb_submit_transfer(transfer))
        {
            return false;
        }
        ++transfers_in_flight_;
    }
    return true;
}

bool IsoStream::read(std::vector<uint8_t> &out, const std::chrono::milliseconds timeout)
{
    std::unique_lock lock(queue_mutex_);
    if (false == queue_cv_.wait_for(lock, timeout, [this]
    {
        return false == queue_.empty();
    }))
    {
        return false;
    }
    out = std::move(queue_.front());
    queue_.pop_front();
    return true;
}

void IsoStream::reset()
{
    // Reset all states and vars before
    details_.bytes_received_ = 0;
    details_.dropped_chunks_ = 0;
    details_.iso_packet_errors_ = 0;
    streaming_ = true;
    queue_.clear();
}

IsoCommunication::Details IsoStream::stats() const
{
    return details_;
}

void IsoStream::onIsoTransfer(libusb_transfer *transfer)
{
    static_cast<IsoStream *>(transfer->user_data)->handleIsoTransfer(transfer);
}

void IsoStream::handleIsoTransfer(libusb_transfer *transfer)
{
    std::cerr << "cb status=" << transfer->status
              << " len=" << transfer->actual_length << '\n';
    if (LIBUSB_TRANSFER_COMPLETED == transfer->status)
    {
        std::vector<std::uint8_t> chunk;
        chunk.reserve(transfer->actual_length);
        for (int i = 0; i < transfer->num_iso_packets; i++)
        {
            const auto &packet = transfer->iso_packet_desc[i];
            if (LIBUSB_TRANSFER_COMPLETED != packet.status)
            {
                ++details_.iso_packet_errors_;
                continue;
            }
            if (0 == packet.actual_length)
            {
                continue;
            }
            const std::uint8_t *data = libusb_get_iso_packet_buffer_simple(transfer, i);
            chunk.insert(chunk.end(), data, data + packet.actual_length);
        }
        if (false == chunk.empty())
        {
            details_.bytes_received_ += chunk.size();
            {
                std::lock_guard lock(queue_mutex_);
                if (queue_.size() >= IsoCommunication::MAX_QUEUED_CHUNKS)
                {
                    queue_.pop_front();
                    ++details_.dropped_chunks_;
                }
                queue_.push_back(std::move(chunk));
            }
            queue_cv_.notify_one();
        }
    }

    // if we still streaming, resubmit the same transfer to get continuous flow of data
    //if (const bool dead = LIBUSB_TRANSFER_CANCELLED == transfer->status || LIBUSB_TRANSFER_NO_DEVICE == transfer->status
     //   ; streaming_ && false == dead && 0 == libusb_submit_transfer(transfer))
    //{
     //   return;
   // }
    //--transfers_in_flight_;
    const bool dead = LIBUSB_TRANSFER_CANCELLED == transfer->status
               || LIBUSB_TRANSFER_NO_DEVICE == transfer->status;
    if (streaming_ && false == dead)
    {
        const int rc = libusb_submit_transfer(transfer);
        if (0 == rc) { return; }
        std::cerr << "resubmit failed: " << libusb_error_name(rc) << '\n';
    }
    --transfers_in_flight_;
}

void IsoStream::eventLoop() const
{
    timeval tv{0, 100'000};
    while (transfers_in_flight_ > 0)
    {
        if (false == streaming_)
        {
            // not found / already done / already canceled - we need to also check it here
            for (auto *transfer: transfers_)
            {
                libusb_cancel_transfer(transfer);
            }
        }
        // sleeps until OS reports that some transfer finished or until timeout
        libusb_handle_events_timeout_completed(nullptr, &tv, nullptr);
    }
}
