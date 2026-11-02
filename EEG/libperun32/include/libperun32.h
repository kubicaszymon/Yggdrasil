//
// Created by Szymon on 21/10/2026.
//

#ifndef libperun32_API
#define libperun32_API
#include <cstdint>
#include <memory>

class Perun32
{
public:
    Perun32(uint16_t vid, uint16_t pid);
    virtual ~Perun32();

    struct AmplifierDetails
    {
        const uint16_t vid_{};
        const uint16_t pid_{};
        uint16_t device_model_{0xFFFF};
    };

    enum class CommandId
    {
        Check,
        PowerOn,
        PowerOff,
        Init,
        Stop,
        Start,
        GetTime,
        Config500Hz,
        Config1000Hz,
        Config2000Hz,
        Config4000Hz,
        Config8000Hz,
        Config16000Hz,
        ConfigImp500Hz,
        ConfigImp1000Hz,
        ConfigImp2000Hz,
        ConfigImp4000Hz,
        ConfigImp8000Hz,
        ConfigImp16000Hz
    };


    [[nodiscard]] AmplifierDetails getAmplifierDetails() const noexcept;
    [[nodiscard]] bool connect() const noexcept;
    [[nodiscard]] std::string getLastError() const noexcept;

private:
    struct pImplPerun32;
    std::unique_ptr<pImplPerun32> pImpl;
};

#endif // libperun32_API
