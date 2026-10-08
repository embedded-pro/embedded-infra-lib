#ifndef DRIVERS_CAMERA_OMNIVISION_REGISTER_TABLE_RUNNER_HPP
#define DRIVERS_CAMERA_OMNIVISION_REGISTER_TABLE_RUNNER_HPP

#include "drivers/camera/omnivision/RegisterTable.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/MemoryRange.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace drivers
{
    class RegisterTableRunner
    {
    public:
        explicit RegisterTableRunner(services::RegisterBusAccess& bus);
        RegisterTableRunner(const RegisterTableRunner&) = delete;
        RegisterTableRunner& operator=(const RegisterTableRunner&) = delete;
        ~RegisterTableRunner();

        void Run(infra::MemoryRange<const RegisterStep> steps, const infra::Function<void()>& onDone);
        bool Busy() const;

    private:
        void ExecuteCurrentStep();
        void WriteCurrentStep();
        void ModifyReadDone();
        void ModifyWriteDone();
        void NextStep();
        void Complete();

        services::RegisterBusAccess& bus;
        infra::TimerSingleShot timer;
        infra::MemoryRange<const RegisterStep> steps;
        std::size_t stepIndex{ 0 };
        infra::Function<void()> onDone;
        bool busy{ false };
        std::array<uint8_t, 1> valueBuffer{};
        std::array<uint8_t, 1> readBuffer{};
    };
}

#endif
