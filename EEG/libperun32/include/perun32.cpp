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
    if (State::DISCONNECTED != state_)
    {
        last_error_ = "already connected";
        return false;
    }

    auto step{InitStep::INIT_LIBUSB};
    auto next = [&step](const bool ok, const InitStep on_ok)
    {
        step = ok ? on_ok : InitStep::FAILED;
    };

    while (InitStep::DONE != step && InitStep::FAILED != step)
    {
        switch (step)
        {
            case InitStep::INIT_LIBUSB:
                next(evaluateDeviceResponse(libusb_init_context(nullptr, nullptr, 0)), InitStep::OPEN_DEVICE);
                break;

            case InitStep::OPEN_DEVICE:
                device_handle_ = libusb_open_device_with_vid_pid(nullptr, amp_details.vid_, amp_details.pid_);
                if (nullptr == device_handle_)
                {
                    last_error_ = "amplifier not found or cannot be opened";
                    step = InitStep::FAILED;
                    break;
                }
                {
                    // model (A,B,C,D) from bcdDevice
                    libusb_device_descriptor descriptor{};
                    libusb_get_device_descriptor(libusb_get_device(device_handle_), &descriptor);
                    amp_details.device_model_ = descriptor.bcdDevice;
                }
                step = InitStep::PREPARE_INTERFACE;
                break;

            case InitStep::PREPARE_INTERFACE:
                libusb_set_auto_detach_kernel_driver(device_handle_, 1);
                next(evaluateDeviceResponse(libusb_claim_interface(device_handle_, PERUN_INTERFACE_ID))
                     && setAltSetting(AltSetting::CONTROL),
                     InitStep::WAIT_READY);
                break;

            case InitStep::WAIT_READY:
                next(setAltSetting(AltSetting::CONTROL) && checkIfAmplifierIsReady(), InitStep::CHECK_LINK);
                break;

            case InitStep::CHECK_LINK:
                next(setAltSetting(AltSetting::EP6_BULK_OUT_EP8_BULK_IN) && sendCommandByBulk(&Commands::CheckData),
                     InitStep::POWER_ON);
                break;

            case InitStep::POWER_ON:
                next(powerOnADCs(), InitStep::INIT_ADC);
                break;

            case InitStep::INIT_ADC:
                next(sendCommandByBulk(&Commands::InitData), InitStep::STOP_ADC);
                break;

            case InitStep::STOP_ADC:
                next(sendCommandByBulk(&Commands::Stop), InitStep::DONE);
                break;

            default:
                last_error_ = "invalid connect step";
                step = InitStep::FAILED;
                break;
        }
    }
    if (InitStep::FAILED == step)
    {
        disconnect(); // the only cleanup place; keeps last_error_
        return false;
    }
    state_ = State::IDLE;
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
    state_ = State::DISCONNECTED;
}

bool Perun32::pImplPerun32::readRaw(std::vector<uint8_t> &out, std::chrono::milliseconds timeout)
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

bool Perun32::pImplPerun32::start()
{
    // Idle state of course required
    if (State::IDLE != state_)
    {
        last_error_ = "state not idle";
        return false;
    }

    // Sampling rate needs to be set before
    if (SamplingRate::Undefined == communication_details.sampling_rate)
    {
        last_error_ = "undefined sampling rate";
        return false;
    }

    // Start acquisition
    if (false == sendCommandByBulk(&Commands::Start))
    {
        return false;
    }

    // For 16khz we need to change for iso endpoint with 2 microframes
    const auto iso_alt_setting = SamplingRate::Hz16000 == communication_details.sampling_rate ? AltSetting::EP2_ISO2_IN_EP6_BULK_OUT_EP8_BULK_IN : AltSetting::EP2_ISO1_IN_EP6_BULK_OUT_EP8_BULK_IN;
    auto ok = setAltSetting(iso_alt_setting);

    if (true == ok)
    {
        // Reset all states and vars before
        bytes_received_ = 0;
        dropped_chunks_ = 0;
        iso_packet_errors_ = 0;
        streaming_ = true;
        ok = submitTransfers();
    }

    if (false == ok)
    {
        abortStreaming();
        return false;
    }

    event_thread_ = std::thread(&pImplPerun32::eventLoop, this);
    state_ = State::RUNNING;
    return true;
}

bool Perun32::pImplPerun32::stop()
{
    if (State::RUNNING != state_)
    {
        last_error_ = "state not running";
        return false;
    }

    abortStreaming();
    state_ = State::IDLE;
    return true;
}

bool Perun32::pImplPerun32::setSamplingRate(const SamplingRate sampling_rate, const bool impedance_mode)
{
    if (State::IDLE != state_)
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

bool Perun32::pImplPerun32::submitTransfers()
{
    transfer_buffers_.assign(ISO_TRANSFERS, std::vector<std::uint8_t>(ISO_PACKETS * ISO_PACKET_LEN));
    for (int i = 0; i < ISO_TRANSFERS; i++)
    {
        libusb_transfer* transfer = libusb_alloc_transfer(ISO_PACKETS);
        if (nullptr == transfer)
        {
            last_error_ = "libusb_alloc_transfer failed";
            return false;
        }
        transfers_.push_back(transfer); // owned from now

        libusb_fill_iso_transfer(transfer, device_handle_, ENDPOINT_2_ADDR, transfer_buffers_[i].data(), static_cast<int>(transfer_buffers_[i].size()), ISO_PACKETS,
            &pImplPerun32::onIsoTransfer, this, ISO_TIMEOUT);
        libusb_set_iso_packet_lengths(transfer, ISO_PACKET_LEN);

        if (false == evaluateDeviceResponse(libusb_submit_transfer(transfer)))
        {
            return false;
        }
        ++transfers_in_flight_;
    }
    return true;
}

void Perun32::pImplPerun32::abortStreaming()
{
    streaming_ = false;
    for (auto* transfer : transfers_)
    {
        libusb_cancel_transfer(transfer);
    }

    if (event_thread_.joinable())
    {
        event_thread_.join();
    }
    else
    {
        eventLoop();
    }

    for (auto *transfer : transfers_)
    {
        libusb_free_transfer(transfer);
    }
    transfers_.clear();
    transfer_buffers_.clear();

    // back to bulk settings
    setAltSetting(AltSetting::EP6_BULK_OUT_EP8_BULK_IN);
    sendCommandByBulk(&Commands::Stop);
}

void Perun32::pImplPerun32::eventLoop()
{
    timeval tv{0, 100'000};
    while (transfers_in_flight_ > 0)
    {
        libusb_handle_events_timeout_completed(nullptr, &tv, nullptr);
    }
}

void LIBUSB_CALL Perun32::pImplPerun32::onIsoTransfer(libusb_transfer *transfer)
{
    static_cast<pImplPerun32*>(transfer->user_data)->handleIsoTransfer(transfer);
}

void Perun32::pImplPerun32::handleIsoTransfer(libusb_transfer *transfer)
{
    if (LIBUSB_TRANSFER_COMPLETED == transfer->status)
    {
        std::vector<std::uint8_t> chunk;
        chunk.reserve(transfer->actual_length);
        for (int i = 0; i < transfer->num_iso_packets; i++)
        {
            const auto& pkt = transfer->iso_packet_desc[i];
            if (LIBUSB_TRANSFER_COMPLETED != pkt.status)
            {
                ++iso_packet_errors_;
                continue;
            }
            if (0 == pkt.actual_length)
            {
                continue;
            }
            const std::uint8_t* src = libusb_get_iso_packet_buffer_simple(transfer, i);
            chunk.insert(chunk.end(), src, src + pkt.actual_length);
        }
        if (false == chunk.empty())
        {
            bytes_received_ += chunk.size();
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                if (queue_.size() >= MAX_QUEUED_CHUNKS)
                {
                    queue_.pop_front();
                    ++dropped_chunks_;
                }
                queue_.push_back(std::move(chunk));
            }
            queue_cv_.notify_one();
        }
    }
    const bool dead = LIBUSB_TRANSFER_CANCELLED == transfer->status || LIBUSB_TRANSFER_NO_DEVICE == transfer->status;
    if (streaming_ && false == dead && 0 == libusb_submit_transfer(transfer))
    {
        return;
    }
    --transfers_in_flight_;
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
        setAltSetting(AltSetting::CONTROL);
        setAltSetting(AltSetting::EP6_BULK_OUT_EP8_BULK_IN);
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

bool Perun32::pImplPerun32::setAltSetting(const AltSetting alt)
{
    if (true == evaluateDeviceResponse(
            libusb_set_interface_alt_setting(device_handle_, PERUN_INTERFACE_ID, static_cast<uint8_t>(alt))))
    {
        current_alt_setting_ = alt;
        return true;
    }
    return false;
}
