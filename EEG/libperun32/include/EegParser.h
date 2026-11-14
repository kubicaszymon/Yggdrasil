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

    // Call before every acquisition. amp0_us - amp counter and pi0_s - pi clock, both read at the same moment
    void start(std::uint32_t amp0_us, double pi0_s);

    // In - raw sample, Out - Sample struct
    void push(const std::vector<std::uint8_t> &chunk, std::vector<Perun32::Sample> &out);

private:
    Perun32::Sample parseFrame(const std::uint8_t *data);
    static std::uint32_t get24Bits(const std::uint8_t *data);

    void reset();

    std::vector<std::uint8_t> buffer_{};
    double pi0_s_{0.0};
    std::uint32_t last_amp_us_{0}; // last 24 bit amp counter reading
    std::uint64_t elapsed_us_{0}; // amp time since amp0, never wraps

    std::uint64_t bad_status_frames_{0}; // frames whose ADC status word doesn't start with 0xC
};


#endif // libperun32_EEGPARSER_H
