#include <iomanip>

#include "../libperun32/api/libperun32.h"
#include <lsl_cpp.h>

#include <iostream>

static constexpr uint16_t kVID = 0x16d0;
static constexpr uint16_t kPID = 0x0fab;

using namespace std::chrono_literals;

static void printSample(const Perun32::Sample &s)
{
    std::cout << "ts=" << s.timestamp
            << " ev=0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(s.events)
            << std::dec << std::setfill(' ') << '\n';

    // 24 EEG channels, 6 per line, then 8 aux
    for (std::size_t i = 0; i < s.channels.size(); i++)
    {
        std::cout << std::setw(8) << Perun32::Sample::channelIndexToCapLabel(i) << "=" << std::setw(9) << s.channels[i];
        if ((i + 1) % 6 == 0 || i + 1 == s.channels.size())
        {
            std::cout << '\n';
        }
    }
}

int main()
{
    const Perun32 lib(kVID, kPID);
    auto result = lib.connect();

    result = lib.stopAcquisition();

    result = lib.setSamplingRate(Perun32::SamplingRate::Hz500, false);
    result = lib.startAcquisition([]
    {
        return lsl::local_clock();
    });

    if (true == result)
    {
        std::vector<Perun32::Sample> samples;
        std::uint32_t prev_ts = 0;
        bool first = true;
        std::size_t total = 0;
        std::vector<std::uint8_t> chunk{};
        const auto end = std::chrono::steady_clock::now() + 10s;

        while (std::chrono::steady_clock::now() < end)
        {
            if (false == lib.readSamples(samples, 200ms))
            {
                std::cout << "Failed to read samples!\n";
                continue;
            }
            for (const auto &s: samples)
            {
                if (total < 20)
                {
                    printSample(s);
                }
                ++total;
            }
        }
        result = lib.stopAcquisition();
        std::cout << "TOTAL SAMPLES GOT: " << total << "\n";
        int a = 3;
    }

    return 0;
}
