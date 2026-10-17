#include "infra/util/Function.hpp"

namespace infra
{
    namespace detail
    {
        namespace
        {
            constexpr auto abortOnExecute = []()
            {
                std::abort();
            };
        }

        const InvokerFunctions<void(), INFRA_DEFAULT_FUNCTION_EXTRA_SIZE>::VirtualMethodTable abortOnExecuteTable = { { nullptr }, &InvokerFunctions<void(), INFRA_DEFAULT_FUNCTION_EXTRA_SIZE>::StaticInvoke<decltype(abortOnExecute)> };
    }

    const infra::Function<void()> emptyFunction = []() {};

    Execute::Execute(Function<void()> f)
    {
        f();
    }

    ExecuteOnDestruction::ExecuteOnDestruction(Function<void()> f)
        : f(f)
    {}

    ExecuteOnDestruction::~ExecuteOnDestruction()
    {
        f();
    }
}
