//
// Created by Szymon on 11/11/2026.
//

#ifndef libperun32_EEGPARSER_H
#define libperun32_EEGPARSER_H

#include <cstdint>
#include <vector>

#include "libperun32.h"

class EegParser
{
public:
    EegParser() = default;
    EegParser(const EegParser &) = delete;

    void push(const std::vector<std::uint8_t> &chunk, std::vector<Perun32::Sample> &out);

private:
    Perun32::Sample parseFrame(const std::uint8_t *data);
    static std::uint32_t get24Bits(const std::uint8_t *data);

    void reset();

    std::vector<std::uint8_t> buffer_{};
    std::uint64_t bad_status_frames_{0}; // frames whose ADC status word doesn't start with 0xC
};


#endif // libperun32_EEGPARSER_H
