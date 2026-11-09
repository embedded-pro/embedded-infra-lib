#include "hal/null/AdcMultiChannelNull.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cstdint>

TEST(AdcMultiChannelNullTest, Measure_reports_neutral_code_on_every_channel)
{
    hal::AdcMultiChannelNull::WithChannels<3> adc{ uint16_t{ 2048 } };
    testing::StrictMock<infra::MockCallback<void(hal::AdcMultiChannel::Samples)>> onDone;

    EXPECT_CALL(onDone, callback(testing::ElementsAre(2048, 2048, 2048)));
    adc.Measure([&onDone](hal::AdcMultiChannel::Samples samples)
        {
            onDone.callback(samples);
        });
}

TEST(AdcMultiChannelNullTest, Measure_after_Stop_reports_neutral_code_again)
{
    hal::AdcMultiChannelNull::WithChannels<2> adc{ uint16_t{ 512 } };
    testing::StrictMock<infra::MockCallback<void(hal::AdcMultiChannel::Samples)>> onDone;

    adc.Stop();

    EXPECT_CALL(onDone, callback(testing::ElementsAre(512, 512)));
    adc.Measure([&onDone](hal::AdcMultiChannel::Samples samples)
        {
            onDone.callback(samples);
        });
}
