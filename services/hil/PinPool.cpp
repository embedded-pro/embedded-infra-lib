#include "services/hil/PinPool.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services::hil
{
    namespace
    {
        uint32_t Bit(Owner owner)
        {
            really_assert(owner <= owner::last);
            return uint32_t{ 1 } << owner;
        }
    }

    PinPool::PinPool(infra::MemoryRange<Slot> slots, PinFactory& factory, infra::MemoryRange<const PinId> reserved)
        : slots(slots)
        , factory(factory)
        , reserved(reserved)
    {}

    PinFactory& PinPool::Factory() const
    {
        return factory;
    }

    Status PinPool::Claim(PinId id, Owner owner, Use use, hal::GpioPin*& pin, const PinOptions& options)
    {
        if (!factory.IsValid(id))
            return Status::pin;

        if (IsReserved(id))
            return Status::busy;

        if (auto slot = Occupied(id))
        {
            if (use != Use::analog || slot->use != Use::analog)
                return Status::busy;

            slot->owners |= Bit(owner);
            pin = slot->pin;
            return Status::done;
        }

        auto slot = Free();
        if (slot == nullptr)
            return Status::busy;

        slot->id = id;
        slot->owners = Bit(owner);
        slot->use = use;
        slot->pin = &factory.Construct(static_cast<std::size_t>(slot - slots.begin()), id, options);
        pin = slot->pin;
        return Status::done;
    }

    Status PinPool::ClaimFunction(PinId id, Owner owner, uint16_t function, uint8_t instance, hal::GpioPin*& pin)
    {
        if (!factory.SupportsFunction(id, function, instance))
            return Status::pin;

        return Claim(id, owner, Use::exclusive, pin);
    }

    Status PinPool::ClaimFunction(const std::optional<PinId>& id, Owner owner, uint16_t function, uint8_t instance, hal::GpioPin*& pin)
    {
        pin = nullptr;

        if (!id)
            return Status::done;

        return ClaimFunction(*id, owner, function, instance, pin);
    }

    Status PinPool::ClaimAnalog(PinId id, Owner owner, hal::GpioPin*& pin)
    {
        if (!factory.SupportsAnalog(id))
            return Status::pin;

        return Claim(id, owner, Use::analog, pin);
    }

    void PinPool::Release(Owner owner)
    {
        const auto bit = Bit(owner);

        for (auto& slot : slots)
            if ((slot.owners & bit) != 0)
                Release(slot, bit);
    }

    void PinPool::Release(PinId id, Owner owner)
    {
        const auto bit = Bit(owner);

        for (auto& slot : slots)
            if ((slot.owners & bit) != 0 && slot.id == id)
                Release(slot, bit);
    }

    bool PinPool::IsReserved(PinId id) const
    {
        for (const auto& pin : reserved)
            if (pin == id)
                return true;

        return false;
    }

    PinPool::Slot* PinPool::Occupied(PinId id)
    {
        for (auto& slot : slots)
            if (slot.owners != 0 && slot.id == id)
                return &slot;

        return nullptr;
    }

    PinPool::Slot* PinPool::Free()
    {
        for (auto& slot : slots)
            if (slot.owners == 0)
                return &slot;

        return nullptr;
    }

    void PinPool::Release(Slot& slot, uint32_t bit)
    {
        slot.owners &= ~bit;

        if (slot.owners == 0)
        {
            slot.pin = nullptr;
            factory.Destroy(static_cast<std::size_t>(&slot - slots.begin()));
        }
    }

    PinOwner::PinOwner(PinPool& pool, Owner owner)
        : pool(pool)
        , owner(owner)
    {}

    PinPool& PinOwner::Pool() const
    {
        return pool;
    }

    Owner PinOwner::Id() const
    {
        return owner;
    }

    Status PinOwner::Claim(PinId id, PinPool::Use use, hal::GpioPin*& pin, const PinOptions& options)
    {
        return pool.Claim(id, owner, use, pin, options);
    }

    Status PinOwner::ClaimFunction(PinId id, uint16_t function, uint8_t instance, hal::GpioPin*& pin)
    {
        return pool.ClaimFunction(id, owner, function, instance, pin);
    }

    Status PinOwner::ClaimFunction(const std::optional<PinId>& id, uint16_t function, uint8_t instance, hal::GpioPin*& pin)
    {
        return pool.ClaimFunction(id, owner, function, instance, pin);
    }

    Status PinOwner::ClaimAnalog(PinId id, hal::GpioPin*& pin)
    {
        return pool.ClaimAnalog(id, owner, pin);
    }

    void PinOwner::Release()
    {
        pool.Release(owner);
    }

    void PinOwner::Release(PinId id)
    {
        pool.Release(id, owner);
    }
}
