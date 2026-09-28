#include "services/hil/HilPinPool.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace services
{
    namespace
    {
        uint32_t Bit(HilOwner owner)
        {
            really_assert(owner <= HilOwners::last);
            return uint32_t{ 1 } << owner;
        }
    }

    HilPinPool::HilPinPool(infra::MemoryRange<Slot> slots, HilPinFactory& factory, infra::MemoryRange<const HilPinId> reserved)
        : slots(slots)
        , factory(factory)
        , reserved(reserved)
    {}

    HilPinFactory& HilPinPool::Factory() const
    {
        return factory;
    }

    HilStatus HilPinPool::Claim(HilPinId id, HilOwner owner, Use use, hal::GpioPin*& pin, const HilPinOptions& options)
    {
        if (!factory.IsValid(id))
            return HilStatus::pin;

        if (IsReserved(id))
            return HilStatus::busy;

        if (auto slot = Occupied(id))
        {
            if (use != Use::analog || slot->use != Use::analog)
                return HilStatus::busy;

            slot->owners |= Bit(owner);
            pin = slot->pin;
            return HilStatus::done;
        }

        auto slot = Free();
        if (slot == nullptr)
            return HilStatus::busy;

        slot->id = id;
        slot->owners = Bit(owner);
        slot->use = use;
        slot->pin = &factory.Construct(static_cast<std::size_t>(slot - slots.begin()), id, options);
        pin = slot->pin;
        return HilStatus::done;
    }

    HilStatus HilPinPool::ClaimFunction(HilPinId id, HilOwner owner, uint16_t function, uint8_t instance, hal::GpioPin*& pin)
    {
        if (!factory.SupportsFunction(id, function, instance))
            return HilStatus::pin;

        return Claim(id, owner, Use::exclusive, pin);
    }

    HilStatus HilPinPool::ClaimFunction(const std::optional<HilPinId>& id, HilOwner owner, uint16_t function, uint8_t instance, hal::GpioPin*& pin)
    {
        pin = nullptr;

        if (!id)
            return HilStatus::done;

        return ClaimFunction(*id, owner, function, instance, pin);
    }

    HilStatus HilPinPool::ClaimAnalog(HilPinId id, HilOwner owner, hal::GpioPin*& pin)
    {
        if (!factory.SupportsAnalog(id))
            return HilStatus::pin;

        return Claim(id, owner, Use::analog, pin);
    }

    void HilPinPool::Release(HilOwner owner)
    {
        const auto bit = Bit(owner);

        for (auto& slot : slots)
            if ((slot.owners & bit) != 0)
                Release(slot, bit);
    }

    void HilPinPool::Release(HilPinId id, HilOwner owner)
    {
        const auto bit = Bit(owner);

        for (auto& slot : slots)
            if ((slot.owners & bit) != 0 && slot.id == id)
                Release(slot, bit);
    }

    bool HilPinPool::IsReserved(HilPinId id) const
    {
        return std::ranges::any_of(reserved, [id](const HilPinId& pin)
            {
                return pin == id;
            });
    }

    HilPinPool::Slot* HilPinPool::Occupied(HilPinId id)
    {
        for (auto& slot : slots)
            if (slot.owners != 0 && slot.id == id)
                return &slot;

        return nullptr;
    }

    HilPinPool::Slot* HilPinPool::Free()
    {
        for (auto& slot : slots)
            if (slot.owners == 0)
                return &slot;

        return nullptr;
    }

    void HilPinPool::Release(Slot& slot, uint32_t bit)
    {
        slot.owners &= ~bit;

        if (slot.owners == 0)
        {
            slot.pin = nullptr;
            factory.Destroy(static_cast<std::size_t>(&slot - slots.begin()));
        }
    }

    HilPinOwner::HilPinOwner(HilPinPool& pool, HilOwner owner)
        : pool(pool)
        , owner(owner)
    {}

    HilPinPool& HilPinOwner::Pool() const
    {
        return pool;
    }

    HilOwner HilPinOwner::Id() const
    {
        return owner;
    }

    HilStatus HilPinOwner::Claim(HilPinId id, HilPinPool::Use use, hal::GpioPin*& pin, const HilPinOptions& options)
    {
        return pool.Claim(id, owner, use, pin, options);
    }

    HilStatus HilPinOwner::ClaimFunction(HilPinId id, uint16_t function, uint8_t instance, hal::GpioPin*& pin)
    {
        return pool.ClaimFunction(id, owner, function, instance, pin);
    }

    HilStatus HilPinOwner::ClaimFunction(const std::optional<HilPinId>& id, uint16_t function, uint8_t instance, hal::GpioPin*& pin)
    {
        return pool.ClaimFunction(id, owner, function, instance, pin);
    }

    HilStatus HilPinOwner::ClaimAnalog(HilPinId id, hal::GpioPin*& pin)
    {
        return pool.ClaimAnalog(id, owner, pin);
    }

    void HilPinOwner::Release()
    {
        pool.Release(owner);
    }

    void HilPinOwner::Release(HilPinId id)
    {
        pool.Release(id, owner);
    }
}
