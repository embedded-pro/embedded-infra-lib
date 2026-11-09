#include "hal/qemu/cmsdk/CmsdkLineUart.hpp"
#include "gtest/gtest.h"
#include <cstdint>

namespace
{
    class CmsdkLineUartTest
        : public testing::Test
    {
    public:
        hal::CmsdkUartRegisters registers{};
        hal::CmsdkLineUart::WithMaxLineLength<16> uart{ reinterpret_cast<uintptr_t>(&registers) };
    };
}

TEST_F(CmsdkLineUartTest, construction_enables_the_transmitter_and_receiver)
{
    EXPECT_EQ(hal::uartControlTxEnable | hal::uartControlRxEnable, registers.control);
}

TEST_F(CmsdkLineUartTest, construction_leaves_the_baud_divisor_untouched)
{
    EXPECT_EQ(0u, registers.bauddiv);
}

TEST_F(CmsdkLineUartTest, poll_without_received_data_reports_no_line)
{
    registers.data = 'a';

    EXPECT_EQ(nullptr, uart.PollLine());
}

TEST_F(CmsdkLineUartTest, write_places_the_text_in_the_data_register)
{
    uart.Write("x");

    EXPECT_EQ(static_cast<uint32_t>('x'), registers.data);
}

TEST_F(CmsdkLineUartTest, write_of_empty_text_leaves_the_data_register_untouched)
{
    registers.data = 0x5a;

    uart.Write("");

    EXPECT_EQ(0x5au, registers.data);
}
