#ifndef SERVICES_HIL_SINGLE_INSTANCE_HPP
#define SERVICES_HIL_SINGLE_INSTANCE_HPP

#include "services/hil/HilArguments.hpp"
#include "services/hil/HilStatus.hpp"
#include <cstdint>
#include <optional>

namespace services
{
    class HilSingleInstance
    {
    public:
        explicit HilSingleInstance(uint8_t instances);

        HilStatus Parse(const HilArguments& arguments, uint8_t& index) const;
        HilStatus Find(const HilArguments& arguments) const;

        bool Occupied() const;
        uint8_t Index() const;

        void Open(uint8_t index);
        void StartClosing();
        void Closed();

    private:
        uint8_t instances;
        std::optional<uint8_t> index;
        bool closing = false;
    };
}

#endif
