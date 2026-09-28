#ifndef SERVICES_HIL_ADC_COMMANDS_HPP
#define SERVICES_HIL_ADC_COMMANDS_HPP

#include "hal/interfaces/AdcMultiChannel.hpp"
#include "hal/synchronous_interfaces/SynchronousAdc.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/WithStorage.hpp"
#include "services/hil/HilCommand.hpp"
#include "services/util/Terminal.hpp"
#include <array>
#include <atomic>
#include <optional>

namespace services
{
    struct HilAdcHandle
    {
        hal::AdcMultiChannel* adc = nullptr;
        hal::SynchronousAdc* synchronous = nullptr;
        std::size_t samplesPerRun = 0;
    };

    class HilAdcFactory
    {
    protected:
        HilAdcFactory() = default;
        HilAdcFactory(const HilAdcFactory& other) = delete;
        HilAdcFactory& operator=(const HilAdcFactory& other) = delete;
        ~HilAdcFactory() = default;

    public:
        virtual std::size_t KeyPositionals() const = 0;
        virtual HilStatus ParseKey(const HilArguments& arguments, uint16_t& key) const = 0;
        virtual infra::MemoryRange<const char* const> OpenKeys() const = 0;
        virtual HilStatus Prepare(uint16_t key, const HilArguments& arguments) = 0;
        virtual HilStatus Open(std::size_t slot, uint16_t key, const HilArguments& arguments, HilPinOwner& pins, HilAdcHandle& handle) = 0;
        virtual void Close(std::size_t slot, uint16_t key, const infra::Function<void()>& onClosed) = 0;
    };

    class HilAdcCommands
        : public services::TerminalCommands
    {
    public:
        struct Slot
        {
            std::optional<uint16_t> key;
            bool closing = false;
            HilAdcHandle handle;
        };

        template<std::size_t Slots, std::size_t MaxValues>
        using WithCapacity = infra::WithStorage<infra::WithStorage<HilAdcCommands, std::array<Slot, Slots>>, std::array<uint16_t, MaxValues>>;

        HilAdcCommands(infra::MemoryRange<Slot> slots, infra::MemoryRange<uint16_t> values, HilContext& context, HilAdcFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    private:
        HilStatus Open(const HilArguments& arguments);
        HilStatus Measure(const HilArguments& arguments);
        HilStatus Close(const HilArguments& arguments);

        HilStatus Allocate(uint16_t key, std::size_t& slot) const;
        HilStatus OpenSlot(std::size_t slot, uint16_t key, const HilArguments& arguments);
        HilStatus Find(const HilArguments& arguments, std::size_t& slot) const;
        void MeasureSynchronous(hal::SynchronousAdc& adc, uint32_t runs, std::size_t samplesPerRun);
        void MeasureAsynchronous(std::size_t slot, uint32_t runs);
        void Collect(hal::AdcMultiChannel::Samples samples);
        void Finish();
        void Timeout();
        void Report();
        HilPinOwner OwnerOf(std::size_t slot) const;

    private:
        infra::MemoryRange<Slot> slots;
        infra::MemoryRange<uint16_t> values;
        HilContext& context;
        HilAdcFactory& factory;
        std::atomic<std::size_t> valueCount{ 0 };
        std::atomic<uint32_t> runsRemaining{ 0 };
        std::optional<std::size_t> measuringSlot;
        infra::TimerSingleShot timer;
        std::array<Command, 3> commands;
    };
}

#endif
