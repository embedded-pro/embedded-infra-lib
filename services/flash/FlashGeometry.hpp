#ifndef SERVICES_FLASH_GEOMETRY_HPP
#define SERVICES_FLASH_GEOMETRY_HPP

#include <cstdint>

namespace services
{
    class FlashGeometry
    {
    public:
        virtual uint32_t NrOfSubSectors() const = 0;
        virtual uint32_t SizeSector() const = 0;
        virtual uint32_t SizeSubSector() const = 0;
        virtual uint32_t SizePage() const = 0;
        virtual bool ExtendedAddressing() const = 0;
    };
}

#endif
