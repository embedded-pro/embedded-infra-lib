#ifndef SERVICES_GATT_CLIENT_LONG_OPERATIONS_HPP
#define SERVICES_GATT_CLIENT_LONG_OPERATIONS_HPP

#include "infra/util/BoundedVector.hpp"
#include "services/ble/GattClientConnection.hpp"

namespace services
{
    // Reading or writing a characteristic value larger than the ATT MTU allows takes several
    // round trips. Composing them is kept apart from GattClientConnection so that a port whose
    // stack performs these natively implements this interface directly instead of having its
    // single-PDU primitives driven from here.
    class GattClientLongOperations
    {
    public:
        // 'value' receives the whole characteristic value and must outlive the procedure; the
        // range reported to 'onDone' views it. A value that does not fit reports
        // insufficientResources, and 'value' then holds as much as was read.
        virtual GattRequestStatus ReadLong(AttAttribute::Handle handle, infra::BoundedVector<uint8_t>& value, const infra::Function<void(GattResult, infra::ConstByteRange)>& onDone) = 0;

        // 'data' must outlive the procedure; it is not copied.
        virtual GattRequestStatus WriteLong(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(GattResult)>& onDone) = 0;
    };
}

#endif
