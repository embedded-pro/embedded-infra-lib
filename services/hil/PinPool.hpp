#ifndef SERVICES_HIL_PIN_POOL_HPP
#define SERVICES_HIL_PIN_POOL_HPP

#include "hal/interfaces/Gpio.hpp"
#include "infra/util/BoundedString.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/WithStorage.hpp"
#include "services/hil/PinId.hpp"
#include "services/hil/Status.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace services::hil
{
    using Owner = uint8_t;

    namespace owner
    {
        constexpr Owner gpio = 0;
        constexpr Owner pwm = 1;
        constexpr Owner uart = 2;
        constexpr Owner spi = 3;
        constexpr Owner comparator = 4;
        constexpr Owner qei = 5;
        constexpr Owner can = 6;
        constexpr Owner adc = 8;
        constexpr Owner extension = 16;
        constexpr Owner last = 31;
    }

    struct PinOptions
    {
        Pull pull = Pull::none;
        bool openDrain = false;
        uint8_t drive = 0;
    };

    class PinFactory
    {
    protected:
        PinFactory() = default;
        PinFactory(const PinFactory& other) = delete;
        PinFactory& operator=(const PinFactory& other) = delete;
        ~PinFactory() = default;

    public:
        virtual bool IsValid(PinId pin) const = 0;
        virtual bool SupportsFunction(PinId pin, uint16_t function, uint8_t instance) const = 0;
        virtual bool SupportsAnalog(PinId pin) const = 0;
        virtual bool SupportsInterrupt(PinId pin) const = 0;
        virtual std::optional<uint8_t> ParseDrive(infra::BoundedConstString text) const = 0;

        virtual hal::GpioPin& Construct(std::size_t slot, PinId pin, const PinOptions& options) = 0;
        virtual void Destroy(std::size_t slot) = 0;
    };

    class PinPool
    {
    public:
        enum class Use : uint8_t
        {
            exclusive,
            analog,
        };

        struct Slot
        {
            hal::GpioPin* pin = nullptr;
            uint32_t owners = 0;
            PinId id;
            Use use = Use::exclusive;
        };

        template<std::size_t Capacity>
        using WithCapacity = infra::WithStorage<PinPool, std::array<Slot, Capacity>>;

        PinPool(infra::MemoryRange<Slot> slots, PinFactory& factory, infra::MemoryRange<const PinId> reserved = {});
        PinPool(const PinPool& other) = delete;
        PinPool& operator=(const PinPool& other) = delete;
        ~PinPool() = default;

        PinFactory& Factory() const;

        Status Claim(PinId id, Owner owner, Use use, hal::GpioPin*& pin, const PinOptions& options = {});
        Status ClaimFunction(PinId id, Owner owner, uint16_t function, uint8_t instance, hal::GpioPin*& pin);
        Status ClaimFunction(const std::optional<PinId>& id, Owner owner, uint16_t function, uint8_t instance, hal::GpioPin*& pin);
        Status ClaimAnalog(PinId id, Owner owner, hal::GpioPin*& pin);
        void Release(Owner owner);
        void Release(PinId id, Owner owner);

    private:
        bool IsReserved(PinId id) const;
        Slot* Occupied(PinId id);
        Slot* Free();
        void Release(Slot& slot, uint32_t bit);

    private:
        infra::MemoryRange<Slot> slots;
        PinFactory& factory;
        infra::MemoryRange<const PinId> reserved;
    };

    class PinOwner
    {
    public:
        PinOwner(PinPool& pool, Owner owner);
        PinOwner(const PinOwner& other) = delete;
        PinOwner& operator=(const PinOwner& other) = delete;
        ~PinOwner() = default;

        PinPool& Pool() const;
        Owner Id() const;

        Status Claim(PinId id, PinPool::Use use, hal::GpioPin*& pin, const PinOptions& options = {});
        Status ClaimFunction(PinId id, uint16_t function, uint8_t instance, hal::GpioPin*& pin);
        Status ClaimFunction(const std::optional<PinId>& id, uint16_t function, uint8_t instance, hal::GpioPin*& pin);
        Status ClaimAnalog(PinId id, hal::GpioPin*& pin);
        void Release();
        void Release(PinId id);

    private:
        PinPool& pool;
        Owner owner;
    };
}

#endif
