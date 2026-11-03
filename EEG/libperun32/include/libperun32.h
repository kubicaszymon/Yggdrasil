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

    enum class SamplingRate
    {
        Hz500,
        Hz1000,
        Hz2000,
        Hz4000,
        Hz8000,
        Hz16000,
        Undefined
    };

    struct CommunicationDetails
    {
        SamplingRate sampling_rate{SamplingRate::Undefined};
        bool impedance_mode{false};
    };

    [[nodiscard]] AmplifierDetails getAmplifierDetails() const noexcept;
    [[nodiscard]] bool connect() const noexcept;
    [[nodiscard]] std::string getLastError() const noexcept;

private:
    struct pImplPerun32;
    std::unique_ptr<pImplPerun32> pImpl;
};

#endif // libperun32_API
