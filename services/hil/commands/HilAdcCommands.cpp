#include "services/hil/commands/HilAdcCommands.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    namespace
    {
        constexpr infra::Duration measureTimeout = std::chrono::milliseconds(1000);
    }

    HilAdcCommands::HilAdcCommands(infra::MemoryRange<Slot> slots, infra::MemoryRange<uint16_t> values, HilContext& context, HilAdcFactory& factory)
        : services::TerminalCommands(context.terminal)
        , slots(slots)
        , values(values)
        , context(context)
        , factory(factory)
        , commands{ {
              HilBind<HilAdcCommands, &HilAdcCommands::Open>("adc.open", "<key>... [key=value]...", *this, context.response),
              HilBind<HilAdcCommands, &HilAdcCommands::Measure>("adc.measure", "<key>... [n=]", *this, context.response),
              HilBind<HilAdcCommands, &HilAdcCommands::Close>("adc.close", "<key>...", *this, context.response),
          } }
    {
        really_assert(HilOwners::adc + slots.size() <= HilOwners::extension);
    }

    infra::MemoryRange<const services::TerminalCommands::Command> HilAdcCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    HilStatus HilAdcCommands::Open(const HilArguments& arguments)
    {
        const auto positionals = factory.KeyPositionals();
        if (!arguments.Shape(positionals, positionals, factory.OpenKeys()))
            return HilStatus::usage;

        uint16_t key = 0;
        HilStatus status = factory.ParseKey(arguments, key);
        if (status == HilStatus::done)
            status = factory.Prepare(key, arguments);
        if (status != HilStatus::done)
            return status;

        std::size_t slot = 0;
        status = Allocate(key, slot);
        if (status != HilStatus::done)
            return status;

        return OpenSlot(slot, key, arguments);
    }

    HilStatus HilAdcCommands::Measure(const HilArguments& arguments)
    {
        const auto positionals = factory.KeyPositionals();
        if (!arguments.Shape(positionals, positionals, { "n" }))
            return HilStatus::usage;

        std::size_t slot = 0;
        uint32_t runs = 1;
        HilStatus status = Find(arguments, slot);
        arguments.Number("n", runs, 1, static_cast<uint32_t>(values.size()), status);
        if (status != HilStatus::done)
            return status;

        const auto& handle = slots[slot].handle;
        if (runs * handle.samplesPerRun > values.size())
            return HilStatus::range;

        if (measuringSlot.has_value())
            return HilStatus::busy;

        valueCount = 0;

        if (handle.synchronous != nullptr)
            MeasureSynchronous(*handle.synchronous, runs, handle.samplesPerRun);
        else
            MeasureAsynchronous(slot, runs);

        return HilStatus::done;
    }

    HilStatus HilAdcCommands::Close(const HilArguments& arguments)
    {
        const auto positionals = factory.KeyPositionals();
        if (!arguments.Shape(positionals, positionals, {}))
            return HilStatus::usage;

        std::size_t slot = 0;
        HilStatus status = Find(arguments, slot);
        if (status != HilStatus::done)
            return status;

        if (measuringSlot == slot)
        {
            slots[slot].handle.adc->Stop();
            runsRemaining = 0;
            measuringSlot = std::nullopt;
            timer.Cancel();
            context.response.Error(HilStatus::failed);
        }

        slots[slot].handle = HilAdcHandle{};
        slots[slot].closing = true;
        factory.Close(slot, *slots[slot].key, [this, slot]()
            {
                OwnerOf(slot).Release();
                slots[slot] = Slot{};
                context.response.Ok();
            });

        return HilStatus::done;
    }

    HilStatus HilAdcCommands::Allocate(uint16_t key, std::size_t& slot) const
    {
        std::optional<std::size_t> free;

        for (std::size_t i = 0; i != slots.size(); ++i)
        {
            if (slots[i].key == key)
                return HilStatus::busy;

            if (!slots[i].key.has_value() && !free.has_value())
                free = i;
        }

        if (!free.has_value())
            return HilStatus::busy;

        slot = *free;
        return HilStatus::done;
    }

    HilStatus HilAdcCommands::OpenSlot(std::size_t slot, uint16_t key, const HilArguments& arguments)
    {
        auto pins = OwnerOf(slot);
        HilAdcHandle handle;
        HilStatus status = factory.Open(slot, key, arguments, pins, handle);
        if (status != HilStatus::done)
        {
            pins.Release();
            return status;
        }

        really_assert(handle.adc != nullptr || handle.synchronous != nullptr);

        slots[slot].key = key;
        slots[slot].handle = handle;
        context.response.Ok();
        return HilStatus::done;
    }

    HilStatus HilAdcCommands::Find(const HilArguments& arguments, std::size_t& slot) const
    {
        uint16_t key = 0;
        HilStatus status = factory.ParseKey(arguments, key);
        if (status != HilStatus::done)
            return status;

        for (std::size_t i = 0; i != slots.size(); ++i)
            if (slots[i].key == key && !slots[i].closing)
            {
                slot = i;
                return HilStatus::done;
            }

        return HilStatus::notOpen;
    }

    void HilAdcCommands::MeasureSynchronous(hal::SynchronousAdc& adc, uint32_t runs, std::size_t samplesPerRun)
    {
        std::size_t count = 0;

        for (uint32_t run = 0; run != runs; ++run)
            for (auto sample : adc.Measure(samplesPerRun))
                if (count != values.size())
                    values[count++] = sample;

        valueCount = count;
        Report();
    }

    void HilAdcCommands::MeasureAsynchronous(std::size_t slot, uint32_t runs)
    {
        measuringSlot = slot;
        runsRemaining = runs;
        timer.Start(measureTimeout, [this]()
            {
                Timeout();
            });

        slots[slot].handle.adc->Measure([this](hal::AdcMultiChannel::Samples samples)
            {
                Collect(samples);
            });
    }

    void HilAdcCommands::Collect(hal::AdcMultiChannel::Samples samples)
    {
        if (runsRemaining == 0)
            return;

        for (auto sample : samples)
        {
            auto position = valueCount.load();
            if (position < values.size())
            {
                values[position] = sample;
                valueCount = position + 1;
            }
        }

        if (--runsRemaining == 0)
        {
            slots[*measuringSlot].handle.adc->Stop();
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    Finish();
                });
        }
    }

    void HilAdcCommands::Finish()
    {
        if (!measuringSlot.has_value())
            return;

        measuringSlot = std::nullopt;
        timer.Cancel();
        Report();
    }

    void HilAdcCommands::Timeout()
    {
        if (!measuringSlot.has_value())
            return;

        runsRemaining = 0;
        slots[*measuringSlot].handle.adc->Stop();
        measuringSlot = std::nullopt;
        context.response.Error(HilStatus::timeout);
    }

    void HilAdcCommands::Report()
    {
        auto line = context.response.Ok();
        line << " samples=";

        for (std::size_t i = 0; i != valueCount; ++i)
            line << (i == 0 ? "" : ",") << static_cast<uint32_t>(values[i]);
    }

    HilPinOwner HilAdcCommands::OwnerOf(std::size_t slot) const
    {
        return HilPinOwner(context.pins, static_cast<HilOwner>(HilOwners::adc + slot));
    }
}
