#ifndef SERVICES_HIL_HIL_FIXTURE_HPP
#define SERVICES_HIL_HIL_FIXTURE_HPP

#include "hal/interfaces/test_doubles/GpioMock.hpp"
#include "infra/stream/StringOutputStream.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "services/hil/Command.hpp"
#include "services/hil/PinNaming.hpp"
#include "services/hil/PinPool.hpp"
#include "services/hil/Response.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/Terminal.hpp"
#include "gmock/gmock.h"
#include <array>
#include <optional>
#include <string>

namespace services::hil
{
    class FakePinFactory
        : public PinFactory
    {
    public:
        bool IsValid(PinId pin) const override
        {
            return pin.port < 6 && pin.index <= 7;
        }

        bool SupportsFunction(PinId pin, uint16_t, uint8_t) const override
        {
            return pin != unsupportedFunctionPin;
        }

        bool SupportsAnalog(PinId pin) const override
        {
            return pin.port == 4;
        }

        bool SupportsInterrupt(PinId pin) const override
        {
            return pin.port < 5;
        }

        std::optional<uint8_t> ParseDrive(infra::BoundedConstString text) const override
        {
            if (text == "2")
                return 0;
            if (text == "8")
                return 2;

            return std::nullopt;
        }

        hal::GpioPin& Construct(std::size_t slot, PinId pin, const PinOptions& options) override
        {
            constructed[slot] = pin;
            this->options[slot] = options;
            return pins[slot];
        }

        void Destroy(std::size_t slot) override
        {
            constructed[slot] = std::nullopt;
        }

        PinId unsupportedFunctionPin{ 5, 7 };
        std::array<testing::StrictMock<hal::GpioPinMock>, 8> pins;
        std::array<std::optional<PinId>, 8> constructed;
        std::array<PinOptions, 8> options;
    };

    class HilFixture
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void Execute(const char* line)
        {
            response.BeginCommand();
            bool processed = terminal.NotifyObservers([line](services::TerminalCommands& commands)
                {
                    return commands.ProcessCommand(line);
                });
            response.EndCommand();

            EXPECT_TRUE(processed) << line;
        }

        std::string Output()
        {
            std::string result(stream.Storage().begin(), stream.Storage().end());
            stream.Storage().clear();
            return result;
        }

        static constexpr std::array<PinAlias, 3> aliases{ {
            { "led", { 5, 1 } },
            { "id0", { 2, 3 }, Pull::up },
            { "vbus", { 4, 0 } },
        } };

        infra::StringOutputStream::WithStorage<2048> stream;
        services::TracerToStream tracer{ stream };
        Response response{ tracer };
        FakePinFactory pinFactory;
        std::array<PinId, 2> reserved{ { { 0, 0 }, { 0, 1 } } };
        PinPool::WithCapacity<8> pins{ pinFactory, infra::MakeRange(std::as_const(reserved)) };
        PinNamingDefault naming{ "ABCDEF", 7, infra::MakeRange(aliases) };
        services::TerminalWithCommands terminal;
        Context context{ response, pins, naming, terminal };
    };
}

#endif
