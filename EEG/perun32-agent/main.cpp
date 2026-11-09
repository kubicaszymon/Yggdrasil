#include "../libperun32/api/libperun32.h"

static constexpr uint16_t kVID = 0x16d0;
static constexpr uint16_t kPID = 0x0fab;

int main()
{
    Perun32 lib(kVID, kPID);

    //for (int i = 0; i < 20; i++)
    {
        if (true == lib.connect())
        {
            auto result = lib.setSamplingRate(Perun32::SamplingRate::Hz500);
        }

        lib.disconnect();
    }


    return 0;
}
