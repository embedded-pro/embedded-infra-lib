#ifndef SERVICES_HIL_PIN_POOL_HPP
#define SERVICES_HIL_PIN_POOL_HPP

#include "hal/interfaces/Gpio.hpp"
#include "infra/util/BoundedString.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/WithStorage.hpp"
#include "services/hil/HilPinId.hpp"
#include "services/hil/HilStatus.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace services
{
    using HilOwner = uint8_t;

    struct HilOwners
    {
        static constexpr HilOwner gpio = 0;
        static constexpr HilOwner pwm = 1;
        static constexpr HilOwner uart = 2;
        static constexpr HilOwner spi = 3;
        static constexpr HilOwner comparator = 4;
        static constexpr HilOwner qei = 5;
        static constexpr HilOwner can = 6;
        static constexpr HilOwner adc = 8;
        static constexpr HilOwner extension = 16;
        static constexpr HilOwner last = 31;
    };

    struct HilPinOptions
    {
        HilPull pull = HilPull::none;
        bool openDrain = false;
        uint8_t drive = 0;
    };

    class HilPinFactory
    {
    protected:
        HilPinFactory() = default;
        HilPinFactory(const HilPinFactory& other) = delete;
        HilPinFactory& operator=(const HilPinFactory& other) = delete;
        ~HilPinFactory() = default;

    public:
        virtual bool IsValid(HilPinId pin) const = 0;
        virtual bool SupportsFunction(HilPinId pin, uint16_t function, uint8_t instance) const = 0;
        virtual bool SupportsAnalog(HilPinId pin) const = 0;
        virtual bool SupportsInterrupt(HilPinId pin) const = 0;
        virtual std::optional<uint8_t> ParseDrive(infra::BoundedConstString text) const = 0;

        virtual hal::GpioPin& Construct(std::size_t slot, HilPinId pin, const HilPinOptions& options) = 0;
        virtual void Destroy(std::size_t slot) = 0;
    };

    class HilPinPool
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
            HilPinId id;
            Use use = Use::exclusive;
        };

        template<std::size_t Capacity>
        using WithCapacity = infra::WithStorage<HilPinPool, std::array<Slot, Capacity>>;

        HilPinPool(infra::MemoryRange<Slot> slots, HilPinFactory& factory, infra::MemoryRange<const HilPinId> reserved = {});
        HilPinPool(const HilPinPool& other) = delete;
        HilPinPool& operator=(const HilPinPool& other) = delete;
        ~HilPinPool() = default;

        HilPinFactory& Factory() const;

        HilStatus Claim(HilPinId id, HilOwner owner, Use use, hal::GpioPin*& pin, const HilPinOptions& options = {});
        HilStatus ClaimFunction(HilPinId id, HilOwner owner, uint16_t function, uint8_t instance, hal::GpioPin*& pin);
        HilStatus ClaimFunction(const std::optional<HilPinId>& id, HilOwner owner, uint16_t function, uint8_t instance, hal::GpioPin*& pin);
        HilStatus ClaimAnalog(HilPinId id, HilOwner owner, hal::GpioPin*& pin);
        void Release(HilOwner owner);
        void Release(HilPinId id, HilOwner owner);

    private:
        bool IsReserved(HilPinId id) const;
        Slot* Occupied(HilPinId id);
        Slot* Free();
        void Release(Slot& slot, uint32_t bit);

    private:
        infra::MemoryRange<Slot> slots;
        HilPinFactory& factory;
        infra::MemoryRange<const HilPinId> reserved;
    };

    class HilPinOwner
    {
    public:
        HilPinOwner(HilPinPool& pool, HilOwner owner);
        HilPinOwner(const HilPinOwner& other) = delete;
        HilPinOwner& operator=(const HilPinOwner& other) = delete;
        ~HilPinOwner() = default;

        HilPinPool& Pool() const;
        HilOwner Id() const;

        HilStatus Claim(HilPinId id, HilPinPool::Use use, hal::GpioPin*& pin, const HilPinOptions& options = {});
        HilStatus ClaimFunction(HilPinId id, uint16_t function, uint8_t instance, hal::GpioPin*& pin);
        HilStatus ClaimFunction(const std::optional<HilPinId>& id, uint16_t function, uint8_t instance, hal::GpioPin*& pin);
        HilStatus ClaimAnalog(HilPinId id, hal::GpioPin*& pin);
        void Release();
        void Release(HilPinId id);

    private:
        HilPinPool& pool;
        HilOwner owner;
    };
}

#endif
