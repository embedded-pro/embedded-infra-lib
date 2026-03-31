#include "services/echo_core/Serialization.hpp"
#include "services/echo_core/Echo.hpp"

namespace services
{
    MethodDeserializerDummy::MethodDeserializerDummy(Echo& echo)
        : echo(echo)
    {}

    void MethodDeserializerDummy::MethodContents(infra::SharedPtr<infra::StreamReaderWithRewinding>&& reader)
    {
        while (!reader->Empty())
            reader->ExtractContiguousRange(std::numeric_limits<uint32_t>::max());
    }

    void MethodDeserializerDummy::ExecuteMethod()
    {
        echo.ServiceDone();
    }

    bool MethodDeserializerDummy::Failed() const
    {
        return false;
    }

    infra::SharedPtr<MethodDeserializer> MethodSerializerFactory::MakeDummyDeserializer(Echo& echo)
    {
        using Deserializer = MethodDeserializerDummy;

        auto memory = DeserializerMemory(sizeof(Deserializer));
        auto deserializer = new (memory->begin()) Deserializer(echo);
        return infra::MakeContainedSharedObject(*deserializer, memory);
    }

    infra::SharedPtr<infra::ByteRange> MethodSerializerFactory::OnHeap::SerializerMemory(uint32_t size)
    {
        serializerStorage = std::make_unique<uint8_t[]>(size);
        serializerMemory = { serializerStorage.get(),
            serializerStorage.get() + size };
        return serializerAccess.MakeShared(serializerMemory);
    }

    infra::SharedPtr<infra::ByteRange> MethodSerializerFactory::OnHeap::DeserializerMemory(uint32_t size)
    {
        deserializerStorage = std::make_unique<uint8_t[]>(size);
        deserializerMemory = { deserializerStorage.get(),
            deserializerStorage.get() + size };
        return deserializerAccess.MakeShared(deserializerMemory);
    }
}
