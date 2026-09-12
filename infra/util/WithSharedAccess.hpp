#ifndef INFRA_WITH_SHARED_ACCESS_HPP
#define INFRA_WITH_SHARED_ACCESS_HPP

#include "infra/util/SharedPtr.hpp"

namespace infra
{
    template<class T>
    class WithSharedAccess
    {
    public:
        template<class... Args>
        explicit WithSharedAccess(Args&&... args)
            : accessedBy([](){})
            , object(std::forward<Args>(args)...)
            , sharedPtr(accessedBy.MakeShared(object))
        {}

        WithSharedAccess(const WithSharedAccess&) = delete;
        WithSharedAccess& operator=(const WithSharedAccess&) = delete;

        T& operator*() { return object; }
        const T& operator*() const { return object; }
        T* operator->() { return &object; }
        const T* operator->() const { return &object; }

    private:
        // sharedPtr destroyed first, object second (drops EnableSharedFromThis::weakPtr),
        // accessedBy last — satisfies its UnReferenced() assertion at destruction.
        infra::AccessedBySharedPtr accessedBy;
        T object;
        infra::SharedPtr<T> sharedPtr;
    };
}

#endif
