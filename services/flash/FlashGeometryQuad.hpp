#ifndef SERVICES_FLASH_GEOMETRY_QUAD_HPP
#define SERVICES_FLASH_GEOMETRY_QUAD_HPP

#include "services/flash/FlashGeometry.hpp"
#include <cstdint>

namespace services
{
    class FlashGeometryQuad
        : virtual public FlashGeometry
    {
    public:
        virtual uint8_t EraseSubSectorCommand() const = 0;
        virtual uint8_t EraseSectorCommand() const = 0;
        virtual uint8_t EraseBulkCommand() const = 0;
        virtual uint8_t PageProgramCommand() const = 0;
        virtual uint8_t ReadDataCommand() const = 0;
        virtual uint8_t ReadDummyCycles() const = 0;
    };
}

#endif
