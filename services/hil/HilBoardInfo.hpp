#ifndef SERVICES_HIL_BOARD_INFO_HPP
#define SERVICES_HIL_BOARD_INFO_HPP

#include "infra/util/ByteRange.hpp"
#include <cstdint>

namespace services
{
    class HilBoardInfo
    {
    protected:
        HilBoardInfo() = default;
        HilBoardInfo(const HilBoardInfo& other) = delete;
        HilBoardInfo& operator=(const HilBoardInfo& other) = delete;
        ~HilBoardInfo() = default;

    public:
        virtual const char* Name() const = 0;
        virtual const char* Family() const = 0;
        virtual uint32_t SystemClock() const = 0;
        virtual const char* ResetCause() const = 0;
        virtual infra::ConstByteRange UniqueId() const = 0;
    };
}

#endif
