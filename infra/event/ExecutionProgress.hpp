#ifndef INFRA_EXECUTION_PROGRESS_HPP
#define INFRA_EXECUTION_PROGRESS_HPP

#include <atomic>
#include <cstdint>

namespace infra
{
    // Steps advance when an action starts and when it finishes, so an odd count means an action is executing.
    // Only the thread running the event dispatcher writes, and interrupts such as a watchdog's early warning
    // read, so every update is a plain store, which is lock-free on all cores
    class ExecutionProgress
    {
    public:
        class ActionScope
        {
        public:
            explicit ActionScope(ExecutionProgress& progress);
            ActionScope(const ActionScope& other) = delete;
            ActionScope& operator=(const ActionScope& other) = delete;
            ~ActionScope();

        private:
            ExecutionProgress& progress;
        };

        uint32_t Steps() const;
        static bool IsExecuting(uint32_t steps);

    private:
        void Step();

        std::atomic<uint32_t> steps{ 0 };
    };
}

#endif
