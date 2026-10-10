#ifndef SERVICES_FLASH_GEOMETRY_QUAD_HPP
#define SERVICES_FLASH_GEOMETRY_QUAD_HPP

#include "services/flash/FlashGeometry.hpp"
#include <cstdint>

namespace services
{
    class FlashGeometryQuad
        : public FlashGeometry
    {
    public:
        virtual uint8_t EraseSubSectorCommand() const = 0;
        virtual uint8_t EraseSectorCommand() const = 0;
        virtual uint8_t EraseBulkCommand() const = 0;
        virtual uint8_t PageProgramCommand() const = 0;
        virtual uint8_t ReadDataCommand() const = 0;
        virtual uint8_t ReadDummyCycles() const = 0;

        // 4 for a quad I/O read (1-4-4), 1 for a quad output read (1-1-4)
        virtual uint8_t ReadAddressLines() const
        {
            return 4;
        }
    };
}

#endif
