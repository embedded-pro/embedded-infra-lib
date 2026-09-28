#ifndef SERVICES_HIL_PWM_COMMANDS_HPP
#define SERVICES_HIL_PWM_COMMANDS_HPP

#include "hal/interfaces/DutyCycle.hpp"
#include "hal/interfaces/Pwm.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "services/hil/HilCommand.hpp"
#include "services/hil/commands/HilSingleInstance.hpp"
#include "services/util/Terminal.hpp"
#include <array>
#include <cstddef>
#include <utility>

namespace services
{
    class HilPwmHandle
    {
    protected:
        HilPwmHandle() = default;
        HilPwmHandle(const HilPwmHandle& other) = delete;
        HilPwmHandle& operator=(const HilPwmHandle& other) = delete;
        ~HilPwmHandle() = default;

    public:
        virtual std::size_t Channels() const = 0;
        virtual void Start(infra::MemoryRange<const hal::DutyCycle> dutyCycles) = 0;
        virtual void SetBaseFrequency(hal::Hertz baseFrequency) = 0;
        virtual void Stop() = 0;
    };

    template<class Driver>
    class HilPwmAdapter
        : public HilPwmHandle
    {
    public:
        HilPwmAdapter(Driver& driver, std::size_t channels);

        std::size_t Channels() const override;
        void Start(infra::MemoryRange<const hal::DutyCycle> dutyCycles) override;
        void SetBaseFrequency(hal::Hertz baseFrequency) override;
        void Stop() override;

    private:
        void StartSame(hal::DutyCycle dutyCycle);
        void StartEach(infra::MemoryRange<const hal::DutyCycle> dutyCycles);

    private:
        Driver& driver;
        std::size_t channels;
    };

    class HilPwmFactory
    {
    protected:
        HilPwmFactory() = default;
        HilPwmFactory(const HilPwmFactory& other) = delete;
        HilPwmFactory& operator=(const HilPwmFactory& other) = delete;
        ~HilPwmFactory() = default;

    public:
        virtual uint8_t Instances() const = 0;
        virtual infra::MemoryRange<const char* const> OpenKeys() const = 0;
        virtual HilStatus Prepare(uint8_t moduleIndex, const HilArguments& arguments) = 0;
        virtual HilStatus Open(uint8_t moduleIndex, const HilArguments& arguments, HilPinOwner& pins, HilPwmHandle*& handle) = 0;
        virtual void ReportOpened(uint8_t moduleIndex, HilResponse::Line& line) = 0;
        virtual HilStatus ChangeFrequency(uint8_t moduleIndex, uint32_t hertz) = 0;
        virtual void Close(uint8_t moduleIndex, const infra::Function<void()>& onClosed) = 0;
    };

    class HilPwmCommands
        : public services::TerminalCommands
    {
    public:
        static constexpr std::size_t maximumChannels = 4;

        HilPwmCommands(HilContext& context, HilPwmFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    private:
        HilStatus Open(const HilArguments& arguments);
        HilStatus Duty(const HilArguments& arguments);
        HilStatus Frequency(const HilArguments& arguments);
        HilStatus Stop(const HilArguments& arguments);
        HilStatus Close(const HilArguments& arguments);

        HilStatus OpenInstance(uint8_t moduleIndex, const HilArguments& arguments);
        void Closed();

    private:
        HilContext& context;
        HilPwmFactory& factory;
        HilSingleInstance instance;
        HilPinOwner pins;
        HilPwmHandle* handle = nullptr;
        std::array<Command, 5> commands;
    };

    ////    Implementation    ////

    template<class Driver>
    HilPwmAdapter<Driver>::HilPwmAdapter(Driver& driver, std::size_t channels)
        : driver(driver)
        , channels(channels)
    {
        really_assert(channels >= 1 && channels <= HilPwmCommands::maximumChannels);
    }

    template<class Driver>
    std::size_t HilPwmAdapter<Driver>::Channels() const
    {
        return channels;
    }

    template<class Driver>
    void HilPwmAdapter<Driver>::Start(infra::MemoryRange<const hal::DutyCycle> dutyCycles)
    {
        really_assert(dutyCycles.size() == 1 || dutyCycles.size() == channels);

        if (dutyCycles.size() == 1)
            StartSame(dutyCycles[0]);
        else
            StartEach(dutyCycles);
    }

    template<class Driver>
    void HilPwmAdapter<Driver>::SetBaseFrequency(hal::Hertz baseFrequency)
    {
        driver.SetBaseFrequency(baseFrequency);
    }

    template<class Driver>
    void HilPwmAdapter<Driver>::Stop()
    {
        driver.Stop();
    }

    template<class Driver>
    void HilPwmAdapter<Driver>::StartSame(hal::DutyCycle dutyCycle)
    {
        if constexpr (requires(Driver& pwm, hal::DutyCycle duty) { pwm.Start(duty); })
            driver.Start(dutyCycle);
        else
        {
            std::array<hal::DutyCycle, HilPwmCommands::maximumChannels> dutyCycles;
            dutyCycles.fill(dutyCycle);
            StartEach(infra::Head(infra::MakeRange(std::as_const(dutyCycles)), channels));
        }
    }

    template<class Driver>
    void HilPwmAdapter<Driver>::StartEach(infra::MemoryRange<const hal::DutyCycle> dutyCycles)
    {
        if constexpr (requires(Driver& pwm, hal::DutyCycle duty) { pwm.Start(duty); })
            if (dutyCycles.size() == 1)
                driver.Start(dutyCycles[0]);

        if constexpr (requires(Driver& pwm, hal::DutyCycle duty) { pwm.Start(duty, duty); })
            if (dutyCycles.size() == 2)
                driver.Start(dutyCycles[0], dutyCycles[1]);

        if constexpr (requires(Driver& pwm, hal::DutyCycle duty) { pwm.Start(duty, duty, duty); })
            if (dutyCycles.size() == 3)
                driver.Start(dutyCycles[0], dutyCycles[1], dutyCycles[2]);

        if constexpr (requires(Driver& pwm, hal::DutyCycle duty) { pwm.Start(duty, duty, duty, duty); })
            if (dutyCycles.size() == 4)
                driver.Start(dutyCycles[0], dutyCycles[1], dutyCycles[2], dutyCycles[3]);
    }
}

#endif
