#include "hal/interfaces/test_doubles/DisplayControllerStub.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace hal
{
    DisplayControllerStub::DisplayControllerStub(DisplaySize size, std::size_t numberOfLayers)
        : size(size)
        , numberOfLayers(numberOfLayers)
    {
        really_assert(numberOfLayers <= maxLayers);

        ON_CALL(*this, Size).WillByDefault([this]()
            {
                return this->size;
            });

        ON_CALL(*this, NumberOfLayers).WillByDefault([this]()
            {
                return this->numberOfLayers;
            });

        ON_CALL(*this, Start).WillByDefault([this](const infra::Function<void()>& onVerticalBlank, const infra::Function<void()>& onUnderrun)
            {
                started = true;
                this->onVerticalBlank = onVerticalBlank;
                this->onUnderrun = onUnderrun;
            });

        ON_CALL(*this, Stop).WillByDefault([this]()
            {
                started = false;
                onVerticalBlank = nullptr;
                onUnderrun = nullptr;
            });

        ON_CALL(*this, ConfigureLayer).WillByDefault([this](std::size_t layer, const DisplayLayer& configuration)
            {
                CheckLayer(layer);
                really_assert(IsValidSurface(configuration.framebuffer));
                staged[layer] = configuration;
            });

        ON_CALL(*this, SetFramebuffer).WillByDefault([this](std::size_t layer, infra::ByteRange framebuffer)
            {
                CheckLayer(layer);
                really_assert(staged[layer]);
                staged[layer]->framebuffer.memory = framebuffer;
            });

        ON_CALL(*this, DisableLayer).WillByDefault([this](std::size_t layer)
            {
                CheckLayer(layer);
                staged[layer] = std::nullopt;
            });

        ON_CALL(*this, Commit).WillByDefault([this](const infra::Function<void()>& onApplied)
            {
                really_assert(!CommitPending());
                this->onApplied = onApplied;
            });

        ON_CALL(*this, SetPalette).WillByDefault([this](std::size_t layer, infra::MemoryRange<const Argb8888> palette)
            {
                CheckLayer(layer);
                palettes[layer] = palette;
            });
    }

    void DisplayControllerStub::VerticalBlank()
    {
        if (onVerticalBlank)
        {
            auto callback = onVerticalBlank;
            callback();
        }
    }

    void DisplayControllerStub::Underrun()
    {
        if (onUnderrun)
        {
            auto callback = onUnderrun;
            callback();
        }
    }

    void DisplayControllerStub::CompleteCommit()
    {
        really_assert(CommitPending());

        applied = staged;
        onApplied();
    }

    bool DisplayControllerStub::Started() const
    {
        return started;
    }

    bool DisplayControllerStub::CommitPending() const
    {
        return static_cast<bool>(onApplied);
    }

    const std::optional<DisplayLayer>& DisplayControllerStub::StagedLayer(std::size_t layer) const
    {
        CheckLayer(layer);
        return staged[layer];
    }

    const std::optional<DisplayLayer>& DisplayControllerStub::AppliedLayer(std::size_t layer) const
    {
        CheckLayer(layer);
        return applied[layer];
    }

    infra::MemoryRange<const Argb8888> DisplayControllerStub::Palette(std::size_t layer) const
    {
        CheckLayer(layer);
        return palettes[layer];
    }

    void DisplayControllerStub::CheckLayer(std::size_t layer) const
    {
        really_assert(layer < numberOfLayers);
    }
}
