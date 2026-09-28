#ifndef SERVICES_HIL_ADC_COMMANDS_HPP
#define SERVICES_HIL_ADC_COMMANDS_HPP

#include "hal/interfaces/AdcMultiChannel.hpp"
#include "hal/synchronous_interfaces/SynchronousAdc.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/WithStorage.hpp"
#include "services/hil/Command.hpp"
#include "services/util/Terminal.hpp"
#include <array>
#include <atomic>
#include <optional>

namespace services::hil
{
    struct AdcHandle
    {
        hal::AdcMultiChannel* adc = nullptr;
        hal::SynchronousAdc* synchronous = nullptr;
        std::size_t samplesPerRun = 0;
    };

    class AdcFactory
    {
    protected:
        AdcFactory() = default;
        AdcFactory(const AdcFactory& other) = delete;
        AdcFactory& operator=(const AdcFactory& other) = delete;
        ~AdcFactory() = default;

    public:
        virtual std::size_t KeyPositionals() const = 0;
        virtual Status ParseKey(const Arguments& arguments, uint16_t& key) const = 0;
        virtual infra::MemoryRange<const char* const> OpenKeys() const = 0;
        virtual Status Prepare(uint16_t key, const Arguments& arguments) = 0;
        virtual Status Open(uint16_t key, const Arguments& arguments, PinOwner& pins, AdcHandle& handle) = 0;
        virtual void Close(uint16_t key, const infra::Function<void()>& onClosed) = 0;
    };

    class AdcCommands
        : public services::TerminalCommands
    {
    public:
        struct Slot
        {
            std::optional<uint16_t> key;
            bool closing = false;
            AdcHandle handle;
        };

        template<std::size_t Slots, std::size_t MaxValues>
        using WithCapacity = infra::WithStorage<infra::WithStorage<AdcCommands, std::array<Slot, Slots>>, std::array<uint16_t, MaxValues>>;

        AdcCommands(infra::MemoryRange<Slot> slots, infra::MemoryRange<uint16_t> values, Context& context, AdcFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    private:
        Status Open(const Arguments& arguments);
        Status Measure(const Arguments& arguments);
        Status Close(const Arguments& arguments);

        Status Allocate(uint16_t key, std::size_t& slot) const;
        Status OpenSlot(std::size_t slot, uint16_t key, const Arguments& arguments);
        Status Find(const Arguments& arguments, std::size_t& slot) const;
        void MeasureSynchronous(hal::SynchronousAdc& adc, uint32_t runs, std::size_t samplesPerRun);
        void MeasureAsynchronous(std::size_t slot, uint32_t runs);
        void Collect(hal::AdcMultiChannel::Samples samples);
        void Finish();
        void Timeout();
        void Report();
        PinOwner OwnerOf(std::size_t slot) const;

    private:
        infra::MemoryRange<Slot> slots;
        infra::MemoryRange<uint16_t> values;
        Context& context;
        AdcFactory& factory;
        std::atomic<std::size_t> valueCount{ 0 };
        std::atomic<uint32_t> runsRemaining{ 0 };
        std::optional<std::size_t> measuringSlot;
        infra::TimerSingleShot timer;
        std::array<Command, 3> commands;
    };
}

#endif
