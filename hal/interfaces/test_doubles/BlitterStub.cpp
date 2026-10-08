#include "hal/interfaces/test_doubles/BlitterStub.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace hal
{
    BlitterStub::BlitterStub()
    {
        ON_CALL(*this, Supports).WillByDefault(testing::Return(true));

        ON_CALL(*this, Fill).WillByDefault([this](const Surface& destination, Argb8888, const infra::Function<void()>& onDone)
            {
                really_assert(IsValidSurface(destination));
                Begin(onDone);
            });

        ON_CALL(*this, Copy).WillByDefault([this](const ConstSurface& source, const Surface& destination, const infra::Function<void()>& onDone)
            {
                really_assert(IsValidSurface(source) && IsValidSurface(destination));
                really_assert(source.size == destination.size);
                Begin(onDone);
            });

        ON_CALL(*this, Blend).WillByDefault([this](const BlendSource& foreground, const ConstSurface& background, const Surface& destination, const infra::Function<void()>& onDone)
            {
                really_assert(IsValidSurface(foreground.surface) && IsValidSurface(background) && IsValidSurface(destination));
                really_assert(foreground.surface.size == destination.size && background.size == destination.size);
                really_assert(IsDirectColour(background.format));
                Begin(onDone);
            });
    }

    void BlitterStub::CompleteOperation()
    {
        really_assert(OperationPending());
        pendingCompletion();
    }

    bool BlitterStub::OperationPending() const
    {
        return static_cast<bool>(pendingCompletion);
    }

    void BlitterStub::Begin(const infra::Function<void()>& onDone)
    {
        really_assert(!OperationPending());
        pendingCompletion = onDone;
    }
}
