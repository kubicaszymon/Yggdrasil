//
// Created by Szymon on 21/10/2026.
//

#ifndef libperun32_API
#define libperun32_API
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/*
 * @brief
 */
class Perun32
{
public:
    Perun32(uint16_t vid, uint16_t pid);
    virtual ~Perun32();

    /*
     * @brief
     */
    struct AmplifierDetails
    {
        const uint16_t vid_{};
        const uint16_t pid_{};
        uint16_t device_model_{0xFFFF};
    };

    /*
     * @brief
     */
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

    /*
     * @brief
     */
    struct CommunicationDetails
    {
        SamplingRate sampling_rate{SamplingRate::Undefined};
        bool impedance_mode{false};
    };

    /*
     * @brief
     */
    [[nodiscard]] AmplifierDetails getAmplifierDetails() const noexcept;

    /*
     * @brief
     */
    [[nodiscard]] bool connect() const;

    /*
    * @brief
    */
    void disconnect() const;

    /*
     * @brief
     */
    [[nodiscard]] std::string getLastError() const noexcept;

    /*
     * @brief
     */
    [[nodiscard]] bool setSamplingRate(SamplingRate sampling_rate, bool impedance_mode = false) const;

    /*
     * @brief
    */
    [[nodiscard]] bool startAcquisition() const;

    /*
    * @brief
    */
    [[nodiscard]] bool stopAcquisition() const;

    /*
    * @brief
    */
    bool readRaw(std::vector<uint8_t> &out, std::chrono::milliseconds timeout) const;

private:
    class pImplPerun32;
    std::unique_ptr<pImplPerun32> pImpl;
};

#endif // libperun32_API
