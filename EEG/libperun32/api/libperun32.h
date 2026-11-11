//
// Created by Szymon on 21/10/2026.
//

#ifndef libperun32_API
#define libperun32_API
#include <array>
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

    struct Sample
    {
        /*
        Index      Label on cap
        0               Fp1
        1               Fp2
        2               F7
        3               F3
        4               Fz
        5               F4
        6               F8
        7               M1
        8               T3
        9               C3
        10              Cz
        11              C4
        12              T4
        13              M2
        14              T5
        15              P3
        16              Pz
        17              P4
        18              T6
        19              O1
        20              O2
        21              A1
        22              A2
        23              AFz/GND
        24              AUX 1
        25              AUX 2
        26              AUX 3
        27              AUX 4
        28              AUX 5
        29              AUX 6
        30              AUX 7
        31              AUX 8
    */
        static constexpr std::size_t EEG_CHANNELS = 24;
        static constexpr std::size_t AUX_CHANNELS = 8;
        [[nodiscard]] static std::string channelIndexToCapLabel(std::uint8_t channel_index);

        std::uint32_t device_timestamp_us{}; // 24 bits
        std::uint8_t events{};
        std::array<std::int32_t, 32> channels{};
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
    bool readSamples(std::vector<Sample> &out, std::chrono::milliseconds timeout) const;

private:
    class pImplPerun32;
    std::unique_ptr<pImplPerun32> pImpl;
};

#endif // libperun32_API
