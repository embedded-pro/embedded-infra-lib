#include "boards/stm32f4_disco_audio/Stm32f4DiscoAudioSetup.hpp"
#include <optional>

namespace boards
{
    Stm32f4DiscoAudioSetup::Stm32f4DiscoAudioSetup(hal::I2cMaster& i2c, hal::GpioPin& codecReset, hal::AudioOutput& i2sStream, uint8_t initialVolume)
        : bus(i2c, drivers::Cs43l22BusAccessI2c::addressAd0Low)
        , codec(bus, i2sStream, codecReset, drivers::Cs43l22::Config{ drivers::Cs43l22::Output::headphone, initialVolume, masterClockRatio, std::nullopt })
    {}

    hal::AudioOutput& Stm32f4DiscoAudioSetup::Output()
    {
        return codec;
    }
}
