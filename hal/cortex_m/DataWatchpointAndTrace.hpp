#ifndef HAL_CORTEX_M_DATA_WATCHPOINT_AND_TRACE_HPP
#define HAL_CORTEX_M_DATA_WATCHPOINT_AND_TRACE_HPP

#include "infra/util/InterfaceConnector.hpp"
#include <cstdint>

#if !defined(__ARM_ARCH_6M__)

namespace hal::cortex
{
    class DataWatchpointAndTrace
        : public infra::InterfaceConnector<DataWatchpointAndTrace>
    {
    public:
        DataWatchpointAndTrace();
        ~DataWatchpointAndTrace();

        void Start() const;
        uint32_t Stop() const;
    };
}

#endif

#endif
