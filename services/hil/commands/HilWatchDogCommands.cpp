#include "services/hil/commands/HilWatchDogCommands.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    namespace
    {
        constexpr uint32_t maximumTimeoutMs = 30000;

        constexpr std::array<HilChoice<bool>, 2> feedModes{ {
            { "auto", true },
            { "manual", false },
        } };
    }

    HilWatchDogCommands::HilWatchDogCommands(HilContext& context, HilWatchDogFactory& factory)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , factory(factory)
        , instance(factory.Instances())
        , commands{ {
              HilBind<HilWatchDogCommands, &HilWatchDogCommands::Start>("wdt.start", "<index> timeout= [feed=] [key=value]...", *this, context.response),
              HilBind<HilWatchDogCommands, &HilWatchDogCommands::Feed>("wdt.feed", "<index>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> HilWatchDogCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    HilStatus HilWatchDogCommands::Start(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, factory.StartKeys()))
            return HilStatus::usage;

        uint8_t index = 0;
        uint32_t timeout = 0;
        bool feed = true;
        HilStatus status = Parse(arguments, index, timeout, feed);
        if (status != HilStatus::done)
            return status;

        if (instance.Occupied())
            return HilStatus::busy;

        return StartInstance(index, std::chrono::milliseconds(timeout), feed, arguments);
    }

    HilStatus HilWatchDogCommands::StartInstance(uint8_t index, infra::Duration timeout, bool feed, const HilArguments& arguments)
    {
        hal::Watchdog* created = nullptr;
        HilStatus status = factory.Create(index, timeout, arguments, created);
        if (status != HilStatus::done)
            return status;

        really_assert(created != nullptr);

        watchDog = created;
        autoFeed = feed;
        warnings = 0;
        instance.Open(index);
        watchDog->Start([this]()
            {
                EarlyWarning();
            });

        context.response.Ok();
        return HilStatus::done;
    }

    HilStatus HilWatchDogCommands::Feed(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return HilStatus::usage;

        HilStatus status = instance.Find(arguments);
        if (status != HilStatus::done)
            return status;

        watchDog->Refresh();
        context.response.Ok();
        return HilStatus::done;
    }

    HilStatus HilWatchDogCommands::Parse(const HilArguments& arguments, uint8_t& index, uint32_t& timeout, bool& feed) const
    {
        HilStatus status = instance.Parse(arguments, index);
        arguments.Number("timeout", timeout, 1, maximumTimeoutMs, status);
        if (status == HilStatus::done)
            status = factory.Prepare(index, arguments);
        arguments.Select("feed", feed, feedModes, status);

        if (status == HilStatus::done && !arguments.Has("timeout"))
            return HilStatus::usage;

        return status;
    }

    void HilWatchDogCommands::EarlyWarning()
    {
        warnings.fetch_add(1);
        reportScheduler.Schedule([this]()
            {
                Report();
            });
    }

    void HilWatchDogCommands::Report()
    {
        if (autoFeed)
            watchDog->Refresh();

        context.response.Event("wdt") << " index=" << static_cast<uint32_t>(instance.Index()) << " warning=" << warnings.load();
    }
}
