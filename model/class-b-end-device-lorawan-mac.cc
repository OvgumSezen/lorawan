/*
 * Copyright (c) 2026 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 *         Martina Capuzzo <capuzzom@dei.unipd.it>
 */

#include "class-b-end-device-lorawan-mac.h"

#include "end-device-lora-phy.h"
#include "lora-beacon-header.h"
#include "lora-tag.h"

#include "ns3/hash.h"
#include "ns3/simulator.h"

#include <limits>

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("ClassBEndDeviceLorawanMac");

NS_OBJECT_ENSURE_REGISTERED(ClassBEndDeviceLorawanMac);

const Time ClassBEndDeviceLorawanMac::BEACON_INTERVAL = Seconds(128);

TypeId
ClassBEndDeviceLorawanMac::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::ClassBEndDeviceLorawanMac")
            .SetParent<EndDeviceLorawanMac>()
            .SetGroupName("lorawan")
            .AddConstructor<ClassBEndDeviceLorawanMac>()
            .AddAttribute("PingSlotPeriodicity",
                          "This device's configured ping slot periodicity, in [0, 7]. The "
                          "device opens 2^(7 - periodicity) ping slots per beacon period.",
                          UintegerValue(4),
                          MakeUintegerAccessor(&ClassBEndDeviceLorawanMac::m_pingSlotPeriodicity),
                          MakeUintegerChecker<uint8_t>(0, 7))
            .AddTraceSource("Synchronized",
                            "Whether this device currently has a valid, recent beacon time "
                            "reference. Fires only on an actual synchronized/desynchronized "
                            "transition, not on every beacon reception/miss.",
                            MakeTraceSourceAccessor(&ClassBEndDeviceLorawanMac::m_synchronized),
                            "ns3::TracedValueCallback::Bool")
            .AddTraceSource(
                "MissedBeaconCount",
                "The number of consecutive missed beacon windows since the last "
                "successfully received one.",
                MakeTraceSourceAccessor(&ClassBEndDeviceLorawanMac::m_missedBeaconCount),
                "ns3::TracedValueCallback::uint8_t")
            .AddTraceSource(
                "BeaconWindowSkipped",
                "A scheduled beacon window was skipped (not opened at all) "
                "because the device was busy with its own ack sequence.",
                MakeTraceSourceAccessor(&ClassBEndDeviceLorawanMac::m_beaconWindowSkipped),
                "ns3::Time::TracedCallback")
            .AddTraceSource("PingSlotSkipped",
                            "A scheduled ping-slot window was skipped (not opened at all) "
                            "because the device was busy with its own ack sequence.",
                            MakeTraceSourceAccessor(&ClassBEndDeviceLorawanMac::m_pingSlotSkipped),
                            "ns3::Time::TracedCallback");
    return tid;
}

ClassBEndDeviceLorawanMac::ClassBEndDeviceLorawanMac()
    : // LoraWAN defaults, same as Class A -- the own-uplink ack sequence is identical
      m_receiveDelay1(Seconds(1)),
      m_receiveDelay2(Seconds(2)),
      m_busy(false),
      // Class B-specific state: unsynchronized until a beacon is actually received
      m_awaitingBeacon(false),
      m_synchronized(false),
      m_missedBeaconCount(0),
      m_lastBeaconTime(Time()),
      m_pingSlotPeriodicity(4), // matches the "PingSlotPeriodicity" attribute's own default
      m_awaitingPingSlot(false),
      m_pingSlotsRemainingThisPeriod(0),
      m_nextPingSlotNominalTime(Time()),
      m_pingSlotInfoReqQueued(false)
{
    NS_LOG_FUNCTION(this);
    // Void the RX2 event
    m_secondReceiveWindow = EventId();
    m_secondReceiveWindow.Cancel();
    // Void the next ping slot event
    m_nextPingSlot = EventId();
    m_nextPingSlot.Cancel();
    // Start trying to acquire the network's beacon right away -- see the class-level Doxygen
    // comment for why this isn't gated on the device's own first uplink the way Class C's
    // continuous listening is.
    StartBeaconSearch();
}

ClassBEndDeviceLorawanMac::~ClassBEndDeviceLorawanMac()
{
    NS_LOG_FUNCTION_NOARGS();
}

bool
ClassBEndDeviceLorawanMac::IsSynchronized() const
{
    NS_LOG_FUNCTION(this);
    return m_synchronized;
}

uint8_t
ClassBEndDeviceLorawanMac::GetPingSlotPeriodicity() const
{
    NS_LOG_FUNCTION(this);
    return m_pingSlotPeriodicity;
}

uint16_t
ClassBEndDeviceLorawanMac::PingNbSlots(uint8_t periodicity)
{
    NS_LOG_FUNCTION(unsigned(periodicity));
    NS_ASSERT_MSG(periodicity <= 7, "periodicity must be in [0, 7]");
    return uint16_t(1) << (7 - periodicity);
}

Time
ClassBEndDeviceLorawanMac::ComputePingOffset(LoraDeviceAddress devAddr,
                                             uint32_t beaconTime,
                                             Time pingPeriod)
{
    NS_LOG_FUNCTION(devAddr.Print() << beaconTime << pingPeriod);
    NS_ASSERT_MSG(pingPeriod.IsStrictlyPositive(), "pingPeriod must be positive");

    uint32_t addr = devAddr.Get();
    uint8_t buffer[8] = {
        uint8_t(addr >> 24),
        uint8_t(addr >> 16),
        uint8_t(addr >> 8),
        uint8_t(addr),
        uint8_t(beaconTime >> 24),
        uint8_t(beaconTime >> 16),
        uint8_t(beaconTime >> 8),
        uint8_t(beaconTime),
    };

    uint32_t hash = Hash32(reinterpret_cast<const char*>(buffer), sizeof(buffer));
    // Bounding the offset by pingPeriod itself (not, e.g., by a slot count) is what guarantees
    // a period's last ping slot always lands strictly before the next beacon is due. Millisecond
    // granularity (rather than, e.g., nanoseconds) is deliberate: hash is only 32 bits wide, and
    // any realistic pingPeriod expressed in nanoseconds exceeds 2^32, which would make "hash %
    // pingPeriodNs" a no-op (hash is always less than the modulus) and collapse the offset's
    // range to a small fraction of the period. Milliseconds keep the modulus comfortably within
    // uint32_t range for every valid periodicity (max pingPeriod is the full 128s
    // BEACON_INTERVAL = 128'000ms), so the full 32 bits of hash entropy actually get used.
    int64_t pingPeriodMs = pingPeriod.GetMilliSeconds();
    NS_ASSERT_MSG(pingPeriodMs > 0 && pingPeriodMs <= std::numeric_limits<uint32_t>::max(),
                  "pingPeriod out of the range this millisecond-granularity computation expects");
    return MilliSeconds(hash % uint32_t(pingPeriodMs));
}

Time
ClassBEndDeviceLorawanMac::GetNextClassTransmissionDelay() const
{
    NS_LOG_FUNCTION(this);
    if (m_busy) // device is in the process of sending and opening RX windows
    {
        NS_LOG_WARN("Attempting to send when device is still busy, postponed by 5s.");
        return Seconds(5);
    }
    // A beacon/ping-slot window may be open even though the device isn't "busy" (that flag
    // only tracks the own-uplink ack sequence). Unlike Class C's continuous listening, these
    // windows are RXSINGLE-based and cannot be interrupted early -- there is no
    // StopReceiveContinuous() equivalent for RXSINGLE mode -- so both an idle window
    // (RX_ENABLED) and an active reception within one (RX_ACTIVE) must be waited out.
    if (auto phyState = DynamicCast<EndDeviceLoraPhy>(m_phy)->GetState();
        phyState == EndDeviceLoraPhy::State::RX_ENABLED ||
        phyState == EndDeviceLoraPhy::State::RX_ACTIVE)
    {
        NS_LOG_WARN("Attempting to send during a beacon/ping-slot window, postponed by 5s.");
        return Seconds(5);
    }
    return Time();
}

void
ClassBEndDeviceLorawanMac::SendToPhy(Ptr<Packet> packetToSend)
{
    NS_LOG_DEBUG("PacketToSend: " << packetToSend);

    // Lock the MAC layer during the TX -> RX1 -> RX2 process
    NS_ASSERT_MSG(!m_busy, "Trying to send while device is already marked busy");
    m_busy = true;

    /////////////////////////
    // Prepare TX parameters
    /////////////////////////

    auto sf = GetSfFromDataRate(m_dataRate);
    auto bw = GetBandwidthFromDataRate(m_dataRate);
    // see SX1272/73 Datasheet, Section 4.1.1.6, Rev. 4, Jan. 2019
    auto ldro = bool((sf == 11 || sf == 12) && bw == 125'000);

    // Craft LoraTxParameters object
    LoraTxParameters params;
    params.spreadingFactor = sf;
    params.bandwidthHz = bw;
    params.codingRate = m_codingRate;
    params.lowDataRateOptimize = ldro;
    params.preambleLenSymb = m_nPreambleSymbols;
    params.implicitHeader = m_headerDisabled;
    params.crcEnabled = true;

    // Select random frequency channel
    Ptr<LogicalLoraChannel> txChannel = GetRandomChannelForTx();

    ///////////////////////////////////////////////
    // Register packet transmission for duty cycle
    ///////////////////////////////////////////////

    // Compute tx duration for duty-cycle management
    Time duration = LoraPhy::GetTimeOnAir(packetToSend->GetSize(), params);
    // Register the tx duration into the LogicalLoraChannelHelper
    m_channelHelper->AddEvent(duration, txChannel);

    /////////////////////////////////////////
    // Store dynamic RX1 window parameters //
    /////////////////////////////////////////

    // Switch the PHY to the channel so that it will listen here for downlink
    m_firstReceiveWindowFrequencyHz = txChannel->GetFrequency();

    /////////////////////
    // Init transmission
    /////////////////////

    // Check that the PHY layer is in an expected state
    auto phyState = DynamicCast<EndDeviceLoraPhy>(m_phy)->GetState();
    NS_ASSERT_MSG(phyState == EndDeviceLoraPhy::State::SLEEP ||
                      phyState == EndDeviceLoraPhy::State::STANDBY,
                  "Busy PHY device (not in SLEEP or STANDBY state): phyState=" << phyState);
    // Wake up PHY layer and directly send the packet
    m_phy->Send(packetToSend, txChannel->GetFrequency(), IQPolarity::UP, params, m_txPowerDbm);
}

void
ClassBEndDeviceLorawanMac::TxFinished(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    // We should always be in STANDBY mode at this point, as this function is meant to be
    // invoked as a callback by the PHY on transmission end
    auto phyState = DynamicCast<EndDeviceLoraPhy>(m_phy)->GetState();
    NS_ASSERT_MSG(phyState == EndDeviceLoraPhy::State::STANDBY,
                  "Unexpected PHY state on TX conclusion: phyState=" << phyState);
    // Switch the PHY to sleep
    DynamicCast<EndDeviceLoraPhy>(m_phy)->Sleep();

    // Schedule the opening of the first receive window
    Simulator::Schedule(m_receiveDelay1, &ClassBEndDeviceLorawanMac::OpenFirstReceiveWindow, this);

    // Schedule the opening of the second receive window
    m_secondReceiveWindow = Simulator::Schedule(m_receiveDelay2,
                                                &ClassBEndDeviceLorawanMac::OpenSecondReceiveWindow,
                                                this);
}

void
ClassBEndDeviceLorawanMac::OpenFirstReceiveWindow()
{
    NS_LOG_FUNCTION(this);
    // We should always be in SLEEP mode at this point
    auto phyState = DynamicCast<EndDeviceLoraPhy>(m_phy)->GetState();
    NS_ASSERT_MSG(phyState == EndDeviceLoraPhy::State::SLEEP,
                  "Unexpected PHY state on RX1 opening: phyState=" << phyState);

    // Gather the parameters required for the first reception window
    auto rx1DataRate = GetFirstReceiveWindowDataRate();
    NS_LOG_DEBUG("m_dataRate=" << unsigned(m_dataRate)
                               << ", m_rx1DrOffset=" << unsigned(m_rx1DrOffset)
                               << ", rx1DataRate=" << unsigned(rx1DataRate));
    // Request a timed reception from the PHY
    DynamicCast<EndDeviceLoraPhy>(m_phy)->ReceiveSingle(
        m_firstReceiveWindowFrequencyHz,
        IQPolarity::DOWN,
        GetSfFromDataRate(rx1DataRate),
        GetBandwidthFromDataRate(rx1DataRate),
        m_receiveWindowDurationInSymbols,
        MakeCallback(&ClassBEndDeviceLorawanMac::CloseFirstReceiveWindow, this));
}

void
ClassBEndDeviceLorawanMac::CloseFirstReceiveWindow()
{
    NS_LOG_FUNCTION(this);
    // We should always be in STANDBY mode at this point, as this function is meant to be
    // invoked as a callback by the PHY on reception timeout
    auto phyState = DynamicCast<EndDeviceLoraPhy>(m_phy)->GetState();
    NS_ASSERT_MSG(phyState == EndDeviceLoraPhy::State::STANDBY,
                  "Unexpected PHY state on RX1 closure: phyState=" << phyState);
    DynamicCast<EndDeviceLoraPhy>(m_phy)->Sleep();
}

void
ClassBEndDeviceLorawanMac::OpenSecondReceiveWindow()
{
    NS_LOG_FUNCTION(this);
    // We might be in SLEEP or in RX_ACTIVE mode at this point
    auto phyState = DynamicCast<EndDeviceLoraPhy>(m_phy)->GetState();
    NS_ASSERT_MSG(phyState == EndDeviceLoraPhy::State::SLEEP ||
                      phyState == EndDeviceLoraPhy::State::RX_ACTIVE,
                  "Unexpected PHY state on RX2 opening: phyState=" << phyState);

    // Return immediately if a reception started on RX1 has not concluded yet
    if (phyState == EndDeviceLoraPhy::State::RX_ACTIVE)
    {
        return;
    }

    // Gather the parameters required for the second reception window
    NS_LOG_DEBUG("m_secondReceiveWindowFrequencyHz=" << m_secondReceiveWindowFrequencyHz
                                                     << ", m_secondReceiveWindowDataRate="
                                                     << unsigned(m_secondReceiveWindowDataRate));
    // Request a timed reception from the PHY
    DynamicCast<EndDeviceLoraPhy>(m_phy)->ReceiveSingle(
        m_secondReceiveWindowFrequencyHz,
        IQPolarity::DOWN,
        GetSfFromDataRate(m_secondReceiveWindowDataRate),
        GetBandwidthFromDataRate(m_secondReceiveWindowDataRate),
        m_receiveWindowDurationInSymbols,
        MakeCallback(&ClassBEndDeviceLorawanMac::CloseSecondReceiveWindow, this));
}

void
ClassBEndDeviceLorawanMac::CloseSecondReceiveWindow()
{
    NS_LOG_FUNCTION(this);
    // We should always be in STANDBY mode at this point, as this function is meant to be
    // invoked as a callback by the PHY on reception timeout
    auto phyState = DynamicCast<EndDeviceLoraPhy>(m_phy)->GetState();
    NS_ASSERT_MSG(phyState == EndDeviceLoraPhy::State::STANDBY,
                  "Unexpected PHY state on RX2 closure: phyState=" << phyState);
    DynamicCast<EndDeviceLoraPhy>(m_phy)->Sleep();

    // We are here if no reception happened. Unlike Class A (which just sleeps until the next
    // application-triggered send) or Class C (which enters continuous listening), a Class B
    // device's own idle state is
    // whatever its already-independently-scheduled beacon/ping-slot window dictates -- this
    // just needs to clear m_busy and get out of the way; there is nothing to schedule here.
    ManageRetransmissions(NONE);
    // Open the context to new transmissions
    m_busy = false;
}

void
ClassBEndDeviceLorawanMac::ManageRetransmissions(RxOutcome outcome)
{
    NS_LOG_FUNCTION(this << outcome);

    bool recv = (outcome == RECV || outcome == ACK); // We received something
    bool needsAck = m_txContext.needsAck;            // We were waiting for acknowledgement
    bool gotAck = (outcome == ACK);                  // We got acknowledgement
    bool canReTx = (m_txContext.nbTxLeft > 0 && !m_nextTx.IsPending()); // We can retransmit
    NS_LOG_DEBUG("recv=" << recv << ", needsAck=" << needsAck << ", gotAck=" << gotAck
                         << ", canReTx=" << canReTx);

    // Condition to schedule retransmission:
    // either we did not receive or we weren't acknowledged + we can retransmit
    if ((!recv || (needsAck && !gotAck)) && canReTx)
    {
        if (outcome == RECV)
        {
            NS_LOG_DEBUG("Received packet without ACK: rescheduling transmission.");
        }
        else if (outcome == FAIL)
        {
            NS_LOG_DEBUG("Reception failed: rescheduling transmission.");
        }
        else if (outcome == NONE)
        {
            NS_LOG_DEBUG("No reception initiated by PHY: rescheduling transmission.");
        }
        NS_LOG_INFO("We have " << unsigned(m_txContext.nbTxLeft) << " retransmissions left.");
        double retransmitTimeout = m_uniformRV->GetValue(1, 3);
        PostponeTransmission(Seconds(retransmitTimeout), m_txContext.packet);
        return;
    }

    // Tracing: end of re-transmission process
    uint8_t txs = m_nbTrans - m_txContext.nbTxLeft;
    // Acknowledgement success of confirmed txs
    if (recv && needsAck && gotAck)
    {
        m_confirmedTxOutcomeCallback(txs, true, m_txContext.firstAttempt, m_txContext.packet);
        NS_LOG_DEBUG("Received ACK packet after "
                     << unsigned(txs) << " transmissions: stopping retransmission process");
    }
    // Acknowledgement failure of confirmed txs
    // (either exhausted all reTxs or new pkt scheduled while busy)
    else if (needsAck && !gotAck && !canReTx)
    {
        m_confirmedTxOutcomeCallback(txs, false, m_txContext.firstAttempt, m_txContext.packet);
        NS_LOG_DEBUG("Ack failure: no more retransmission opportunities. Used "
                     << unsigned(txs) << " transmissions.");
    }

    // Exhaust remaining re-transmissions
    m_txContext.nbTxLeft = 0;

    // Update uplink frame counter
    m_fCnt++;
    // Update ADRACKCnt only if nothing was received
    if (!recv)
    {
        m_adrAckCnt++;
    }
}

void
ClassBEndDeviceLorawanMac::StartBeaconSearch()
{
    NS_LOG_FUNCTION(this);
    Simulator::Schedule(TimeUntilNextBeaconBoundary(),
                        &ClassBEndDeviceLorawanMac::OpenBeaconWindow,
                        this);
}

void
ClassBEndDeviceLorawanMac::OpenBeaconWindow()
{
    NS_LOG_FUNCTION(this);

    if (m_busy)
    {
        // The device is mid ack-sequence; the two schedules are independent and can collide.
        // Skip this occurrence rather than corrupt PHY state.
        NS_LOG_DEBUG("Skipping beacon window: device is busy with its own ack sequence.");
        m_beaconWindowSkipped(Simulator::Now());
        Simulator::Schedule(TimeUntilNextBeaconBoundary(),
                            &ClassBEndDeviceLorawanMac::OpenBeaconWindow,
                            this);
        return;
    }

    auto phyState = DynamicCast<EndDeviceLoraPhy>(m_phy)->GetState();
    NS_ASSERT_MSG(phyState == EndDeviceLoraPhy::State::SLEEP ||
                      phyState == EndDeviceLoraPhy::State::STANDBY,
                  "Unexpected PHY state on beacon window opening: phyState=" << phyState);

    m_awaitingBeacon = true;
    DynamicCast<EndDeviceLoraPhy>(m_phy)->ReceiveSingle(
        BEACON_FREQUENCY_HZ,
        IQPolarity::DOWN,
        GetSfFromDataRate(BEACON_DATA_RATE),
        GetBandwidthFromDataRate(BEACON_DATA_RATE),
        m_receiveWindowDurationInSymbols,
        MakeCallback(&ClassBEndDeviceLorawanMac::CloseBeaconWindow, this));
}

void
ClassBEndDeviceLorawanMac::CloseBeaconWindow()
{
    NS_LOG_FUNCTION(this);
    // We should always be in STANDBY mode at this point, as this function is meant to be
    // invoked as a callback by the PHY on reception timeout (nothing ever locked onto a
    // preamble during the whole window -- a clean miss).
    auto phyState = DynamicCast<EndDeviceLoraPhy>(m_phy)->GetState();
    NS_ASSERT_MSG(phyState == EndDeviceLoraPhy::State::STANDBY,
                  "Unexpected PHY state on beacon window closure: phyState=" << phyState);
    DynamicCast<EndDeviceLoraPhy>(m_phy)->Sleep();

    m_awaitingBeacon = false;
    OnBeaconWindowMissed();
}

void
ClassBEndDeviceLorawanMac::OnBeaconWindowMissed()
{
    NS_LOG_FUNCTION(this);

    if (m_missedBeaconCount < 255)
    {
        m_missedBeaconCount = m_missedBeaconCount + 1;
    }
    if (m_missedBeaconCount >= 2 && m_synchronized)
    {
        NS_LOG_INFO("2 consecutive missed beacon windows: marking device desynchronized.");
        m_synchronized = false;
        // No point keeping a future ping slot scheduled for a period whose offset was computed
        // from a beacon time reference we can no longer trust.
        if (m_nextPingSlot.IsPending())
        {
            m_nextPingSlot.Cancel();
        }
    }

    // Keep trying to reacquire the beacon regardless of (de)synchronization state.
    Simulator::Schedule(TimeUntilNextBeaconBoundary(),
                        &ClassBEndDeviceLorawanMac::OpenBeaconWindow,
                        this);
}

Time
ClassBEndDeviceLorawanMac::TimeUntilNextBeaconBoundary() const
{
    NS_LOG_FUNCTION(this);
    int64_t nowNs = Simulator::Now().GetNanoSeconds();
    int64_t intervalNs = BEACON_INTERVAL.GetNanoSeconds();
    int64_t elapsedInPeriodNs = nowNs % intervalNs;
    // If already exactly on a boundary (e.g., at simulation time 0), wait a full interval for
    // the *next* one, not this instant.
    return NanoSeconds(intervalNs - elapsedInPeriodNs);
}

Time
ClassBEndDeviceLorawanMac::PingPeriod() const
{
    NS_LOG_FUNCTION(this);
    return BEACON_INTERVAL / PingNbSlots(m_pingSlotPeriodicity);
}

void
ClassBEndDeviceLorawanMac::OpenPingSlotWindow()
{
    NS_LOG_FUNCTION(this);

    if (m_busy)
    {
        // The device is mid ack-sequence; skip this occurrence rather than corrupt PHY state.
        NS_LOG_DEBUG("Skipping ping slot: device is busy with its own ack sequence.");
        m_pingSlotSkipped(Simulator::Now());
        AdvanceToNextPingSlot();
        return;
    }

    auto phyState = DynamicCast<EndDeviceLoraPhy>(m_phy)->GetState();
    NS_ASSERT_MSG(phyState == EndDeviceLoraPhy::State::SLEEP ||
                      phyState == EndDeviceLoraPhy::State::STANDBY,
                  "Unexpected PHY state on ping slot window opening: phyState=" << phyState);

    m_awaitingPingSlot = true;
    DynamicCast<EndDeviceLoraPhy>(m_phy)->ReceiveSingle(
        BEACON_FREQUENCY_HZ,
        IQPolarity::DOWN,
        GetSfFromDataRate(BEACON_DATA_RATE),
        GetBandwidthFromDataRate(BEACON_DATA_RATE),
        m_receiveWindowDurationInSymbols,
        MakeCallback(&ClassBEndDeviceLorawanMac::ClosePingSlotWindow, this));
}

void
ClassBEndDeviceLorawanMac::ClosePingSlotWindow()
{
    NS_LOG_FUNCTION(this);
    // We should always be in STANDBY mode at this point, as this function is meant to be
    // invoked as a callback by the PHY on reception timeout (a clean miss -- nothing ever
    // locked onto a preamble). Unlike a missed beacon, this has no bearing on synchronization.
    auto phyState = DynamicCast<EndDeviceLoraPhy>(m_phy)->GetState();
    NS_ASSERT_MSG(phyState == EndDeviceLoraPhy::State::STANDBY,
                  "Unexpected PHY state on ping slot window closure: phyState=" << phyState);
    DynamicCast<EndDeviceLoraPhy>(m_phy)->Sleep();

    m_awaitingPingSlot = false;
    AdvanceToNextPingSlot();
}

void
ClassBEndDeviceLorawanMac::AdvanceToNextPingSlot()
{
    NS_LOG_FUNCTION(this);

    if (m_pingSlotsRemainingThisPeriod > 0)
    {
        m_pingSlotsRemainingThisPeriod--;
    }

    if (!m_synchronized || m_pingSlotsRemainingThisPeriod == 0)
    {
        // Either desynchronized (stop trying until the next successful beacon), or this
        // period's allotment of slots is exhausted (the next successful beacon reception, see
        // Receive(), computes a fresh offset and restarts the sequence for the new period).
        return;
    }

    m_nextPingSlotNominalTime += PingPeriod();
    Time delay = Max(m_nextPingSlotNominalTime - Simulator::Now(), Time(0));
    m_nextPingSlot =
        Simulator::Schedule(delay, &ClassBEndDeviceLorawanMac::OpenPingSlotWindow, this);
}

void
ClassBEndDeviceLorawanMac::Receive(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    // We should always be in STANDBY mode at this point, as this function is meant to be invoked
    // as a callback by the PHY on reception end. Unlike Class C, none of Class B's receive
    // windows (own-uplink ack sequence, beacon, ping slot) ever use RXCONTINUOUS, so there is no
    // auto-resume-to-RX_ENABLED subtlety to account for here: every window is ReceiveSingle-
    // based and always concludes in STANDBY before this callback fires, exactly like Class A.
    auto phyState = DynamicCast<EndDeviceLoraPhy>(m_phy)->GetState();
    NS_ASSERT_MSG(phyState == EndDeviceLoraPhy::State::STANDBY,
                  "Unexpected PHY state on RX end: phyState=" << phyState);

    if (m_awaitingPingSlot)
    {
        // A ping-slot downlink *is* a normal LoRaWAN frame (MHDR+FHDR+payload), addressed to
        // this device -- unlike a beacon. Parse and forward it just like an RX1/RX2 reception
        // would, but without any of the ack-sequence-specific bookkeeping (m_busy,
        // m_secondReceiveWindow, retransmissions) that doesn't apply outside that sequence.
        Ptr<Packet> pingSlotPacketCopy = packet->Copy();
        LorawanMacHeader pingSlotMHdr;
        pingSlotPacketCopy->RemoveHeader(pingSlotMHdr);
        NS_ASSERT_MSG(!pingSlotMHdr.IsUplink(), "Received uplink package, check PHY polarity");
        LoraFrameHeader pingSlotFHdr;
        pingSlotFHdr.SetAsDownlink();
        pingSlotPacketCopy->RemoveHeader(pingSlotFHdr);

        if (m_address != pingSlotFHdr.GetAddress())
        {
            NS_LOG_DEBUG("The ping-slot downlink is intended for another recipient.");
            FailedReception(packet);
            return;
        }

        m_awaitingPingSlot = false;
        DynamicCast<EndDeviceLoraPhy>(m_phy)->Sleep();
        NS_LOG_INFO("Ping-slot downlink is for us!");

        LoraTag pingSlotTag;
        packet->PeekPacketTag(pingSlotTag);
        /// @see ns3::lorawan::AdrComponent::RxPowerToSNR
        m_lastRxSnr = pingSlotTag.GetReceivePower() + 174 - 10 * log10(125000) - 6;

        ApplyMACCommands(pingSlotFHdr);

        if (!m_receiveCallback.IsNull())
        {
            m_receiveCallback(pingSlotPacketCopy);
        }
        m_receivedPacket(packet);

        AdvanceToNextPingSlot();
        return;
    }

    if (m_awaitingBeacon)
    {
        // A beacon is not a normal LoRaWAN frame (no MHDR/FHDR, no DevAddr) -- parse it as a
        // LoraBeaconHeader instead of falling through to the normal frame-parsing path below.
        Ptr<Packet> packetCopy = packet->Copy();
        LoraBeaconHeader beaconHdr;
        packetCopy->RemoveHeader(beaconHdr);
        NS_LOG_INFO("Beacon received, Time=" << beaconHdr.GetTime());

        m_lastBeaconTime = Seconds(beaconHdr.GetTime());
        m_missedBeaconCount = 0;
        if (!m_synchronized)
        {
            NS_LOG_INFO("Device (re)synchronized to the network's beacon.");
            m_synchronized = true;
        }
        m_awaitingBeacon = false;

        // Compute this period's ping-slot schedule and (re)start it, discarding whatever was
        // left pending from the previous period (if any).
        uint16_t pingNbSlots = PingNbSlots(m_pingSlotPeriodicity);
        Time pingPeriod = PingPeriod();
        Time offset = ComputePingOffset(m_address, beaconHdr.GetTime(), pingPeriod);
        NS_LOG_DEBUG("Ping slot schedule for this period: pingNbSlots="
                     << pingNbSlots << ", pingPeriod=" << pingPeriod.As(Time::S)
                     << ", offset=" << offset.As(Time::S));
        m_pingSlotsRemainingThisPeriod = pingNbSlots;
        m_nextPingSlotNominalTime = m_lastBeaconTime + offset;
        if (m_nextPingSlot.IsPending())
        {
            m_nextPingSlot.Cancel();
        }
        m_nextPingSlot =
            Simulator::Schedule(Max(m_nextPingSlotNominalTime - Simulator::Now(), Time(0)),
                                &ClassBEndDeviceLorawanMac::OpenPingSlotWindow,
                                this);

        DynamicCast<EndDeviceLoraPhy>(m_phy)->Sleep();
        Simulator::Schedule(TimeUntilNextBeaconBoundary(),
                            &ClassBEndDeviceLorawanMac::OpenBeaconWindow,
                            this);
        return;
    }

    // Work on a copy of the packet
    Ptr<Packet> packetCopy = packet->Copy();

    // Remove the Mac Header to get some information
    LorawanMacHeader mHdr;
    packetCopy->RemoveHeader(mHdr);
    NS_ASSERT_MSG(!mHdr.IsUplink(), "Received uplink package, check PHY polarity");
    NS_LOG_DEBUG("Downlink Mac Header: " << mHdr);
    // Remove the Frame Header
    LoraFrameHeader fHdr;
    fHdr.SetAsDownlink();
    packetCopy->RemoveHeader(fHdr);
    NS_LOG_DEBUG("Downlink Frame Header: " << fHdr);

    /// TODO: early packet filtering at PHY layer

    // Determine whether this packet is for us
    if (m_address != fHdr.GetAddress())
    {
        NS_LOG_DEBUG("The message is intended for another recipient.");
        FailedReception(packet);
        return;
    }

    // Set PHY to sleep
    DynamicCast<EndDeviceLoraPhy>(m_phy)->Sleep();

    NS_LOG_INFO("The message is for us!");
    // If it exists, cancel the second receive window event
    m_secondReceiveWindow.Cancel();
    // Open the context to new transmissions
    m_busy = false;
    // Reset ADR backoff counter
    m_adrAckCnt = 0;
    // Clear commands that are re-sent until downlink (DlChannelAns and RxTimingSetupAns)
    m_macCommandList.clear();

    // Link quality metadata
    LoraTag tag;
    packet->PeekPacketTag(tag);
    /// @see ns3::lorawan::AdrComponent::RxPowerToSNR
    m_lastRxSnr = tag.GetReceivePower() + 174 - 10 * log10(125000) - 6;

    // Parse the MAC commands
    ApplyMACCommands(fHdr);
    // Manage acknowledgement and retransmission
    ManageRetransmissions(fHdr.GetAck() ? ACK : RECV);

    // Pass the packet up to the NetDevice
    if (!m_receiveCallback.IsNull())
    {
        m_receiveCallback(packetCopy);
    }
    // Call the trace source
    m_receivedPacket(packet);
}

void
ClassBEndDeviceLorawanMac::FailedReception(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    // We should always be in STANDBY mode at this point, as this function is meant to be
    // invoked as a callback by the PHY on reception end (see the note in Receive())
    auto phyState = DynamicCast<EndDeviceLoraPhy>(m_phy)->GetState();
    NS_ASSERT_MSG(phyState == EndDeviceLoraPhy::State::STANDBY,
                  "Unexpected PHY state on RX end: phyState=" << phyState);
    // Switch to sleep after a failed reception
    DynamicCast<EndDeviceLoraPhy>(m_phy)->Sleep();

    if (m_awaitingPingSlot)
    {
        // Something locked onto a preamble during the ping slot but either failed to demodulate
        // correctly or (per Receive()) turned out to be addressed to someone else. Unlike a
        // missed beacon, this has no bearing on synchronization -- just move on to the next slot.
        NS_LOG_INFO("Failed to receive during ping slot.");
        m_awaitingPingSlot = false;
        AdvanceToNextPingSlot();
        return;
    }

    if (m_awaitingBeacon)
    {
        // Something locked onto a preamble during the beacon window but failed to demodulate
        // correctly (e.g., interference) -- counts as a miss, same as a clean timeout.
        NS_LOG_INFO("Failed to receive beacon.");
        m_awaitingBeacon = false;
        OnBeaconWindowMissed();
        return;
    }

    // Nothing valid was received; if we are past the 2nd RX window, we can reschedule
    if (m_secondReceiveWindow.IsExpired())
    {
        ManageRetransmissions(FAIL);
        // Open the context to new transmissions
        m_busy = false;
    }
}

void
ClassBEndDeviceLorawanMac::OnRxParamSetupReq(uint8_t rx1DrOffset,
                                             uint8_t rx2DataRate,
                                             double frequencyHz)
{
    NS_LOG_FUNCTION(this << unsigned(rx1DrOffset) << unsigned(rx2DataRate)
                         << uint32_t(frequencyHz));

    // Adapted from: github.com/Lora-net/SWL2001.git v4.3.1
    // For the time being, this implementation is valid for the EU868 region

    bool rx1DrOffsetAck = true;
    bool rx2DataRateAck = true;
    bool channelAck = true;

    if (rx1DrOffset >= m_replyDataRateMatrix.at(m_dataRate).size())
    {
        NS_LOG_WARN("Invalid rx1DrOffset");
        rx1DrOffsetAck = false;
    }

    if (!GetSfFromDataRate(rx2DataRate) || !GetBandwidthFromDataRate(rx2DataRate))
    {
        NS_LOG_WARN("Invalid rx2DataRate");
        rx2DataRateAck = false;
    }

    if (!m_channelHelper->IsFrequencyValid(frequencyHz))
    {
        NS_LOG_WARN("Invalid rx2 frequency");
        channelAck = false;
    }

    if (rx1DrOffsetAck && rx2DataRateAck && channelAck)
    {
        m_rx1DrOffset = rx1DrOffset;
        m_secondReceiveWindowDataRate = rx2DataRate;
        m_secondReceiveWindowFrequencyHz = frequencyHz;
    }

    NS_LOG_INFO("Adding RxParamSetupAns reply");
    m_macCommandList.emplace_back(
        Create<RxParamSetupAns>(rx1DrOffsetAck, rx2DataRateAck, channelAck));
}

void
ClassBEndDeviceLorawanMac::Send(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    if (!m_pingSlotInfoReqQueued)
    {
        // Let the Network Server know this device's chosen ping slot periodicity. Queued here
        // (rather than at construction) so it reflects m_pingSlotPeriodicity's actual value at
        // the time of the device's first real uplink, not whatever it happened to be built
        // with, and reaches the frame header about to be built for that very uplink rather
        // than only the *next* one after it. The existing m_macCommandList mechanism re-sends
        // it on every subsequent uplink until any downlink is received, same as it already
        // does for e.g. DlChannelAns/RxTimingSetupAns -- reasonable given the real spec's own
        // intent that a device keep announcing this until acknowledged. Purely informational:
        // it does not gate this device's own local ping-slot scheduling, which already runs
        // off m_pingSlotPeriodicity directly.
        AddMacCommand(Create<PingSlotInfoReq>(m_pingSlotPeriodicity));
        m_pingSlotInfoReqQueued = true;
    }

    EndDeviceLorawanMac::Send(packet);
}

} // namespace lorawan
} // namespace ns3
