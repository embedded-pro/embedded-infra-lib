#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/SharedObjectAllocatorFixedSize.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/ble/ClaimingGattClientConnection.hpp"
#include "services/ble/test_doubles/GattClientConnectionMock.hpp"
#include "services/ble/test_doubles/GattClientMock.hpp"
#include "gmock/gmock.h"
#include <algorithm>

namespace
{
    // Stands in for a platform stack: it pre-allocates its connections, hands each one out as an
    // infra::SharedPtr, and lets go of its own reference when the link is lost.
    class GattClientStub
        : public services::GattClient
    {
    public:
        template<std::size_t MaxConnections>
        using WithMaxConnections = infra::WithStorage<GattClientStub, infra::SharedObjectAllocatorFixedSize<testing::StrictMock<services::GattClientConnectionMock>, void()>::WithStorage<MaxConnections>>;

        explicit GattClientStub(infra::SharedObjectAllocator<testing::StrictMock<services::GattClientConnectionMock>, void()>& connections)
            : connections(connections)
        {}

        std::size_t MaxNumberOfConnections() const override
        {
            return maxNumberOfConnections;
        }

        std::size_t NumberOfConnections() const override
        {
            return established.size();
        }

        infra::SharedPtr<services::GattClientConnection> Establish()
        {
            auto connection = connections.Allocate();
            if (connection == nullptr)
                return nullptr;

            established.push_back(connection);

            NotifyObservers([&connection](auto& observer)
                {
                    observer.ConnectionEstablished(connection);
                });

            return connection;
        }

        void Release(const infra::SharedPtr<services::GattClientConnection>& connection)
        {
            NotifyObservers([&connection](auto& observer)
                {
                    observer.ConnectionReleased(*connection);
                });

            established.erase(std::remove_if(established.begin(), established.end(), [&connection](const auto& each)
                                  {
                                      return each == connection;
                                  }),
                established.end());
        }

        static constexpr std::size_t maxNumberOfConnections = 2;

    private:
        infra::SharedObjectAllocator<testing::StrictMock<services::GattClientConnectionMock>, void()>& connections;
        infra::BoundedVector<infra::SharedPtr<services::GattClientConnection>>::WithMaxSize<maxNumberOfConnections> established;
    };

    class GattClientTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        GattClientStub::WithMaxConnections<GattClientStub::maxNumberOfConnections> gattClient;
        testing::StrictMock<services::GattClientObserverMock> observer{ gattClient };
    };
}

TEST_F(GattClientTest, reports_the_maximum_number_of_connections)
{
    EXPECT_EQ(2, gattClient.MaxNumberOfConnections());
    EXPECT_EQ(0, gattClient.NumberOfConnections());
}

TEST_F(GattClientTest, reports_each_established_connection_to_its_observer)
{
    EXPECT_CALL(observer, ConnectionEstablished(testing::_));
    auto first = gattClient.Establish();

    EXPECT_EQ(1, gattClient.NumberOfConnections());

    EXPECT_CALL(observer, ConnectionEstablished(testing::_));
    auto second = gattClient.Establish();

    EXPECT_EQ(2, gattClient.NumberOfConnections());
    EXPECT_NE(first, second);
}

TEST_F(GattClientTest, refuses_a_connection_beyond_the_maximum)
{
    EXPECT_CALL(observer, ConnectionEstablished(testing::_)).Times(2);
    auto first = gattClient.Establish();
    auto second = gattClient.Establish();

    EXPECT_EQ(nullptr, gattClient.Establish());
}

TEST_F(GattClientTest, operations_are_performed_on_the_connection_they_are_issued_on)
{
    EXPECT_CALL(observer, ConnectionEstablished(testing::_)).Times(2);
    auto first = gattClient.Establish();
    auto second = gattClient.Establish();

    const services::AttAttribute::Handle handle = 0x5;
    infra::Function<void(services::GattResult)> onDone;

    EXPECT_CALL(static_cast<services::GattClientConnectionMock&>(*first), Write(handle, testing::_, testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));

    EXPECT_EQ(services::GattRequestStatus::accepted, first->Write(handle, infra::MakeStringByteRange("first"), onDone));
}

TEST_F(GattClientTest, an_observer_of_one_connection_does_not_see_updates_of_another)
{
    EXPECT_CALL(observer, ConnectionEstablished(testing::_)).Times(2);
    auto first = gattClient.Establish();
    auto second = gattClient.Establish();

    testing::StrictMock<services::GattClientUpdateObserverMock> firstUpdates{ *first };
    testing::StrictMock<services::GattClientUpdateObserverMock> secondUpdates{ *second };

    const services::AttAttribute::Handle handle = 0x5;
    const auto data = infra::MakeStringByteRange("update");

    EXPECT_CALL(secondUpdates, NotificationReceived(handle, testing::_));
    second->infra::Subject<services::GattClientUpdateObserver>::NotifyObservers([handle, &data](auto& updateObserver)
        {
            updateObserver.NotificationReceived(handle, data);
        });
}

TEST_F(GattClientTest, each_connection_serialises_its_own_operations)
{
    EXPECT_CALL(observer, ConnectionEstablished(testing::_)).Times(2);
    auto first = gattClient.Establish();
    auto second = gattClient.Establish();

    services::ClaimingGattClientConnection claimingFirst{ *first };
    services::ClaimingGattClientConnection claimingSecond{ *second };

    const services::AttAttribute::Handle handle = 0x5;
    infra::Function<void(services::GattResult)> ignoredResult;
    infra::Function<void(services::GattResult)> onFirstDiscoveryDone;

    EXPECT_CALL(static_cast<services::GattClientConnectionMock&>(*first), DiscoverServices(testing::_)).WillOnce(testing::DoAll(testing::SaveArg<0>(&onFirstDiscoveryDone), testing::Return(services::GattRequestStatus::accepted)));
    EXPECT_EQ(services::GattRequestStatus::accepted, claimingFirst.DiscoverServices(ignoredResult));
    ExecuteAllActions();

    EXPECT_CALL(static_cast<services::GattClientConnectionMock&>(*second), DiscoverServices(testing::_)).WillOnce(testing::Return(services::GattRequestStatus::accepted));
    EXPECT_EQ(services::GattRequestStatus::accepted, claimingSecond.DiscoverServices(ignoredResult));
    ExecuteAllActions();
}

TEST_F(GattClientTest, a_connection_outlives_the_stack_reference_while_a_holder_remains)
{
    EXPECT_CALL(observer, ConnectionEstablished(testing::_));
    auto connection = gattClient.Establish();

    EXPECT_CALL(observer, ConnectionReleased(testing::_));
    gattClient.Release(connection);

    EXPECT_EQ(0, gattClient.NumberOfConnections());
    EXPECT_NE(nullptr, connection);
}

TEST_F(GattClientTest, a_released_slot_is_reused_only_once_the_last_holder_lets_go)
{
    EXPECT_CALL(observer, ConnectionEstablished(testing::_)).Times(2);
    auto first = gattClient.Establish();
    auto second = gattClient.Establish();

    EXPECT_CALL(observer, ConnectionReleased(testing::_));
    gattClient.Release(first);
    EXPECT_EQ(nullptr, gattClient.Establish());

    first = nullptr;

    EXPECT_CALL(observer, ConnectionEstablished(testing::_));
    EXPECT_NE(nullptr, gattClient.Establish());
}

TEST_F(GattClientTest, a_weak_pointer_to_an_expired_connection_no_longer_locks)
{
    EXPECT_CALL(observer, ConnectionEstablished(testing::_));
    auto connection = gattClient.Establish();

    infra::WeakPtr<services::GattClientConnection> weakConnection = connection;
    EXPECT_NE(nullptr, weakConnection.lock());

    EXPECT_CALL(observer, ConnectionReleased(testing::_));
    gattClient.Release(connection);
    connection = nullptr;

    EXPECT_EQ(nullptr, weakConnection.lock());
}

TEST_F(GattClientTest, reports_a_released_connection_to_its_observer)
{
    EXPECT_CALL(observer, ConnectionEstablished(testing::_));
    auto connection = gattClient.Establish();

    EXPECT_CALL(observer, ConnectionReleased(testing::Ref(*connection)));
    gattClient.Release(connection);
}
