#ifndef BOARDS_MB1166_MB1166_SETUP_HPP
#define BOARDS_MB1166_MB1166_SETUP_HPP

#include "drivers/display/mipi_dsi/MipiDsiPanelCore.hpp"
#include "drivers/display/mipi_dsi/MipiDsiVideoPanel.hpp"
#include "hal/interfaces/Display.hpp"
#include "hal/interfaces/DsiHost.hpp"
#include "hal/interfaces/Gpio.hpp"
#include <array>
#include <chrono>
#include <cstdint>

namespace boards
{
    namespace mb1166
    {
        using Command = drivers::MipiDsiPanelCore::Command;
        using Packet = drivers::MipiDsiPanelCore::Packet;

        inline constexpr std::array<uint8_t, 0> noIdentification{};

        inline constexpr std::array<uint8_t, 1> shift00{ 0x00 };
        inline constexpr std::array<uint8_t, 3> registerFFa{ 0x80, 0x09, 0x01 };
        inline constexpr std::array<uint8_t, 1> shift80{ 0x80 };
        inline constexpr std::array<uint8_t, 2> registerFFb{ 0x80, 0x09 };
        inline constexpr std::array<uint8_t, 1> registerC4a{ 0x30 };
        inline constexpr std::array<uint8_t, 1> shift8A{ 0x8a };
        inline constexpr std::array<uint8_t, 1> registerC4b{ 0x40 };
        inline constexpr std::array<uint8_t, 1> shiftB1{ 0xb1 };
        inline constexpr std::array<uint8_t, 1> registerC5a{ 0xa9 };
        inline constexpr std::array<uint8_t, 1> shift91{ 0x91 };
        inline constexpr std::array<uint8_t, 1> registerC5b{ 0x34 };
        inline constexpr std::array<uint8_t, 1> shiftB4{ 0xb4 };
        inline constexpr std::array<uint8_t, 1> registerC0a{ 0x50 };
        inline constexpr std::array<uint8_t, 1> registerD9a{ 0x4e };
        inline constexpr std::array<uint8_t, 1> shift81{ 0x81 };
        inline constexpr std::array<uint8_t, 1> registerC1a{ 0x66 };
        inline constexpr std::array<uint8_t, 1> shiftA1{ 0xa1 };
        inline constexpr std::array<uint8_t, 1> registerC1b{ 0x08 };
        inline constexpr std::array<uint8_t, 1> shift92{ 0x92 };
        inline constexpr std::array<uint8_t, 1> registerC5c{ 0x01 };
        inline constexpr std::array<uint8_t, 1> shift95{ 0x95 };
        inline constexpr std::array<uint8_t, 2> registerD8a{ 0x79, 0x79 };
        inline constexpr std::array<uint8_t, 1> shift94{ 0x94 };
        inline constexpr std::array<uint8_t, 1> registerC5d{ 0x33 };
        inline constexpr std::array<uint8_t, 1> shiftA3{ 0xa3 };
        inline constexpr std::array<uint8_t, 1> registerC0b{ 0x1b };
        inline constexpr std::array<uint8_t, 1> shift82{ 0x82 };
        inline constexpr std::array<uint8_t, 1> registerC5e{ 0x83 };
        inline constexpr std::array<uint8_t, 1> registerC4c{ 0x83 };
        inline constexpr std::array<uint8_t, 1> registerC1c{ 0x0e };
        inline constexpr std::array<uint8_t, 1> shiftA6{ 0xa6 };
        inline constexpr std::array<uint8_t, 2> registerB3a{ 0x00, 0x01 };
        inline constexpr std::array<uint8_t, 6> registerCEa{ 0x85, 0x01, 0x00, 0x84, 0x01, 0x00 };
        inline constexpr std::array<uint8_t, 1> shiftA0{ 0xa0 };
        inline constexpr std::array<uint8_t, 14> registerCEb{ 0x18, 0x04, 0x03, 0x39, 0x00, 0x00, 0x00, 0x18, 0x03, 0x03, 0x3a, 0x00, 0x00, 0x00 };
        inline constexpr std::array<uint8_t, 1> shiftB0{ 0xb0 };
        inline constexpr std::array<uint8_t, 14> registerCEc{ 0x18, 0x02, 0x03, 0x3b, 0x00, 0x00, 0x00, 0x18, 0x01, 0x03, 0x3c, 0x00, 0x00, 0x00 };
        inline constexpr std::array<uint8_t, 1> shiftC0{ 0xc0 };
        inline constexpr std::array<uint8_t, 10> registerCFa{ 0x01, 0x01, 0x20, 0x20, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00 };
        inline constexpr std::array<uint8_t, 1> shiftD0{ 0xd0 };
        inline constexpr std::array<uint8_t, 1> registerCFb{ 0x00 };
        inline constexpr std::array<uint8_t, 10> registerCBa{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        inline constexpr std::array<uint8_t, 1> shift90{ 0x90 };
        inline constexpr std::array<uint8_t, 15> registerCBb{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        inline constexpr std::array<uint8_t, 15> registerCBc{ 0x00, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        inline constexpr std::array<uint8_t, 15> registerCBd{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00 };
        inline constexpr std::array<uint8_t, 1> shiftE0{ 0xe0 };
        inline constexpr std::array<uint8_t, 1> shiftF0{ 0xf0 };
        inline constexpr std::array<uint8_t, 10> registerCBe{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
        inline constexpr std::array<uint8_t, 10> registerCCa{ 0x00, 0x26, 0x09, 0x0b, 0x01, 0x25, 0x00, 0x00, 0x00, 0x00 };
        inline constexpr std::array<uint8_t, 15> registerCCb{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x26, 0x0a, 0x0c, 0x02 };
        inline constexpr std::array<uint8_t, 15> registerCCc{ 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        inline constexpr std::array<uint8_t, 10> registerCCd{ 0x00, 0x25, 0x0c, 0x0a, 0x02, 0x26, 0x00, 0x00, 0x00, 0x00 };
        inline constexpr std::array<uint8_t, 15> registerCCe{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x25, 0x0b, 0x09, 0x01 };
        inline constexpr std::array<uint8_t, 15> registerCCf{ 0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        inline constexpr std::array<uint8_t, 1> registerC5f{ 0x66 };
        inline constexpr std::array<uint8_t, 1> shiftB6{ 0xb6 };
        inline constexpr std::array<uint8_t, 1> registerF5a{ 0x06 };
        inline constexpr std::array<uint8_t, 1> registerC6a{ 0x06 };
        inline constexpr std::array<uint8_t, 3> registerFFc{ 0xff, 0xff, 0xff };
        inline constexpr std::array<uint8_t, 16> registerE1a{ 0x00, 0x09, 0x0f, 0x0e, 0x07, 0x10, 0x0b, 0x0a, 0x04, 0x07, 0x0b, 0x08, 0x0f, 0x10, 0x0a, 0x01 };
        inline constexpr std::array<uint8_t, 16> registerE2a{ 0x00, 0x09, 0x0f, 0x0e, 0x07, 0x10, 0x0b, 0x0a, 0x04, 0x07, 0x0b, 0x08, 0x0f, 0x10, 0x0a, 0x01 };
        inline constexpr std::array<uint8_t, 4> register2Aa{ 0x00, 0x00, 0x03, 0x1f };
        inline constexpr std::array<uint8_t, 4> register2Ba{ 0x00, 0x00, 0x01, 0xdf };
        inline constexpr std::array<uint8_t, 1> register51a{ 0x7f };
        inline constexpr std::array<uint8_t, 1> register53a{ 0x2c };
        inline constexpr std::array<uint8_t, 1> register55a{ 0x02 };
        inline constexpr std::array<uint8_t, 1> register5Ea{ 0xff };

        inline constexpr std::array<Command, 89> beforeSleepOut{ {
            { Packet::dcs, 0x00, shift00, 0 },
            { Packet::dcs, 0xff, registerFFa, 0 },
            { Packet::dcs, 0x00, shift80, 0 },
            { Packet::dcs, 0xff, registerFFb, 0 },
            { Packet::dcs, 0x00, shift80, 0 },
            { Packet::dcs, 0xc4, registerC4a, 10 },
            { Packet::dcs, 0x00, shift8A, 0 },
            { Packet::dcs, 0xc4, registerC4b, 10 },
            { Packet::dcs, 0x00, shiftB1, 0 },
            { Packet::dcs, 0xc5, registerC5a, 0 },
            { Packet::dcs, 0x00, shift91, 0 },
            { Packet::dcs, 0xc5, registerC5b, 0 },
            { Packet::dcs, 0x00, shiftB4, 0 },
            { Packet::dcs, 0xc0, registerC0a, 0 },
            { Packet::dcs, 0x00, shift00, 0 },
            { Packet::dcs, 0xd9, registerD9a, 0 },
            { Packet::dcs, 0x00, shift81, 0 },
            { Packet::dcs, 0xc1, registerC1a, 0 },
            { Packet::dcs, 0x00, shiftA1, 0 },
            { Packet::dcs, 0xc1, registerC1b, 0 },
            { Packet::dcs, 0x00, shift92, 0 },
            { Packet::dcs, 0xc5, registerC5c, 0 },
            { Packet::dcs, 0x00, shift95, 0 },
            { Packet::dcs, 0xc5, registerC5b, 0 },
            { Packet::dcs, 0x00, shift00, 0 },
            { Packet::dcs, 0xd8, registerD8a, 0 },
            { Packet::dcs, 0x00, shift94, 0 },
            { Packet::dcs, 0xc5, registerC5d, 0 },
            { Packet::dcs, 0x00, shiftA3, 0 },
            { Packet::dcs, 0xc0, registerC0b, 0 },
            { Packet::dcs, 0x00, shift82, 0 },
            { Packet::dcs, 0xc5, registerC5e, 0 },
            { Packet::dcs, 0x00, shift81, 0 },
            { Packet::dcs, 0xc4, registerC4c, 0 },
            { Packet::dcs, 0x00, shiftA1, 0 },
            { Packet::dcs, 0xc1, registerC1c, 0 },
            { Packet::dcs, 0x00, shiftA6, 0 },
            { Packet::dcs, 0xb3, registerB3a, 0 },
            { Packet::dcs, 0x00, shift80, 0 },
            { Packet::dcs, 0xce, registerCEa, 0 },
            { Packet::dcs, 0x00, shiftA0, 0 },
            { Packet::dcs, 0xce, registerCEb, 0 },
            { Packet::dcs, 0x00, shiftB0, 0 },
            { Packet::dcs, 0xce, registerCEc, 0 },
            { Packet::dcs, 0x00, shiftC0, 0 },
            { Packet::dcs, 0xcf, registerCFa, 0 },
            { Packet::dcs, 0x00, shiftD0, 0 },
            { Packet::dcs, 0xcf, registerCFb, 0 },
            { Packet::dcs, 0x00, shift80, 0 },
            { Packet::dcs, 0xcb, registerCBa, 0 },
            { Packet::dcs, 0x00, shift90, 0 },
            { Packet::dcs, 0xcb, registerCBb, 0 },
            { Packet::dcs, 0x00, shiftA0, 0 },
            { Packet::dcs, 0xcb, registerCBb, 0 },
            { Packet::dcs, 0x00, shiftB0, 0 },
            { Packet::dcs, 0xcb, registerCBa, 0 },
            { Packet::dcs, 0x00, shiftC0, 0 },
            { Packet::dcs, 0xcb, registerCBc, 0 },
            { Packet::dcs, 0x00, shiftD0, 0 },
            { Packet::dcs, 0xcb, registerCBd, 0 },
            { Packet::dcs, 0x00, shiftE0, 0 },
            { Packet::dcs, 0xcb, registerCBa, 0 },
            { Packet::dcs, 0x00, shiftF0, 0 },
            { Packet::dcs, 0xcb, registerCBe, 0 },
            { Packet::dcs, 0x00, shift80, 0 },
            { Packet::dcs, 0xcc, registerCCa, 0 },
            { Packet::dcs, 0x00, shift90, 0 },
            { Packet::dcs, 0xcc, registerCCb, 0 },
            { Packet::dcs, 0x00, shiftA0, 0 },
            { Packet::dcs, 0xcc, registerCCc, 0 },
            { Packet::dcs, 0x00, shiftB0, 0 },
            { Packet::dcs, 0xcc, registerCCd, 0 },
            { Packet::dcs, 0x00, shiftC0, 0 },
            { Packet::dcs, 0xcc, registerCCe, 0 },
            { Packet::dcs, 0x00, shiftD0, 0 },
            { Packet::dcs, 0xcc, registerCCf, 0 },
            { Packet::dcs, 0x00, shift81, 0 },
            { Packet::dcs, 0xc5, registerC5f, 0 },
            { Packet::dcs, 0x00, shiftB6, 0 },
            { Packet::dcs, 0xf5, registerF5a, 0 },
            { Packet::dcs, 0x00, shiftB1, 0 },
            { Packet::dcs, 0xc6, registerC6a, 0 },
            { Packet::dcs, 0x00, shift00, 0 },
            { Packet::dcs, 0xff, registerFFc, 0 },
            { Packet::dcs, 0x00, shift00, 0 },
            { Packet::dcs, 0x00, shift00, 0 },
            { Packet::dcs, 0xe1, registerE1a, 0 },
            { Packet::dcs, 0x00, shift00, 0 },
            { Packet::dcs, 0xe2, registerE2a, 0 },
        } };

        inline constexpr std::array<Command, 6> afterSleepOut{ {
            { Packet::dcs, 0x2a, register2Aa, 0 },
            { Packet::dcs, 0x2b, register2Ba, 0 },
            { Packet::dcs, 0x51, register51a, 0 },
            { Packet::dcs, 0x53, register53a, 0 },
            { Packet::dcs, 0x55, register55a, 0 },
            { Packet::dcs, 0x5e, register5Ea, 0 },
        } };

        constexpr drivers::MipiDsiPanelCore::Timings MakeTimings()
        {
            drivers::MipiDsiPanelCore::Timings timings;
            timings.resetPulse = std::chrono::milliseconds(20);
            timings.resetRecovery = std::chrono::milliseconds(10);
            return timings;
        }
    }

    inline constexpr drivers::MipiDsiPanelCore::Panel mb1166Panel{ { 800, 480 }, 0x60, { 0, mb1166::noIdentification }, mb1166::beforeSleepOut, mb1166::afterSleepOut, mb1166::MakeTimings() };

    class Mb1166Setup
    {
    public:
        Mb1166Setup(hal::DsiHost& host, hal::DsiVideoStream& stream, hal::GpioPin& reset, hal::PixelFormat format, const infra::Function<void(drivers::MipiDsiPanelCore::InitializationResult)>& onInitialized);
        Mb1166Setup(const Mb1166Setup& other) = delete;
        Mb1166Setup& operator=(const Mb1166Setup& other) = delete;

        drivers::MipiDsiVideoPanel& Panel();

    private:
        drivers::MipiDsiVideoPanel panel;
    };
}

#endif
