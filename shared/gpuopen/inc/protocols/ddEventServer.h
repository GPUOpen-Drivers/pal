/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/
/**
***********************************************************************************************************************
* @file  ddEventServer.h
* @brief Header for EventServer
***********************************************************************************************************************
*/

#pragma once

#include <baseProtocolServer.h>
#include <util/hashMap.h>
#include <util/ddBitSet.h>
#include <protocols/ddEventProtocol.h>

namespace DevDriver
{

namespace EventProtocol
{

class IEventProvider;
class EventServerSession;

class EventServer final : public BaseProtocolServer
{
    friend class IEventProvider;
    friend class EventServerSession;

public:
    explicit EventServer(IMsgChannel* pMsgChannel);
    ~EventServer() = default;

    bool AcceptSession(const SharedPointer<ISession>& pSession) override;
    void SessionEstablished(const SharedPointer<ISession>& pSession) override;
    void UpdateSession(const SharedPointer<ISession>& pSession) override;
    void SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason) override;

    Result RegisterProvider(IEventProvider* pProvider);
    Result UnregisterProvider(IEventProvider* pProvider);

private:
    Result LogEvent(const void* pEventData, size_t eventDataSize);
    Result BuildQueryProvidersResponse(BlockId* pBlockId);
    Result ApplyProviderUpdate(const ProviderUpdateHeader* pUpdate);

    HashMap<EventProviderId, IEventProvider*, 16u> m_eventProviders;
    Platform::Mutex                                m_eventProvidersMutex;
    Platform::Mutex                                m_activeSessionMutex;
    EventServerSession*                            m_pActiveSession = nullptr;
};

class IEventProvider
{
    friend class EventServer;

protected:
    IEventProvider() {}
    virtual ~IEventProvider() {}

    Result LogEvent(EventServer* pServer, const void* pEventData, size_t eventDataSize)
    {
        DD_ASSERT(pServer != nullptr);
        DD_ASSUME(pServer != nullptr);

        return pServer->LogEvent(pEventData, eventDataSize);
    }

    virtual EventProviderId GetId() const = 0;

    virtual ProviderDescriptionHeader GetHeader() const = 0;

    virtual uint32 GetNumEvents() const = 0;
    virtual const void* GetEventDescriptionData() const = 0;
    virtual uint32 GetEventDescriptionDataSize() const = 0;
    virtual const void* GetEventData() const = 0;
    virtual size_t GetEventDataSize() const = 0;

private:
    virtual void UpdateEventData(const void* pEventData, size_t eventDataSize) = 0;

    virtual void Enable() = 0;
    virtual void Disable() = 0;

    virtual void EnableEvent(uint32 eventId) = 0;
    virtual void DisableEvent(uint32 eventId) = 0;

    virtual void SetEventServer(EventServer* pServer) = 0;
};

template <uint32 NumEvents>
class EventProvider : public IEventProvider
{
public:
    uint32 GetNumEvents() const override { return NumEvents; }
    const void* GetEventData() const override { return m_eventState.GetBitData(); }
    size_t GetEventDataSize() const override { return m_eventState.GetBitDataSize(); }

    bool IsProviderEnabled() const { return m_isEnabled; }
    bool IsEventEnabled(uint32 eventId) const { return m_eventState[eventId]; }
    bool IsProviderRegistered() const { return (m_pServer != nullptr); }

protected:
    EventProvider() = default;
    virtual ~EventProvider() = default;

    // Returns Success if the write would have made it through to the event server
    // Returns Unavailable if the event provider has no way to forward events to a server
    // Returns Rejected if the write would have been rejected due to event filtering settings
    Result QueryEventWriteStatus(uint32 eventId) const
    {
        Result result = IsProviderRegistered() ? Result::Success
                                               : Result::Unavailable;

        if (result == Result::Success)
        {
            result = (IsProviderEnabled() && IsEventEnabled(eventId)) ? Result::Success
                                                                      : Result::Rejected;
        }

        return result;
    }

    Result WriteEvent(uint32 eventId, const void* pEventData, size_t eventDataSize)
    {
        Result result = QueryEventWriteStatus(eventId);

        if (result == Result::Success)
        {
            result = LogEvent(m_pServer, pEventData, eventDataSize);
        }

        return result;
    }

    ProviderDescriptionHeader GetHeader() const override
    {
        return ProviderDescriptionHeader(GetId(),
                                         NumEvents,
                                         GetEventDescriptionDataSize(),
                                         m_isEnabled);
    }

private:
    void Enable() override { m_isEnabled = true; }
    void Disable() override { m_isEnabled = false; }

    void UpdateEventData(const void* pEventData, size_t eventDataSize) override
    {
        m_eventState.UpdateBitData(pEventData, eventDataSize);
    }

    void EnableEvent(uint32 eventId) override { m_eventState.SetBit(eventId); }
    void DisableEvent(uint32 eventId) override { m_eventState.ResetBit(eventId); }

    void SetEventServer(EventServer* pServer) override { m_pServer = pServer; }

    EventServer*          m_pServer = nullptr;
    BitSet<NumEvents>     m_eventState;
    bool                  m_isEnabled = false;
};

} // EventProtocol

} // DevDriver
