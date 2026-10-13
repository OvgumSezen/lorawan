/*
 * Copyright (c) 2017 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 */

#include "gateway-lorawan-mac.h"

#include "lora-beacon-header.h"
#include "lora-net-device.h"
#include "lora-phy.h"
#include "lora-tag.h"
#include "lorawan-mac-header.h"

#include "ns3/boolean.h"
#include "ns3/simulator.h"

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("GatewayLorawanMac");

NS_OBJECT_ENSURE_REGISTERED(GatewayLorawanMac);

TypeId
GatewayLorawanMac::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::GatewayLorawanMac")
            .SetParent<LorawanMac>()
            .AddConstructor<GatewayLorawanMac>()
            .SetGroupName("lorawan")
            .AddAttribute("BeaconInterval",
                          "The interval between consecutive Class B beacon transmissions "
                          "from this gateway.",
                          TimeValue(Seconds(128)),
                          MakeTimeAccessor(&GatewayLorawanMac::m_beaconInterval),
                          MakeTimeChecker())
            .AddAttribute("BeaconEnabled",
                          "Whether this gateway periodically transmits Class B beacons. "
                          "Toggling this (including via the attribute system) starts or "
                          "stops beacon transmission immediately.",
                          BooleanValue(false),
                          MakeBooleanAccessor(&GatewayLorawanMac::SetBeaconEnabled,
                                              &GatewayLorawanMac::GetBeaconEnabled),
                          MakeBooleanChecker());
    return tid;
}

GatewayLorawanMac::GatewayLorawanMac()
    : m_beaconEnabled(false),
      m_beaconInterval(Seconds(128))
{
    NS_LOG_FUNCTION(this);
}

GatewayLorawanMac::~GatewayLorawanMac()
{
    NS_LOG_FUNCTION(this);
}

void
GatewayLorawanMac::Send(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    // Get data rate to send this packet with
    LoraTag tag;
    packet->RemovePacketTag(tag);
    uint8_t dataRate = tag.GetDataRate();
    uint32_t frequencyHz = tag.GetFrequency();
    NS_LOG_DEBUG("DR: " << unsigned(dataRate));
    NS_LOG_DEBUG("SF: " << unsigned(GetSfFromDataRate(dataRate)));
    NS_LOG_DEBUG("BW: " << GetBandwidthFromDataRate(dataRate));
    NS_LOG_DEBUG("Freq: " << frequencyHz << " Hz");
    packet->AddPacketTag(tag);

    // Make sure we can transmit this packet
    if (GetWaitTime(frequencyHz).IsStrictlyPositive())
    {
        // We cannot send now!
        NS_LOG_WARN("Trying to send a packet but Duty Cycle won't allow it. Aborting.");
        return;
    }

    auto sf = GetSfFromDataRate(dataRate);
    auto bw = GetBandwidthFromDataRate(dataRate);
    // see SX1272/73 Datasheet, Section 4.1.1.6, Rev. 4, Jan. 2019
    auto ldro = bool((sf == 11 || sf == 12) && bw == 125'000);

    LoraTxParameters params;
    params.spreadingFactor = sf;
    params.bandwidthHz = bw;
    params.codingRate = CodingRate::CR_4_5;
    params.lowDataRateOptimize = ldro;
    params.preambleLenSymb = 8;
    params.implicitHeader = false;
    params.crcEnabled = true;

    // Get the duration
    Time duration = LoraPhy::GetTimeOnAir(packet->GetSize(), params);

    NS_LOG_DEBUG("Duration: " << duration.As(Time::S));

    // Find the channel with the desired frequency
    double sendingPower = m_channelHelper->GetTxPowerForChannel(frequencyHz);

    // Add the event to the channelHelper to keep track of duty cycle
    m_channelHelper->AddEvent(duration, frequencyHz);

    // Send the packet to the PHY layer to send it on the channel
    m_phy->Send(packet, frequencyHz, IQPolarity::DOWN, params, sendingPower);

    m_sentNewPacket(packet);
}

bool
GatewayLorawanMac::IsTransmitting()
{
    return m_phy->IsTransmitting();
}

Time
GatewayLorawanMac::GetWaitTime(uint32_t frequencyHz)
{
    NS_LOG_FUNCTION_NOARGS();
    return m_channelHelper->GetWaitTime(frequencyHz);
}

void
GatewayLorawanMac::TxFinished(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
}

void
GatewayLorawanMac::Receive(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    // Make a copy of the packet to work on
    Ptr<Packet> packetCopy = packet->Copy();

    // Only forward the packet if it's uplink
    LorawanMacHeader macHdr;
    packetCopy->PeekHeader(macHdr);

    if (macHdr.IsUplink())
    {
        NS_LOG_DEBUG("Received packet: " << packet);
        // Pass the packet up to the NetDevice
        if (!m_receiveCallback.IsNull())
        {
            m_receiveCallback(packetCopy);
        }
        m_receivedPacket(packet);
    }
    else
    {
        NS_LOG_DEBUG("Not forwarding downlink message to NetDevice");
    }
}

void
GatewayLorawanMac::FailedReception(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
}

void
GatewayLorawanMac::SetBeaconEnabled(bool enable)
{
    NS_LOG_FUNCTION(this << enable);

    if (m_beaconEnabled == enable)
    {
        return;
    }

    m_beaconEnabled = enable;

    if (m_beaconEnabled)
    {
        // Send the first beacon right away (i.e., whenever beaconing gets enabled); every
        // later one is spaced out by m_beaconInterval, from within SendBeacon() itself.
        m_nextBeacon = Simulator::Schedule(Seconds(0), &GatewayLorawanMac::SendBeacon, this);
    }
    else if (m_nextBeacon.IsPending())
    {
        m_nextBeacon.Cancel();
    }
}

bool
GatewayLorawanMac::GetBeaconEnabled() const
{
    NS_LOG_FUNCTION(this);

    return m_beaconEnabled;
}

void
GatewayLorawanMac::SendBeacon()
{
    NS_LOG_FUNCTION(this);

    LoraBeaconHeader beaconHdr;
    beaconHdr.SetTime(static_cast<uint32_t>(Simulator::Now().GetSeconds()));

    Ptr<Packet> packet = Create<Packet>(0);
    packet->AddHeader(beaconHdr);

    LoraTag tag;
    tag.SetDataRate(BEACON_DATA_RATE);
    tag.SetFrequency(BEACON_FREQUENCY_HZ);
    packet->AddPacketTag(tag);

    // Reuses the regular Send() path: normal duty-cycle bookkeeping applies, and if it
    // happens to block this particular beacon, Send() logs a warning and drops just this
    // one -- the next attempt below is scheduled unconditionally regardless.
    Send(packet);

    m_nextBeacon = Simulator::Schedule(m_beaconInterval, &GatewayLorawanMac::SendBeacon, this);
}

} // namespace lorawan
} // namespace ns3
