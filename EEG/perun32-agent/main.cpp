#include "../libperun32/api/libperun32.h"

static constexpr uint16_t kVID = 0x16d0;
static constexpr uint16_t kPID = 0x0fab;

using namespace std::chrono_literals;

int main()
{
    Perun32 lib(kVID, kPID);
    auto result = lib.connect();
    result = lib.setSamplingRate(Perun32::SamplingRate::Hz500, false);
    result = lib.startAcquisition();

    if (true == result)
    {
        size_t bytes{0};
        std::vector<std::uint8_t> chunk{};
        const auto end = std::chrono::steady_clock::now() + 10s;
        while (std::chrono::steady_clock::now() < end)
        {
            if (lib.readRaw(chunk, 200ms))
            {
                bytes += chunk.size();
            }
        }
        result = lib.stopAcquisition();
        int a = 3;
    }

    return 0;
}
