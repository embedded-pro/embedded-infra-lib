#include "services/hil/commands/AdcCommands.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services::hil
{
    namespace
    {
        constexpr infra::Duration measureTimeout = std::chrono::milliseconds(1000);
    }

    AdcCommands::AdcCommands(infra::MemoryRange<Slot> slots, infra::MemoryRange<uint16_t> values, Context& context, AdcFactory& factory)
        : services::TerminalCommands(context.terminal)
        , slots(slots)
        , values(values)
        , context(context)
        , factory(factory)
        , commands{ {
              Bind<AdcCommands, &AdcCommands::Open>("adc.open", "<key>... [key=value]...", *this, context.response),
              Bind<AdcCommands, &AdcCommands::Measure>("adc.measure", "<key>... [n=]", *this, context.response),
              Bind<AdcCommands, &AdcCommands::Close>("adc.close", "<key>...", *this, context.response),
          } }
    {
        really_assert(owner::adc + slots.size() <= owner::extension);
    }

    infra::MemoryRange<const services::TerminalCommands::Command> AdcCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status AdcCommands::Open(const Arguments& arguments)
    {
        const auto positionals = factory.KeyPositionals();
        if (!arguments.Shape(positionals, positionals, factory.OpenKeys()))
            return Status::usage;

        uint16_t key = 0;
        Status status = factory.ParseKey(arguments, key);
        if (status == Status::done)
            status = factory.Prepare(key, arguments);
        if (status != Status::done)
            return status;

        std::size_t slot = 0;
        status = Allocate(key, slot);
        if (status != Status::done)
            return status;

        return OpenSlot(slot, key, arguments);
    }

    Status AdcCommands::Measure(const Arguments& arguments)
    {
        const auto positionals = factory.KeyPositionals();
        if (!arguments.Shape(positionals, positionals, { "n" }))
            return Status::usage;

        std::size_t slot = 0;
        uint32_t runs = 1;
        Status status = Find(arguments, slot);
        arguments.Number("n", runs, 1, static_cast<uint32_t>(values.size()), status);
        if (status != Status::done)
            return status;

        const auto& handle = slots[slot].handle;
        if (runs * handle.samplesPerRun > values.size())
            return Status::range;

        if (measuringSlot)
            return Status::busy;

        valueCount = 0;

        if (handle.synchronous != nullptr)
            MeasureSynchronous(*handle.synchronous, runs, handle.samplesPerRun);
        else
            MeasureAsynchronous(slot, runs);

        return Status::done;
    }

    Status AdcCommands::Close(const Arguments& arguments)
    {
        const auto positionals = factory.KeyPositionals();
        if (!arguments.Shape(positionals, positionals, {}))
            return Status::usage;

        std::size_t slot = 0;
        Status status = Find(arguments, slot);
        if (status != Status::done)
            return status;

        if (measuringSlot == slot)
        {
            slots[slot].handle.adc->Stop();
            runsRemaining = 0;
            measuringSlot = std::nullopt;
            timer.Cancel();
            context.response.Error(Status::failed);
        }

        slots[slot].handle = AdcHandle{};
        slots[slot].closing = true;
        factory.Close(slot, *slots[slot].key, [this, slot]()
            {
                OwnerOf(slot).Release();
                slots[slot] = Slot{};
                context.response.Ok();
            });

        return Status::done;
    }

    Status AdcCommands::Allocate(uint16_t key, std::size_t& slot) const
    {
        std::optional<std::size_t> free;

        for (std::size_t i = 0; i != slots.size(); ++i)
        {
            if (slots[i].key == key)
                return Status::busy;

            if (!slots[i].key && !free)
                free = i;
        }

        if (!free)
            return Status::busy;

        slot = *free;
        return Status::done;
    }

    Status AdcCommands::OpenSlot(std::size_t slot, uint16_t key, const Arguments& arguments)
    {
        auto pins = OwnerOf(slot);
        AdcHandle handle;
        Status status = factory.Open(slot, key, arguments, pins, handle);
        if (status != Status::done)
        {
            pins.Release();
            return status;
        }

        really_assert(handle.adc != nullptr || handle.synchronous != nullptr);

        slots[slot].key = key;
        slots[slot].handle = handle;
        context.response.Ok();
        return Status::done;
    }

    Status AdcCommands::Find(const Arguments& arguments, std::size_t& slot) const
    {
        uint16_t key = 0;
        Status status = factory.ParseKey(arguments, key);
        if (status != Status::done)
            return status;

        for (std::size_t i = 0; i != slots.size(); ++i)
            if (slots[i].key == key && !slots[i].closing)
            {
                slot = i;
                return Status::done;
            }

        return Status::notOpen;
    }

    void AdcCommands::MeasureSynchronous(hal::SynchronousAdc& adc, uint32_t runs, std::size_t samplesPerRun)
    {
        std::size_t count = 0;

        for (uint32_t run = 0; run != runs; ++run)
            for (auto sample : adc.Measure(samplesPerRun))
                if (count != values.size())
                    values[count++] = sample;

        valueCount = count;
        Report();
    }

    void AdcCommands::MeasureAsynchronous(std::size_t slot, uint32_t runs)
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

    void AdcCommands::Collect(hal::AdcMultiChannel::Samples samples)
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

    void AdcCommands::Finish()
    {
        if (!measuringSlot)
            return;

        measuringSlot = std::nullopt;
        timer.Cancel();
        Report();
    }

    void AdcCommands::Timeout()
    {
        if (!measuringSlot)
            return;

        runsRemaining = 0;
        slots[*measuringSlot].handle.adc->Stop();
        measuringSlot = std::nullopt;
        context.response.Error(Status::timeout);
    }

    void AdcCommands::Report()
    {
        auto line = context.response.Ok();
        line << " samples=";

        for (std::size_t i = 0; i != valueCount; ++i)
            line << (i == 0 ? "" : ",") << static_cast<uint32_t>(values[i]);
    }

    PinOwner AdcCommands::OwnerOf(std::size_t slot) const
    {
        return PinOwner(context.pins, static_cast<Owner>(owner::adc + slot));
    }
}
