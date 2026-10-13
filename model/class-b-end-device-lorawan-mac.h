/*
 * Copyright (c) 2026 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 *         Martina Capuzzo <capuzzom@dei.unipd.it>
 */

#ifndef CLASS_B_END_DEVICE_LORAWAN_MAC_H
#define CLASS_B_END_DEVICE_LORAWAN_MAC_H

#include "end-device-lorawan-mac.h"

namespace ns3
{
namespace lorawan
{

/**
 * @ingroup lorawan
 *
 * Class representing the MAC layer of a Class B LoRaWAN device.
 *
 * A Class B device's own transmit/RX1/RX2 sequence is identical to a Class A device's
 * (see ClassAEndDeviceLorawanMac); what makes it Class B is an additional, independent
 * schedule of beacon-acquisition and ping-slot receive windows, timed off the network's
 * shared beacon epoch rather than off the device's own transmissions. This device
 * acquires and tracks synchronization to the network's beacon (becoming, and staying,
 * "synchronized" -- see IsSynchronized()), and, once synchronized, opens PingNbSlots()
 * brief ping-slot windows per beacon period at a network-computable (but per-device
 * pseudo-random) offset, on top of which a downlink can be delivered at any time without
 * requiring a prior uplink from the device -- unlike Class A.
 *
 * Beacon acquisition starts unconditionally at construction (StartBeaconSearch(), called
 * from the constructor) rather than being gated on the device's own first uplink: unlike
 * Class C's continuous listening (which depends on completing an ack sequence first), the
 * beacon schedule is public and shared, so there is no equivalent bootstrap gap here.
 * Ping-slot scheduling, in turn, only starts once a beacon has actually been received (see
 * Receive()'s m_awaitingBeacon branch) -- there is no way to compute a device's ping-slot
 * offset without first learning the current beacon time.
 */
class ClassBEndDeviceLorawanMac : public EndDeviceLorawanMac
{
  public:
    /**
     *  Register this type.
     *  @return The object TypeId.
     */
    static TypeId GetTypeId();

    ClassBEndDeviceLorawanMac();           //!< Default constructor
    ~ClassBEndDeviceLorawanMac() override; //!< Destructor

    /**
     * Queues this device's one-time PingSlotInfoReq (declaring m_pingSlotPeriodicity) on the
     * very first call, before delegating to EndDeviceLorawanMac::Send() -- so it rides along on
     * the frame header about to be built for whatever the device's next outgoing uplink turns
     * out to be, capturing m_pingSlotPeriodicity's value at that time (i.e., after any
     * attribute configuration a scenario performs), not at construction time, when it may
     * still be an unconfigured default.
     *
     * @param packet The packet to send.
     */
    void Send(Ptr<Packet> packet) override;

    /**
     * The fixed frequency [Hz] this device listens for Class B beacons (and, by convention,
     * ping slots -- see GatewayLorawanMac::BEACON_FREQUENCY_HZ) on. Must match whatever the
     * gateways in the network are configured to beacon on; this module does not implement
     * BeaconFreqReq, so there is no way to negotiate a different value at runtime.
     */
    static constexpr uint32_t BEACON_FREQUENCY_HZ = 869525000;

    /**
     * The fixed data rate this device listens for Class B beacons on (DR3, i.e. SF9BW125 in
     * the EU868 region). See GatewayLorawanMac::BEACON_DATA_RATE.
     */
    static constexpr uint8_t BEACON_DATA_RATE = 3;

    /**
     * The interval this device assumes gateways beacon at (the real spec value). See
     * GatewayLorawanMac::BeaconInterval; this module does not implement BeaconFreqReq or an
     * equivalent for the interval, so this is a fixed assumption, not a negotiated value.
     */
    static const Time BEACON_INTERVAL;

    /**
     * Whether this device currently has a valid, recent beacon time reference (fewer than 2
     * consecutive missed beacon windows since the last successfully received one).
     *
     * @return True if synchronized, false otherwise.
     */
    bool IsSynchronized() const;

    /**
     * Get this device's currently-configured ping slot periodicity.
     *
     * Exposed so the Network Server can compute this device's ping-slot schedule on demand
     * (via the same public PingNbSlots()/ComputePingOffset() formula) from its own "cheating"
     * pointer to this MAC, the same documented perfect-synchronization assumption already
     * relied on elsewhere (e.g. EndDeviceStatus::IsClassB()).
     *
     * @return The ping slot periodicity, in [0, 7].
     */
    uint8_t GetPingSlotPeriodicity() const;

    /**
     * Compute the number of ping slots per beacon period for a given ping slot periodicity,
     * per the LoRaWAN spec formula: 2^(7 - periodicity).
     *
     * Public (and static) since the Network Server needs to run the exact same computation
     * (see this feature's design decision 8) to know when a given device's ping slots occur,
     * without needing a live MAC object to ask.
     *
     * @param periodicity The ping slot periodicity, in [0, 7].
     * @return The number of ping slots per beacon period.
     */
    static uint16_t PingNbSlots(uint8_t periodicity);

    /**
     * Compute a device's pseudo-random ping-slot offset for a given beacon period: how far
     * after the period's own start (i.e., after its beacon's Time field) this device's first
     * ping slot of the period falls. Subsequent slots follow every pingPeriod after that (see
     * PingPeriod()); bounding the offset itself by pingPeriod (not, e.g., by the number of
     * slots in a period) is what guarantees the last of a period's slots always lands strictly
     * before the next beacon is due, never colliding with it.
     *
     * Deterministic and computable identically by both the device and the Network Server from
     * public information alone (DevAddr, the period's beacon time, and the device's negotiated
     * periodicity, which determines pingPeriod) -- no protocol exchange of the offset itself is
     * needed, matching the real spec's intent. Uses ns-3's own well-mixed Hash32() over
     * (devAddr, beaconTime) rather than the spec's AES-128-based construction (see this
     * feature's design decision 1): still deterministic and evenly-distributed, without
     * implementing a block cipher for a detail that doesn't change observable network/energy
     * behavior.
     *
     * @param devAddr The device's address.
     * @param beaconTime The Time field of the beacon that opened this period.
     * @param pingPeriod The duration of one ping slot period (see PingPeriod()).
     * @return The offset, in [0, pingPeriod).
     */
    static Time ComputePingOffset(LoraDeviceAddress devAddr, uint32_t beaconTime, Time pingPeriod);

  private:
    /**
     * Set of possible outcomes of a reception window
     */
    enum RxOutcome
    {
        ACK,  //!< Correctly received a network acknowledgement
        RECV, //!< Correctly received a downlink packet (no ACK)
        FAIL, //!< Reception initiated but failed
        NONE  //!< Reception window timed out
    };

    Time GetNextClassTransmissionDelay() const override;
    void SendToPhy(Ptr<Packet> packet) override;
    void TxFinished(Ptr<const Packet> packet) override;

    /**
     * Perform operations needed to open the first receive window.
     */
    void OpenFirstReceiveWindow();

    /**
     * Perform operations needed to open the second receive window.
     */
    void OpenSecondReceiveWindow();

    /**
     * Perform operations needed to close the first receive window.
     */
    void CloseFirstReceiveWindow();

    /**
     * Perform operations needed to close the second receive window.
     */
    void CloseSecondReceiveWindow();

    /**
     * Decide whether we can retransmit based on reception outcome.
     *
     * @param outcome Outcome of the reception.
     */
    void ManageRetransmissions(RxOutcome outcome);

    void Receive(Ptr<const Packet> packet) override;
    void FailedReception(Ptr<const Packet> packet) override;
    void OnRxParamSetupReq(uint8_t rx1DrOffset, uint8_t rx2DataRate, double frequencyHz) override;

    /**
     * Begin trying to acquire the network's beacon, from deployment (called once, from the
     * constructor). Schedules the first beacon-window attempt at the next BEACON_INTERVAL
     * boundary from the current simulation time.
     */
    void StartBeaconSearch();

    /**
     * Open a beacon-acquisition receive window, unless the device is currently busy with its
     * own ack sequence (m_busy), in which case this occurrence is skipped and the next one is
     * scheduled instead -- the two schedules are independent and can collide.
     */
    void OpenBeaconWindow();

    /**
     * Callback for a beacon window's clean timeout (nothing ever locked onto a preamble during
     * the whole window). Counts as a miss, exactly like a failed reception during the window
     * (see FailedReception()) -- both funnel into OnBeaconWindowMissed().
     */
    void CloseBeaconWindow();

    /**
     * Shared bookkeeping for a missed beacon window, regardless of whether it was a clean
     * timeout (CloseBeaconWindow()) or a failed reception (FailedReception()): increments
     * m_missedBeaconCount, marks the device desynchronized once 2 consecutive misses have
     * accumulated, and schedules the next beacon-window attempt regardless of the outcome (a
     * desynchronized device keeps trying to reacquire the beacon indefinitely).
     */
    void OnBeaconWindowMissed();

    /**
     * Compute the delay from the current simulation time until the next BEACON_INTERVAL
     * boundary (i.e., until the next time a gateway is expected to transmit a beacon), assuming
     * gateways beacon on a fixed schedule anchored at simulation time 0 -- consistent with
     * GatewayLorawanMac's own beacon scheduling, and requiring no clock-drift modeling since
     * ns-3's simulation clock is exact (see this feature's design decision 1).
     *
     * @return The delay until the next beacon window should open.
     */
    Time TimeUntilNextBeaconBoundary() const;

    /**
     * The duration of one ping slot period at this device's currently-configured periodicity:
     * BEACON_INTERVAL / PingNbSlots(m_pingSlotPeriodicity).
     *
     * @return The ping slot period.
     */
    Time PingPeriod() const;

    /**
     * Open a ping-slot receive window, unless the device is currently busy with its own ack
     * sequence (m_busy), in which case this occurrence is skipped (still counted against
     * m_pingSlotsRemainingThisPeriod) and the next one is scheduled instead -- same reasoning
     * as OpenBeaconWindow().
     */
    void OpenPingSlotWindow();

    /**
     * Callback for a ping-slot window's clean timeout (nothing ever locked onto a preamble).
     * Just moves on to the next scheduled slot (see AdvanceToNextPingSlot()) -- unlike a missed
     * beacon, a missed ping slot has no bearing on synchronization state.
     */
    void ClosePingSlotWindow();

    /**
     * Shared bookkeeping for a concluded ping slot (however it concluded: clean timeout,
     * skipped because busy, successful reception, or failed reception): decrements
     * m_pingSlotsRemainingThisPeriod, and, if the device is still synchronized and slots remain
     * in the current period, schedules the next one PingPeriod() after the last one's nominal
     * (not actual) time -- so a reception's processing delay doesn't accumulate drift across a
     * period, the same self-correcting reasoning as TimeUntilNextBeaconBoundary(). Does nothing
     * further once the period's slots are exhausted or the device is desynchronized; the next
     * successful beacon reception (see Receive()) restarts the sequence from a fresh offset.
     */
    void AdvanceToNextPingSlot();

    EventId m_secondReceiveWindow; //!< The event of the second receive window opening, used
                                   //!< to cancel the second window in case the first one is
                                   //!< successful.

    // Reception window parameters (own-uplink ack sequence; identical role to Class A's)

    Time m_receiveDelay1; //!< The interval between when a packet is done sending and when the first
                          //!< receive window is opened.
    uint32_t m_firstReceiveWindowFrequencyHz; //!< The frequency [Hz] to listen on for the first
                                              //!< receive window. This value is set dynamically to
                                              //!< the last uplink transmission frequency.

    Time m_receiveDelay2; //!< The interval between when a packet is done sending and when the
                          //!< second receive window is opened.

    // Rescheduling purposes

    bool m_busy; //!< Whether the MAC layer is currently busy with the LoRaWAN process of
                 //!< transmitting an uplink packet and then opening two reception windows
                 //!< (own-uplink ack sequence only; independent of beacon/ping-slot state).

    // Class B-specific state

    bool m_awaitingBeacon; //!< Whether the PHY's currently-open (or just-concluded) receive
                           //!< window was opened by OpenBeaconWindow(), so that Receive()/
                           //!< FailedReception() know to parse a LoraBeaconHeader instead of a
                           //!< normal frame, and CloseBeaconWindow() knows a clean timeout
                           //!< belongs to this window rather than some other state.

    TracedValue<bool> m_synchronized; //!< Whether this device has a valid, recent beacon time
                                      //!< reference (fewer than 2 consecutive missed beacon
                                      //!< windows since the last successfully received one).
    TracedValue<uint8_t>
        m_missedBeaconCount; //!< Consecutive missed beacon windows since the last successfully
                             //!< received one; capped at 255 (does not wrap around).
    Time m_lastBeaconTime;   //!< Network time carried by the last successfully received beacon;
                             //!< meaningless if m_synchronized is (and has always been) false.
    uint8_t m_pingSlotPeriodicity; //!< This device's configured ping slot periodicity, in
                                   //!< [0, 7]; see PingNbSlots().

    bool m_awaitingPingSlot; //!< Same role as m_awaitingBeacon, but for the currently-open (or
                             //!< just-concluded) window having been opened by
                             //!< OpenPingSlotWindow() instead.
    EventId m_nextPingSlot;  //!< The event of the next scheduled ping-slot window opening, used
                             //!< to cancel a stale one when a new beacon restarts the sequence.
    uint16_t m_pingSlotsRemainingThisPeriod; //!< Number of ping slots (out of
                                             //!< PingNbSlots(m_pingSlotPeriodicity)) still to be
                                             //!< attempted in the current beacon period.
    Time m_nextPingSlotNominalTime;          //!< The exact (drift-free) intended time of the next
                                    //!< scheduled ping slot, tracked independently of when its
                                    //!< predecessor actually finished being processed.

    /**
     * Trace source fired when a scheduled beacon window is skipped (not opened at all)
     * because the device is busy with its own ack sequence at the time.
     */
    TracedCallback<Time> m_beaconWindowSkipped;

    /**
     * Trace source fired when a scheduled ping-slot window is skipped (not opened at all)
     * because the device is busy with its own ack sequence at the time.
     */
    TracedCallback<Time> m_pingSlotSkipped;

    bool m_pingSlotInfoReqQueued; //!< Whether DoSend() has already queued this device's one-time
                                  //!< PingSlotInfoReq (see DoSend()'s doc comment).
};

} /* namespace lorawan */
} /* namespace ns3 */

#endif /* CLASS_B_END_DEVICE_LORAWAN_MAC_H */
