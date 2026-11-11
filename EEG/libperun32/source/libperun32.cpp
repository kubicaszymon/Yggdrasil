//
// Created by Szymon on 21/10/2026.
//

#include "../api/libperun32.h"
#include "perun32.h"
#include "commands.h"

Perun32::Perun32(const uint16_t vid, const uint16_t pid)
    : pImpl(std::make_unique<pImplPerun32>(vid, pid)) {}

Perun32::AmplifierDetails Perun32::getAmplifierDetails() const noexcept
{
    return pImpl->amp_details;
}

bool Perun32::connect() const
{
    return pImpl->connect();
}

void Perun32::disconnect() const
{
    pImpl->disconnect();
}

std::string Perun32::getLastError() const noexcept
{
    return pImpl->getLastError();
}

bool Perun32::setSamplingRate(const SamplingRate sampling_rate, const bool impedance_mode) const
{
    return pImpl->setSamplingRate(sampling_rate, impedance_mode);
}

bool Perun32::startAcquisition() const
{
    return pImpl->start();
}

bool Perun32::stopAcquisition() const
{
    return pImpl->stop();
}

bool Perun32::readSamples(std::vector<Sample> &out, const std::chrono::milliseconds timeout) const
{
    return pImpl->readSamples(out, timeout);
}

/*
* why here? because if I would not put it there, the compiler would generate default destructor in the class declaration (.h file),
* and such destructor will try to delete pImpl unique_ptr and because I made it as incomplete struct declaration (PImpl),
* the destructor in .h file would not determine the overall size of object thus it needs to be done here where the PImpl type is completed.
*/
Perun32::~Perun32() = default;

std::string Perun32::Sample::channelIndexToCapLabel(const std::uint8_t channel_index)
{
    return channel_index < 32 ? EegData::layout32.label[channel_index] : std::string{};
}
