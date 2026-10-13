/*
 * Copyright (c) 2017 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 */

#ifndef GATEWAY_LORAWAN_MAC_H
#define GATEWAY_LORAWAN_MAC_H

#include "lorawan-mac.h"

namespace ns3
{
namespace lorawan
{

/**
 * @ingroup lorawan
 *
 * Class representing the MAC layer of a LoRaWAN gateway.
 */
class GatewayLorawanMac : public LorawanMac
{
  public:
    /**
     *  Register this type.
     *  @return The object TypeId.
     */
    static TypeId GetTypeId();

    GatewayLorawanMac();           //!< Default constructor
    ~GatewayLorawanMac() override; //!< Destructor

    /**
     * The fixed frequency [Hz] Class B beacons are transmitted on (and, by convention, that
     * Class B end devices listen for beacons and ping slots on, since this module does not
     * implement PingSlotChannelReq/BeaconFreqReq).
     */
    static constexpr uint32_t BEACON_FREQUENCY_HZ = 869525000;

    /**
     * The fixed data rate Class B beacons are transmitted with (DR3, i.e. SF9BW125 in the
     * EU868 region).
     */
    static constexpr uint8_t BEACON_DATA_RATE = 3;

    void Send(Ptr<Packet> packet) override;

    /**
     * Check whether the underlying PHY layer of the gateway is currently transmitting.
     *
     * @return True if it is transmitting, false otherwise.
     */
    bool IsTransmitting();

    /**
     * Return the next time at which we will be able to transmit on the specified frequency.
     *
     * @param frequencyHz The frequency value [Hz].
     * @return The next transmission time.
     */
    Time GetWaitTime(uint32_t frequencyHz);

    /**
     * Enable or disable periodic Class B beacon transmission from this gateway.
     *
     * Enabling schedules the first beacon transmission right away (i.e., at whatever
     * simulation time this is called); subsequent beacons follow automatically, spaced by
     * BeaconInterval, for as long as beaconing stays enabled.
     *
     * @param enable Whether beaconing should be enabled.
     */
    void SetBeaconEnabled(bool enable);

    /**
     * Get whether this gateway is currently transmitting periodic Class B beacons.
     *
     * @return True if beaconing is enabled.
     */
    bool GetBeaconEnabled() const;

  private:
    void TxFinished(Ptr<const Packet> packet) override;
    void Receive(Ptr<const Packet> packet) override;
    void FailedReception(Ptr<const Packet> packet) override;

    /**
     * Build and transmit one Class B beacon frame (see LoraBeaconHeader), then reschedule
     * this method to run again after BeaconInterval.
     *
     * Reuses the regular Send() path (and, with it, the regular duty-cycle bookkeeping): if
     * duty cycle happens to block a particular beacon, Send() logs a warning and drops just
     * that one, and the next attempt is still scheduled normally.
     */
    void SendBeacon();

    bool m_beaconEnabled;  //!< Whether this gateway periodically transmits Class B beacons
    Time m_beaconInterval; //!< The interval between consecutive beacon transmissions
    EventId m_nextBeacon;  //!< The next scheduled SendBeacon() call
};

} // namespace lorawan
} // namespace ns3

#endif /* GATEWAY_LORAWAN_MAC_H */
