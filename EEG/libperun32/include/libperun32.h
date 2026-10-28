//
// Created by Szymon on 21/10/2026.
//

#ifndef libperun32_API
#define libperun32_API
#include <cstdint>
#include <memory>

class Perun32 {
    public:
    Perun32(uint16_t vid, uint16_t pid);
    virtual ~Perun32();

    struct AmplifierDetails {
        const uint16_t vid_{};
        const uint16_t pid_{};
        uint16_t device_model_{0xFFFF};
    };
    [[nodiscard]] AmplifierDetails getAmplifierDetails() const;

    [[nodiscard]] bool connect() const;


    private:
    struct pImplPerun32;
    std::unique_ptr<pImplPerun32> pImpl;

};

#endif // libperun32_API
