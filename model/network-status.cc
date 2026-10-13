/*
 * Copyright (c) 2018 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Davide Magrin <magrinda@dei.unipd.it>
 *          Martina Capuzzo <capuzzom@dei.unipd.it>
 */

#include "network-status.h"

#include "class-b-end-device-lorawan-mac.h"
#include "end-device-lorawan-mac.h"
#include "lora-tag.h"

#include "ns3/simulator.h"

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("NetworkStatus");

NS_OBJECT_ENSURE_REGISTERED(NetworkStatus);

TypeId
NetworkStatus::GetTypeId()
{
    static TypeId tid = TypeId("ns3::NetworkStatus")
                            .SetParent<Object>()
                            .AddConstructor<NetworkStatus>()
                            .SetGroupName("lorawan");
    return tid;
}

NetworkStatus::NetworkStatus()
{
    NS_LOG_FUNCTION_NOARGS();
}

NetworkStatus::~NetworkStatus()
{
    NS_LOG_FUNCTION_NOARGS();
}

void
NetworkStatus::AddNode(Ptr<EndDeviceLorawanMac> edMac)
{
    NS_LOG_FUNCTION(this << edMac);

    // Check whether this device already exists in our list
    LoraDeviceAddress edAddress = edMac->GetDeviceAddress();
    if (m_endDeviceStatuses.find(edAddress) == m_endDeviceStatuses.end())
    {
        // The device doesn't exist. Create new EndDeviceStatus
        Ptr<EndDeviceStatus> edStatus = CreateObject<EndDeviceStatus>(edAddress, edMac);

        // Add it to the map
        m_endDeviceStatuses.insert(
            std::pair<LoraDeviceAddress, Ptr<EndDeviceStatus>>(edAddress, edStatus));
        NS_LOG_DEBUG("Added to the list a device with address " << edAddress.Print());
    }
}

void
NetworkStatus::AddGateway(Address& address, Ptr<GatewayStatus> gwStatus)
{
    NS_LOG_FUNCTION(this);

    // Check whether this device already exists in the list
    if (m_gatewayStatuses.find(address) == m_gatewayStatuses.end())
    {
        // The device doesn't exist.

        // Add it to the map
        m_gatewayStatuses.insert(std::pair<Address, Ptr<GatewayStatus>>(address, gwStatus));
        NS_LOG_DEBUG("Added to the list a gateway with address " << address);
    }
}

void
NetworkStatus::OnReceivedPacket(Ptr<const Packet> packet, const Address& gwAddress)
{
    NS_LOG_FUNCTION(this << packet << gwAddress);

    // Create a copy of the packet
    Ptr<Packet> myPacket = packet->Copy();

    // Extract the headers
    LorawanMacHeader macHdr;
    myPacket->RemoveHeader(macHdr);
    LoraFrameHeader frameHdr;
    frameHdr.SetAsUplink();
    myPacket->RemoveHeader(frameHdr);

    // Update the correct EndDeviceStatus object
    LoraDeviceAddress edAddr = frameHdr.GetAddress();
    NS_LOG_DEBUG("Node address: " << edAddr);
    m_endDeviceStatuses.at(edAddr)->InsertReceivedPacket(packet, gwAddress);
}

bool
NetworkStatus::NeedsReply(LoraDeviceAddress deviceAddress)
{
    // Throws out of range if no device is found
    return m_endDeviceStatuses.at(deviceAddress)->NeedsReply();
}

Address
NetworkStatus::GetBestGatewayForDevice(LoraDeviceAddress deviceAddress, int window)
{
    // Get the endDeviceStatus we are interested in
    Ptr<EndDeviceStatus> edStatus = m_endDeviceStatuses.at(deviceAddress);
    uint32_t replyFrequency;
    if (window == 1)
    {
        replyFrequency = edStatus->GetFirstReceiveWindowFrequency();
    }
    else if (window == 2)
    {
        replyFrequency = edStatus->GetSecondReceiveWindowFrequency();
    }
    else if (window == 3)
    {
        // Class B ping slot: fixed frequency, same convention as the beacon channel.
        replyFrequency = ClassBEndDeviceLorawanMac::BEACON_FREQUENCY_HZ;
    }
    else
    {
        NS_ABORT_MSG("Invalid window value");
    }

    // Get the list of gateways that this device can reach
    // NOTE: At this point, we could also take into account the whole network to
    // identify the best gateway according to various metrics. For now, we just
    // ask the EndDeviceStatus to pick the best gateway for us via its method.
    std::map<double, Address> gwAddresses = edStatus->GetPowerGatewayMap();

    // By iterating on the map in reverse, we go from the 'best'
    // gateway, i.e. the one with the highest received power, to the
    // worst.
    Address bestGwAddress;
    for (auto it = gwAddresses.rbegin(); it != gwAddresses.rend(); it++)
    {
        bool isAvailable =
            m_gatewayStatuses.find(it->second)->second->IsAvailableForTransmission(replyFrequency);
        if (isAvailable)
        {
            bestGwAddress = it->second;
            break;
        }
    }

    return bestGwAddress;
}

void
NetworkStatus::SendThroughGateway(Ptr<Packet> packet, Address gwAddress)
{
    NS_LOG_FUNCTION(packet << gwAddress);

    m_gatewayStatuses.find(gwAddress)->second->GetNetDevice()->Send(packet, gwAddress, 0x0800);
}

Ptr<Packet>
NetworkStatus::GetReplyForDevice(LoraDeviceAddress edAddress, int windowNumber)
{
    // Get the reply packet
    Ptr<EndDeviceStatus> edStatus = m_endDeviceStatuses.find(edAddress)->second;
    Ptr<Packet> packet = edStatus->GetCompleteReplyPacket();

    // Apply the appropriate tag
    LoraTag tag;
    switch (windowNumber)
    {
    case 1:
        tag.SetDataRate(edStatus->GetMac()->GetFirstReceiveWindowDataRate());
        tag.SetFrequency(edStatus->GetFirstReceiveWindowFrequency());
        break;
    case 2:
        tag.SetDataRate(edStatus->GetMac()->GetSecondReceiveWindowDataRate());
        tag.SetFrequency(edStatus->GetSecondReceiveWindowFrequency());
        break;
    case 3:
        tag.SetDataRate(ClassBEndDeviceLorawanMac::BEACON_DATA_RATE);
        tag.SetFrequency(ClassBEndDeviceLorawanMac::BEACON_FREQUENCY_HZ);
        break;
    }

    packet->AddPacketTag(tag);
    return packet;
}

Ptr<EndDeviceStatus>
NetworkStatus::GetEndDeviceStatus(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    // Get the address
    LorawanMacHeader mHdr;
    LoraFrameHeader fHdr;
    fHdr.SetAsUplink();
    Ptr<Packet> myPacket = packet->Copy();
    myPacket->RemoveHeader(mHdr);
    myPacket->RemoveHeader(fHdr);
    auto it = m_endDeviceStatuses.find(fHdr.GetAddress());
    if (it != m_endDeviceStatuses.end())
    {
        return (*it).second;
    }
    else
    {
        NS_LOG_ERROR("EndDeviceStatus not found");
        return nullptr;
    }
}

Ptr<EndDeviceStatus>
NetworkStatus::GetEndDeviceStatus(LoraDeviceAddress address)
{
    NS_LOG_FUNCTION(this << address);

    auto it = m_endDeviceStatuses.find(address);
    if (it != m_endDeviceStatuses.end())
    {
        return (*it).second;
    }
    else
    {
        NS_LOG_ERROR("EndDeviceStatus not found");
        return nullptr;
    }
}

int
NetworkStatus::CountEndDevices()
{
    NS_LOG_FUNCTION(this);

    return m_endDeviceStatuses.size();
}

void
NetworkStatus::SendDownlinkToDevice(LoraDeviceAddress edAddress, Ptr<Packet> payload)
{
    NS_LOG_FUNCTION(this << edAddress << payload);

    Ptr<EndDeviceStatus> edStatus = m_endDeviceStatuses.at(edAddress);

    // Populate the reply: an unconfirmed, unsolicited downlink with the given payload
    edStatus->SetReplyPayload(payload);
    LorawanMacHeader mHdr;
    mHdr.SetFType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
    edStatus->SetReplyMacHeader(mHdr);
    LoraFrameHeader fHdr;
    fHdr.SetAsDownlink();
    fHdr.SetAck(false);
    fHdr.SetAddress(edAddress);
    edStatus->SetReplyFrameHeader(fHdr);
    edStatus->m_reply.needsReply = true;

    if (edStatus->IsClassC())
    {
        // Class C devices are continuously listening: no need to wait for an uplink from this
        // device, attempt delivery right away.
        AttemptClassCDelivery(edAddress);
    }
    else if (edStatus->IsClassB())
    {
        // Class B devices only listen at their own ping slots: attempt delivery timed to land
        // inside the device's next one.
        AttemptClassBDelivery(edAddress);
    }
    // For Class A devices, the reply above just waits for the device's next reactive
    // opportunity, handled entirely by the existing NetworkScheduler flow.
}

void
NetworkStatus::AttemptClassCDelivery(LoraDeviceAddress edAddress)
{
    NS_LOG_FUNCTION(this << edAddress);

    if (!NeedsReply(edAddress))
    {
        // Already delivered, e.g. reactively via the device's own next uplink in the meantime.
        return;
    }

    // Class C's continuously open receive window is what NetworkScheduler otherwise associates
    // with "window 2".
    Address gwAddress = GetBestGatewayForDevice(edAddress, 2);
    if (gwAddress == Address())
    {
        NS_LOG_DEBUG("No suitable gateway found for immediate Class C delivery, retrying in 1s");
        Simulator::Schedule(Seconds(1), &NetworkStatus::AttemptClassCDelivery, this, edAddress);
        return;
    }

    NS_LOG_DEBUG("Found available gateway with address: " << gwAddress);
    SendThroughGateway(GetReplyForDevice(edAddress, 2), gwAddress);
    m_endDeviceStatuses.at(edAddress)->InitializeReply();
}

void
NetworkStatus::AttemptClassBDelivery(LoraDeviceAddress edAddress)
{
    NS_LOG_FUNCTION(this << edAddress);

    if (!NeedsReply(edAddress))
    {
        // Already delivered, e.g. reactively via the device's own next uplink in the meantime.
        return;
    }

    Ptr<EndDeviceStatus> edStatus = m_endDeviceStatuses.at(edAddress);
    Ptr<ClassBEndDeviceLorawanMac> classBMac =
        DynamicCast<ClassBEndDeviceLorawanMac>(edStatus->GetMac());
    NS_ASSERT_MSG(classBMac, "AttemptClassBDelivery called for a non-Class-B device");

    if (!classBMac->IsSynchronized())
    {
        // No beacon time reference (yet, or any longer): the device isn't actually opening any
        // ping slots right now, so there is no occurrence to target. Retry after giving it a
        // chance to (re)acquire the beacon.
        NS_LOG_DEBUG("Device not synchronized to the beacon, retrying Class B delivery in "
                     << ClassBEndDeviceLorawanMac::BEACON_INTERVAL.As(Time::S));
        Simulator::Schedule(ClassBEndDeviceLorawanMac::BEACON_INTERVAL,
                            &NetworkStatus::AttemptClassBDelivery,
                            this,
                            edAddress);
        return;
    }

    Time targetSlot = NextPingSlotTime(edAddress, classBMac, Simulator::Now());
    if (targetSlot > Simulator::Now())
    {
        NS_LOG_DEBUG("Scheduling Class B delivery at the device's next ping slot: "
                     << targetSlot.As(Time::S));
        Simulator::Schedule(targetSlot - Simulator::Now(),
                            &NetworkStatus::AttemptClassBDelivery,
                            this,
                            edAddress);
        return;
    }

    // We are exactly at the target ping slot: attempt delivery now.
    Address gwAddress = GetBestGatewayForDevice(edAddress, 3);
    if (gwAddress == Address())
    {
        NS_LOG_DEBUG("No suitable gateway available at this ping slot, retrying at the next one");
        Time nextSlot = NextPingSlotTime(edAddress, classBMac, targetSlot + NanoSeconds(1));
        Simulator::Schedule(nextSlot - Simulator::Now(),
                            &NetworkStatus::AttemptClassBDelivery,
                            this,
                            edAddress);
        return;
    }

    NS_LOG_DEBUG("Found available gateway with address: " << gwAddress);
    SendThroughGateway(GetReplyForDevice(edAddress, 3), gwAddress);
    edStatus->InitializeReply();
}

Time
NetworkStatus::NextPingSlotTime(LoraDeviceAddress edAddress,
                                Ptr<ClassBEndDeviceLorawanMac> classBMac,
                                Time notBefore) const
{
    uint16_t pingNbSlots =
        ClassBEndDeviceLorawanMac::PingNbSlots(classBMac->GetPingSlotPeriodicity());
    Time beaconInterval = ClassBEndDeviceLorawanMac::BEACON_INTERVAL;
    Time pingPeriod = beaconInterval / pingNbSlots;

    // Beacons are transmitted on a fixed schedule anchored at simulation time 0 (see
    // GatewayLorawanMac::SendBeacon / ClassBEndDeviceLorawanMac::TimeUntilNextBeaconBoundary):
    // the period containing notBefore starts at the largest multiple of beaconInterval not
    // exceeding it.
    int64_t periodIndex = notBefore.GetNanoSeconds() / beaconInterval.GetNanoSeconds();
    Time periodStart = beaconInterval * periodIndex;

    // At most two iterations in practice: the period containing notBefore, and (only if that
    // period's own slots are all already behind notBefore) the one right after it.
    for (;;)
    {
        Time offset = ClassBEndDeviceLorawanMac::ComputePingOffset(
            edAddress,
            static_cast<uint32_t>(periodStart.GetSeconds()),
            pingPeriod);
        Time firstSlot = periodStart + offset;

        if (firstSlot >= notBefore)
        {
            return firstSlot;
        }

        int64_t deltaNs = (notBefore - firstSlot).GetNanoSeconds();
        int64_t periodNs = pingPeriod.GetNanoSeconds();
        int64_t k = deltaNs / periodNs;
        if (deltaNs % periodNs != 0)
        {
            k += 1;
        }

        if (k < pingNbSlots)
        {
            // Still within this beacon period's own slots (guaranteed < periodStart +
            // beaconInterval, by the same bound ComputePingOffset() itself relies on).
            return firstSlot + pingPeriod * k;
        }

        // This period's slots are exhausted: move to the next period, which has its own,
        // independently pseudo-random offset (a hash of that period's own beacon time).
        periodStart += beaconInterval;
    }
}

} // namespace lorawan
} // namespace ns3
