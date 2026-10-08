#ifndef DRIVERS_MICROPHONES_PDM_TEST_DOUBLES_PDM_MICROPHONE_CHIP_TEST_HPP
#define DRIVERS_MICROPHONES_PDM_TEST_DOUBLES_PDM_MICROPHONE_CHIP_TEST_HPP

#include "drivers/microphones/pdm/test_doubles/PdmToPcmMock.hpp"
#include "hal/interfaces/test_doubles/AudioInputStub.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/MemoryRange.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace drivers
{
    template<class Microphone>
    class PdmMicrophoneChipTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        PdmMicrophoneChipTest()
        {
            EXPECT_CALL(converter, Decimation()).WillRepeatedly(testing::ReturnPointee(&decimation));
            EXPECT_CALL(converter, MaxSamples(testing::_)).WillRepeatedly(testing::ReturnPointee(&producing));
            EXPECT_CALL(converter, Convert(testing::_, testing::_)).WillRepeatedly(testing::ReturnPointee(&producing));

            driver.emplace(input, converter, infra::MakeRange(storage));
        }

        ~PdmMicrophoneChipTest() override
        {
            EXPECT_CALL(input, Stop(testing::_)).Times(testing::AnyNumber());
            driver.reset();
        }

        void Start(hal::AudioFormat format, uint16_t converterDecimation, hal::AudioFormat sourceFormat)
        {
            decimation = converterDecimation;
            EXPECT_CALL(converter, Reset(format.channels, format.sampleRate));
            EXPECT_CALL(input, Start(sourceFormat, testing::_, testing::_));

            driver->Start(
                format, [this](hal::AudioInput::Samples samples)
                {
                    ++periodsReceived;
                    receivedSize = samples.size();
                },
                []() {});
        }

        void Capture(std::size_t samples)
        {
            producing = samples;
            input.PeriodCaptured(infra::MakeConstRange(captured));
        }

        testing::StrictMock<hal::AudioInputStub> input;
        testing::StrictMock<PdmToPcmMock> converter;
        std::array<int16_t, 4096> storage{};
        std::array<int16_t, 16> captured{};
        std::optional<Microphone> driver;

        uint16_t decimation{ 0 };
        std::size_t producing{ 0 };
        std::size_t receivedSize{ 0 };
        int periodsReceived{ 0 };
    };
}

#endif
