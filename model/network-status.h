/*
 * Copyright (c) 2018 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Martina Capuzzo <capuzzom@dei.unipd.it>
 *          Davide Magrin <magrinda@dei.unipd.it>
 */

#ifndef NETWORK_STATUS_H
#define NETWORK_STATUS_H

#include "end-device-status.h"
#include "gateway-status.h"

namespace ns3
{
namespace lorawan
{

class ClassBEndDeviceLorawanMac;

/**
 * @ingroup lorawan
 *
 * This class represents the knowledge about the state of the network that is
 * available at the network server. It is essentially a collection of two maps:
 * one containing DeviceStatus objects, and the other containing GatewayStatus
 * objects.
 *
 * This class is meant to be queried by NetworkController components, which
 * can decide to take action based on the current status of the network.
 */
class NetworkStatus : public Object
{
  public:
    /**
     *  Register this type.
     *  @return The object TypeId.
     */
    static TypeId GetTypeId();

    NetworkStatus();           //!< Default constructor
    ~NetworkStatus() override; //!< Destructor

    /**
     * Add a device to the ones that are tracked by this NetworkStatus object.
     *
     * @param edMac Pointer to the MAC layer object of the device to be tracked.
     */
    void AddNode(Ptr<EndDeviceLorawanMac> edMac);

    /**
     * Add a new gateway to the list of gateways connected to the network.
     *
     * Each gateway is identified by its NetDevice Address in the network connecting it to the
     * network server.
     *
     * @param address The gateway's NetDevice Address.
     * @param gwStatus A pointer to a GatewayStatus object for the gateway.
     */
    void AddGateway(Address& address, Ptr<GatewayStatus> gwStatus);

    /**
     * Update network status on a received packet.
     *
     * @param packet The received packet.
     * @param gwaddress Address of the gateway this packet was received from.
     */
    void OnReceivedPacket(Ptr<const Packet> packet, const Address& gwaddress);

    /**
     * Return whether the specified device needs a reply.
     *
     * @param deviceAddress The address of the device we are interested in.
     * @return True if we need to reply to the last packet from the device, false otherwise.
     */
    bool NeedsReply(LoraDeviceAddress deviceAddress);

    /**
     * Return whether we have a gateway that is available to send a reply to the specified
     * device.
     *
     * @param deviceAddress The address of the device we are interested in.
     * @param window The device reception window we are currently targeting (1 or 2; 3 stands
     * for a Class B device's ping slot, listened to at a fixed frequency, see
     * ClassBEndDeviceLorawanMac::BEACON_FREQUENCY_HZ).
     * @return The Address of the gateway which measured the best RSSI of the last packet from the
     * device, selected among the gateways being currently available for downlink transmission.
     */
    Address GetBestGatewayForDevice(LoraDeviceAddress deviceAddress, int window);

    /**
     * Send a packet through a gateway.
     *
     * This function assumes that the packet is already tagged with a LoraTag
     * that will inform the gateway of the parameters to use for the
     * transmission.
     *
     * @param packet The packet.
     * @param gwAddress The address of the gateway.
     */
    void SendThroughGateway(Ptr<Packet> packet, Address gwAddress);

    /**
     * Get the reply packet prepared for a reception window of a device.
     *
     * @param edAddress The address of the device.
     * @param windowNumber The reception window number (1 or 2; 3 stands for a Class B device's
     * ping slot, see GetBestGatewayForDevice()).
     * @return The reply packet.
     */
    Ptr<Packet> GetReplyForDevice(LoraDeviceAddress edAddress, int windowNumber);

    /**
     * Get the EndDeviceStatus of the device that sent a packet.
     *
     * @param packet The packet sent by the end device.
     * @return A pointer to the end device status.
     */
    Ptr<EndDeviceStatus> GetEndDeviceStatus(Ptr<const Packet> packet);

    /**
     * Get the EndDeviceStatus corresponding to a LoraDeviceAddress.
     *
     * @param address The LoraDeviceAddress of the end device.
     * @return A pointer to the end device status.
     */
    Ptr<EndDeviceStatus> GetEndDeviceStatus(LoraDeviceAddress address);

    /**
     * Return the number of end devices currently managed by the server.
     *
     * @return The number of end devices as an int.
     */
    int CountEndDevices();

    /**
     * Queue a downlink to be delivered to a device, independent of any uplink from it.
     *
     * For a Class C device, since its second receive window is continuously open, this attempts
     * immediate delivery, retrying every second until a gateway becomes available. For a Class B
     * device, sending outside its ping slots is pointless (it isn't listening), so this instead
     * computes the device's next ping-slot occurrence and schedules delivery precisely then,
     * retrying at the *next* ping-slot occurrence (not a blind 1-second retry) if no gateway is
     * available at that instant. For a Class A device, the downlink is simply queued and only
     * delivered reactively, at the device's next scheduled receive window opportunity (see
     * NetworkScheduler::OnReceiveWindowOpportunity()), matching real LoRaWAN semantics: a Class A
     * device cannot receive spontaneously.
     *
     * @param edAddress The address of the destination device.
     * @param payload The downlink payload (FRMPayload) to deliver.
     */
    void SendDownlinkToDevice(LoraDeviceAddress edAddress, Ptr<Packet> payload);

  private:
    /**
     * Attempt to deliver the currently queued reply to a Class C device, retrying every second
     * until a gateway becomes available. No-ops if the reply was already delivered (e.g., via
     * the device's own reactive receive window opportunity) by the time a retry fires.
     *
     * @param edAddress The address of the destination device.
     */
    void AttemptClassCDelivery(LoraDeviceAddress edAddress);

    /**
     * Attempt to deliver the currently queued reply to a Class B device at its next ping-slot
     * occurrence, retrying at the ping-slot occurrence after that if no gateway is available at
     * that instant. No-ops if the reply was already delivered (e.g., reactively via the device's
     * own next uplink) by the time this runs. If the device has not (yet, or any longer)
     * synchronized to the network's beacon, no ping-slot occurrence can be computed at all;
     * retries after one BeaconInterval to give the device a chance to (re)acquire the beacon.
     *
     * @param edAddress The address of the destination device.
     */
    void AttemptClassBDelivery(LoraDeviceAddress edAddress);

    /**
     * Compute the earliest ping-slot occurrence, at or after a given instant, for a Class B
     * device -- the same deterministic, publicly-computable formula
     * (ClassBEndDeviceLorawanMac::PingNbSlots()/ComputePingOffset()) the device itself uses,
     * reading only its configured periodicity from the "cheating" m_mac pointer.
     *
     * @param edAddress The address of the device.
     * @param classBMac The device's Class B MAC layer (passed in since callers already have it).
     * @param notBefore The earliest acceptable instant for the returned occurrence.
     * @return The earliest ping-slot occurrence at or after notBefore.
     */
    Time NextPingSlotTime(LoraDeviceAddress edAddress,
                          Ptr<ClassBEndDeviceLorawanMac> classBMac,
                          Time notBefore) const;

  public:
    std::map<LoraDeviceAddress, Ptr<EndDeviceStatus>>
        m_endDeviceStatuses; //!< Map tracking the state of devices connected to this network server
    std::map<Address, Ptr<GatewayStatus>>
        m_gatewayStatuses; //!< Map tracking the state of gateways connected to this network server
};

} // namespace lorawan
} // namespace ns3

#endif /* NETWORK_STATUS_H */
