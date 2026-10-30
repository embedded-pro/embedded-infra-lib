#include "drivers/eeprom/Eeprom24xx.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace drivers
{
    Eeprom24xx::Eeprom24xx(hal::I2cMaster& master, const Config& config)
        : master(master)
        , config(config)
    {
        really_assert(config.size != 0 && config.size <= maxSize);
        really_assert(config.pageSize != 0 && config.pageSize <= maxPageSize);
    }

    uint32_t Eeprom24xx::Size() const
    {
        return config.size;
    }

    void Eeprom24xx::WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        Start(onDone);
        source = buffer;
        erasing = false;
        StartWrite(address, static_cast<uint32_t>(buffer.size()));
    }

    void Eeprom24xx::ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        really_assert(address <= config.size && buffer.size() <= config.size - address);

        Start(onDone);
        this->address = address;
        destination = buffer;
        ReadNextBlock();
    }

    void Eeprom24xx::Erase(infra::Function<void()> onDone)
    {
        Start(onDone);
        source = infra::ConstByteRange();
        erasing = true;
        StartWrite(0, config.size);
    }

    hal::I2cAddress Eeprom24xx::DeviceAddress(uint32_t address) const
    {
        return hal::I2cAddress(static_cast<uint16_t>(config.deviceAddress | (address / blockSize)));
    }

    void Eeprom24xx::Start(const infra::Function<void()>& onDone)
    {
        really_assert(!busy);

        busy = true;
        this->onDone = onDone;
    }

    void Eeprom24xx::Finish()
    {
        busy = false;
        onDone();
    }

    void Eeprom24xx::StartWrite(uint32_t address, uint32_t size)
    {
        really_assert(address <= config.size && size <= config.size - address);

        this->address = address;
        remaining = size;
        WriteNextPage();
    }

    void Eeprom24xx::WriteNextPage()
    {
        if (remaining == 0)
        {
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    Finish();
                });
            return;
        }

        chunk = std::min(remaining, config.pageSize - address % config.pageSize);

        frame[0] = static_cast<uint8_t>(address);
        auto payload = infra::Head(infra::DiscardHead(infra::MakeRange(frame), 1), chunk);
        if (erasing)
            std::fill(payload.begin(), payload.end(), 0xff);
        else
            std::copy_n(source.begin(), chunk, payload.begin());

        expired = false;
        writeDeadline.Start(config.writeTimeout, [this]()
            {
                expired = true;
            });

        SendPage();
    }

    void Eeprom24xx::SendPage()
    {
        master.SendData(DeviceAddress(address), infra::Head(infra::MakeRange(frame), chunk + 1), hal::Action::stop, [this](hal::Result result, uint32_t)
            {
                PageSent(result);
            });
    }

    // A device still in the write cycle of the previous page acknowledges nothing, so the page is retried until the timeout
    void Eeprom24xx::PageSent(hal::Result result)
    {
        if (result == hal::Result::complete)
            Poll();
        else if (!expired)
            SendPage();
        else
            PageDone();
    }

    // Only the word address: the device acknowledges it once its write cycle ends, and it starts no new write
    void Eeprom24xx::Poll()
    {
        master.SendData(DeviceAddress(address), infra::Head(infra::MakeRange(frame), 1), hal::Action::stop, [this](hal::Result result, uint32_t)
            {
                Polled(result);
            });
    }

    void Eeprom24xx::Polled(hal::Result result)
    {
        if (result == hal::Result::complete || expired)
            PageDone();
        else
            Poll();
    }

    void Eeprom24xx::PageDone()
    {
        writeDeadline.Cancel();

        address += chunk;
        remaining -= chunk;
        if (!erasing)
            source = infra::DiscardHead(source, chunk);

        WriteNextPage();
    }

    infra::ByteRange Eeprom24xx::CurrentChunk() const
    {
        return infra::Head(destination, std::min<std::size_t>(destination.size(), blockSize - address % blockSize));
    }

    void Eeprom24xx::ReadNextBlock()
    {
        if (destination.empty())
        {
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    Finish();
                });
            return;
        }

        frame[0] = static_cast<uint8_t>(address);

        master.SendData(DeviceAddress(address), infra::Head(infra::MakeRange(frame), 1), hal::Action::repeatedStart, [this](hal::Result result, uint32_t)
            {
                WordAddressSent(result);
            });
    }

    void Eeprom24xx::WordAddressSent(hal::Result result)
    {
        if (result != hal::Result::complete)
        {
            ChunkReceived(result);
            return;
        }

        master.ReceiveData(DeviceAddress(address), CurrentChunk(), hal::Action::stop, [this](hal::Result result)
            {
                ChunkReceived(result);
            });
    }

    void Eeprom24xx::ChunkReceived(hal::Result result)
    {
        const auto received = CurrentChunk();

        if (result != hal::Result::complete)
            std::fill(received.begin(), received.end(), 0xff);

        address += static_cast<uint32_t>(received.size());
        destination = infra::DiscardHead(destination, received.size());
        ReadNextBlock();
    }
}
