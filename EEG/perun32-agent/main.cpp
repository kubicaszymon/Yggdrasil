#include "../libperun32/include/libperun32.h"

static constexpr uint16_t kVID = 0x16d0;
static constexpr uint16_t kPID = 0x0fab;

int main() {

    Perun32 lib(kVID, kPID);
    if (true == lib.connect()) {
        int a = 3;
    }

    return 0;
}