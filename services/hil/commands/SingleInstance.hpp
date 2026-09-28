#ifndef SERVICES_HIL_SINGLE_INSTANCE_HPP
#define SERVICES_HIL_SINGLE_INSTANCE_HPP

#include "services/hil/Arguments.hpp"
#include "services/hil/Status.hpp"
#include <cstdint>
#include <optional>

namespace services::hil
{
    class SingleInstance
    {
    public:
        explicit SingleInstance(uint8_t instances);

        Status Parse(const Arguments& arguments, uint8_t& index) const;
        Status Find(const Arguments& arguments) const;

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
