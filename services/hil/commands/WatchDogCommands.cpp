#include "services/hil/commands/WatchDogCommands.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services::hil
{
    namespace
    {
        constexpr uint32_t maximumTimeoutMs = 30000;

        constexpr std::array<Choice<bool>, 2> feedModes{ {
            { "auto", true },
            { "manual", false },
        } };
    }

    WatchDogCommands::WatchDogCommands(Context& context, WatchDogFactory& factory)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , factory(factory)
        , instance(factory.Instances())
        , commands{ {
              Bind<WatchDogCommands, &WatchDogCommands::Start>("wdt.start", "<index> timeout= [feed=] [key=value]...", *this, context.response),
              Bind<WatchDogCommands, &WatchDogCommands::Feed>("wdt.feed", "<index>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> WatchDogCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status WatchDogCommands::Start(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, factory.StartKeys()))
            return Status::usage;

        uint8_t index = 0;
        uint32_t timeout = 0;
        bool feed = true;
        Status status = Parse(arguments, index, timeout, feed);
        if (status != Status::done)
            return status;

        if (instance.Occupied())
            return Status::busy;

        return StartInstance(index, std::chrono::milliseconds(timeout), feed, arguments);
    }

    Status WatchDogCommands::StartInstance(uint8_t index, infra::Duration timeout, bool feed, const Arguments& arguments)
    {
        hal::Watchdog* created = nullptr;
        Status status = factory.Create(index, timeout, arguments, created);
        if (status != Status::done)
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
        return Status::done;
    }

    Status WatchDogCommands::Feed(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        Status status = instance.Find(arguments);
        if (status != Status::done)
            return status;

        watchDog->Refresh();
        context.response.Ok();
        return Status::done;
    }

    Status WatchDogCommands::Parse(const Arguments& arguments, uint8_t& index, uint32_t& timeout, bool& feed) const
    {
        Status status = instance.Parse(arguments, index);
        arguments.Number("timeout", timeout, 1, maximumTimeoutMs, status);
        if (status == Status::done)
            status = factory.Prepare(index, arguments);
        arguments.Select("feed", feed, feedModes, status);

        if (status == Status::done && !arguments.Has("timeout"))
            return Status::usage;

        return status;
    }

    void WatchDogCommands::EarlyWarning()
    {
        warnings.fetch_add(1, std::memory_order_relaxed);

        if (!reportPending.exchange(true))
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    Report();
                });
    }

    void WatchDogCommands::Report()
    {
        reportPending = false;

        if (autoFeed)
            watchDog->Refresh();

        context.response.Event("wdt") << " index=" << static_cast<uint32_t>(instance.Index()) << " warning=" << warnings.load();
    }
}
