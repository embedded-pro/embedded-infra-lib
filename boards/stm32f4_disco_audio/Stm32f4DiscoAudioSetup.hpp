#ifndef BOARDS_STM32F4_DISCO_AUDIO_STM32F4_DISCO_AUDIO_SETUP_HPP
#define BOARDS_STM32F4_DISCO_AUDIO_STM32F4_DISCO_AUDIO_SETUP_HPP

#include "drivers/audio/cs43l22/Cs43l22.hpp"
#include "drivers/audio/cs43l22/Cs43l22BusAccessI2c.hpp"
#include "hal/interfaces/AudioOutput.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "hal/interfaces/I2c.hpp"
#include <cstdint>

namespace boards
{
    class Stm32f4DiscoAudioSetup
    {
    public:
        static constexpr uint16_t masterClockRatio{ 256 };
        static constexpr uint8_t defaultInitialVolume{ 50 };

        Stm32f4DiscoAudioSetup(hal::I2cMaster& i2c, hal::GpioPin& codecReset, hal::AudioOutput& i2sStream, uint8_t initialVolume = defaultInitialVolume);
        Stm32f4DiscoAudioSetup(const Stm32f4DiscoAudioSetup& other) = delete;
        Stm32f4DiscoAudioSetup& operator=(const Stm32f4DiscoAudioSetup& other) = delete;

        hal::AudioOutput& Output();

    private:
        drivers::Cs43l22BusAccessI2c bus;
        drivers::Cs43l22 codec;
    };
}

#endif
