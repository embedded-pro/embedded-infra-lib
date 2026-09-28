#ifndef SERVICES_HIL_PWM_COMMANDS_HPP
#define SERVICES_HIL_PWM_COMMANDS_HPP

#include "hal/interfaces/DutyCycle.hpp"
#include "hal/interfaces/Pwm.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "services/hil/Command.hpp"
#include "services/hil/commands/SingleInstance.hpp"
#include "services/util/Terminal.hpp"
#include <array>
#include <cstddef>
#include <utility>

namespace services::hil
{
    class PwmHandle
    {
    protected:
        PwmHandle() = default;
        PwmHandle(const PwmHandle& other) = delete;
        PwmHandle& operator=(const PwmHandle& other) = delete;
        ~PwmHandle() = default;

    public:
        virtual std::size_t Channels() const = 0;
        virtual void Start(infra::MemoryRange<const hal::DutyCycle> dutyCycles) = 0;
        virtual void SetBaseFrequency(hal::Hertz baseFrequency) = 0;
        virtual void Stop() = 0;
    };

    template<class Driver>
    class PwmAdapter
        : public PwmHandle
    {
    public:
        PwmAdapter(Driver& driver, std::size_t channels);

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

    class PwmFactory
    {
    protected:
        PwmFactory() = default;
        PwmFactory(const PwmFactory& other) = delete;
        PwmFactory& operator=(const PwmFactory& other) = delete;
        ~PwmFactory() = default;

    public:
        virtual uint8_t Instances() const = 0;
        virtual infra::MemoryRange<const char* const> OpenKeys() const = 0;
        virtual Status Prepare(uint8_t module, const Arguments& arguments) = 0;
        virtual Status Open(uint8_t module, const Arguments& arguments, PinOwner& pins, PwmHandle*& handle) = 0;
        virtual void ReportOpened(uint8_t module, Response::Line& line) = 0;
        virtual Status ChangeFrequency(uint8_t module, uint32_t hertz) = 0;
        virtual void Close(uint8_t module, const infra::Function<void()>& onClosed) = 0;
    };

    class PwmCommands
        : public services::TerminalCommands
    {
    public:
        static constexpr std::size_t maximumChannels = 4;

        PwmCommands(Context& context, PwmFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    private:
        Status Open(const Arguments& arguments);
        Status Duty(const Arguments& arguments);
        Status Frequency(const Arguments& arguments);
        Status Stop(const Arguments& arguments);
        Status Close(const Arguments& arguments);

        Status OpenInstance(uint8_t module, const Arguments& arguments);
        void Closed();

    private:
        Context& context;
        PwmFactory& factory;
        SingleInstance instance;
        PinOwner pins;
        PwmHandle* handle = nullptr;
        std::array<Command, 5> commands;
    };

    ////    Implementation    ////

    template<class Driver>
    PwmAdapter<Driver>::PwmAdapter(Driver& driver, std::size_t channels)
        : driver(driver)
        , channels(channels)
    {
        really_assert(channels >= 1 && channels <= PwmCommands::maximumChannels);
    }

    template<class Driver>
    std::size_t PwmAdapter<Driver>::Channels() const
    {
        return channels;
    }

    template<class Driver>
    void PwmAdapter<Driver>::Start(infra::MemoryRange<const hal::DutyCycle> dutyCycles)
    {
        really_assert(dutyCycles.size() == 1 || dutyCycles.size() == channels);

        if (dutyCycles.size() == 1)
            StartSame(dutyCycles[0]);
        else
            StartEach(dutyCycles);
    }

    template<class Driver>
    void PwmAdapter<Driver>::SetBaseFrequency(hal::Hertz baseFrequency)
    {
        driver.SetBaseFrequency(baseFrequency);
    }

    template<class Driver>
    void PwmAdapter<Driver>::Stop()
    {
        driver.Stop();
    }

    template<class Driver>
    void PwmAdapter<Driver>::StartSame(hal::DutyCycle dutyCycle)
    {
        if constexpr (requires(Driver& pwm, hal::DutyCycle duty) { pwm.Start(duty); })
            driver.Start(dutyCycle);
        else
        {
            std::array<hal::DutyCycle, PwmCommands::maximumChannels> dutyCycles;
            dutyCycles.fill(dutyCycle);
            StartEach(infra::Head(infra::MakeRange(std::as_const(dutyCycles)), channels));
        }
    }

    template<class Driver>
    void PwmAdapter<Driver>::StartEach(infra::MemoryRange<const hal::DutyCycle> dutyCycles)
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
