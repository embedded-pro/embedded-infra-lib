#ifndef DRIVERS_DISPLAY_MIPI_DSI_TEST_MIPI_DSI_TEST_PANEL_HPP
#define DRIVERS_DISPLAY_MIPI_DSI_TEST_MIPI_DSI_TEST_PANEL_HPP

#include "drivers/display/mipi_dsi/MipiDsiPanelCore.hpp"
#include "hal/interfaces/test_doubles/DsiHostMock.hpp"
#include "gmock/gmock.h"
#include <array>
#include <cstdint>
#include <vector>

namespace dsitest
{
    using Command = drivers::MipiDsiPanelCore::Command;
    using Identification = drivers::MipiDsiPanelCore::Identification;
    using InitializationResult = drivers::MipiDsiPanelCore::InitializationResult;
    using Packet = drivers::MipiDsiPanelCore::Packet;
    using Panel = drivers::MipiDsiPanelCore::Panel;

    inline constexpr hal::DisplaySize panelSize{ 480, 800 };
    inline constexpr uint8_t addressMode = 0x48;

    inline constexpr std::array<uint8_t, 0> noParameters{};
    inline constexpr std::array<uint8_t, 2> vendorPassword{ 0xff, 0x83 };
    inline constexpr std::array<uint8_t, 2> vendorRegisterB{ 0xba, 0x01 };
    inline constexpr std::array<uint8_t, 3> gamma{ 0x11, 0x22, 0x33 };
    inline constexpr std::array<uint8_t, 3> vendorRegisterE{ 0xe0, 0x05, 0x06 };
    inline constexpr std::array<uint8_t, 3> panelIdentification{ 0x12, 0x34, 0x56 };

    inline constexpr std::array<Command, 2> beforeSleepOut{ { { Packet::dcs, 0xb9, vendorPassword, 0 }, { Packet::generic, 0, vendorRegisterB, 10 } } };
    inline constexpr std::array<Command, 3> afterSleepOut{ { { Packet::dcs, 0xc1, gamma, 0 }, { Packet::generic, 0, vendorRegisterE, 0 }, { Packet::dcs, 0xd2, noParameters, 5 } } };

    inline Panel MakePanel(Identification identification = Identification{ 0, infra::ConstByteRange() })
    {
        return Panel{ panelSize, addressMode, identification, beforeSleepOut, afterSleepOut, {} };
    }

    inline Panel MakeIdentifiedPanel()
    {
        return MakePanel(Identification{ 0x04, panelIdentification });
    }

    struct InitializationOptions
    {
        uint8_t pixelFormat{ 0x55 };
        uint8_t addressMode{ 0x48 };
        bool tearingEffect{ false };
    };

    inline void ExpectDcs(hal::DsiHostMock& host, uint8_t command, std::vector<uint8_t> parameters)
    {
        EXPECT_CALL(host, WriteDcsMock(command, parameters));
    }

    inline void ExpectGeneric(hal::DsiHostMock& host, std::vector<uint8_t> data)
    {
        EXPECT_CALL(host, WriteGenericMock(data));
    }

    inline void ExpectInitializationBeforeDisplayOn(hal::DsiHostMock& host, InitializationOptions options = InitializationOptions())
    {
        ExpectDcs(host, 0xb9, { 0xff, 0x83 });
        ExpectGeneric(host, { 0xba, 0x01 });
        ExpectDcs(host, 0x11, {});
        ExpectDcs(host, 0x3a, { options.pixelFormat });
        ExpectDcs(host, 0x36, { options.addressMode });

        if (options.tearingEffect)
            ExpectDcs(host, 0x35, { 0x00 });

        ExpectDcs(host, 0xc1, { 0x11, 0x22, 0x33 });
        ExpectGeneric(host, { 0xe0, 0x05, 0x06 });
        ExpectDcs(host, 0xd2, {});
    }

    inline void AllowAnyCommands(hal::DsiHostMock& host)
    {
        EXPECT_CALL(host, WriteDcsMock(testing::_, testing::_)).Times(testing::AnyNumber());
        EXPECT_CALL(host, WriteGenericMock(testing::_)).Times(testing::AnyNumber());
    }
}

#endif
