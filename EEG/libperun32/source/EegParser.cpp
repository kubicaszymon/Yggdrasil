//
// Created by Szymon on 11/11/2026.
//

#include "../include/EegParser.h"
#include "definitions.h"

void EegParser::push(const std::vector<std::uint8_t> &chunk, std::vector<Perun32::Sample> &out)
{
    buffer_.insert(buffer_.end(), chunk.begin(), chunk.end());
    std::size_t pos{0};
    while (buffer_.size() - pos >= IsoCommunication::FRAME_SIZE)
    {
        out.push_back(parseFrame(buffer_.data() + pos));
        pos += IsoCommunication::FRAME_SIZE;
    }
    buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(pos));
}

Perun32::Sample EegParser::parseFrame(const std::uint8_t *data)
{
    Perun32::Sample sample;

    sample.device_timestamp_us = data[0] | (data[1] << 8) | (data[2] << 16);
    sample.events = static_cast<std::uint8_t>(data[3] ^ 0x0F);
    data += 4;

    std::array<std::int32_t, 32> raw{};
    bool status_ok = true;
    for (int adc = 0; adc < 4; adc++)
    {
        if ((get24Bits(data) >> 20) != 0xC)
        {
            status_ok = false;
        }

        data += 3;
        for (int ch = 0; ch < 8; ch++)
        {
            raw[adc * 8 + ch] = static_cast<std::int32_t>(get24Bits(data) ^ 0x800000u) - 0x800000;
            data += 3;
        }
    }

    if (false == status_ok)
    {
        ++bad_status_frames_;
    }

    for (std::size_t i = 0; i < 32; i++)
    {
        // reorder once so for example index = 3 (T6) becomes index = 18 in sample channels vector
        sample.channels[i] = raw[EegData::layout32.raw_index[i]];
    }
    return sample;
}

std::uint32_t EegParser::get24Bits(const std::uint8_t *data)
{
    return (static_cast<std::uint32_t>(data[0]) << 16) | (static_cast<std::uint32_t>(data[1]) << 8) | data[2];
}

void EegParser::reset()
{
    buffer_.clear();
}
