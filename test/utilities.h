/*
 * Copyright (c) 2018 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Davide Magrin <magrinda@dei.unipd.it>
 */

#ifndef TEST_UTILITIES_H
#define TEST_UTILITIES_H

#include "ns3/lora-channel.h"
#include "ns3/lora-net-device.h"
#include "ns3/lorawan-mac-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/node-container.h"

namespace ns3
{
namespace lorawan
{

/**
 * @ingroup lorawan
 *
 * Stores the main elements of a simulated LoRaWAN network
 */
struct NetworkComponents
{
    Ptr<LoraChannel> channel; //!< A pointer to the LoraChannel object.
    NodeContainer endDevices; //!< Container of the end device nodes.
    NodeContainer gateways;   //!< Container of the gateway nodes.
    Ptr<Node> nsNode;         //!< A pointer to the network server Node.
};

Ptr<LoraChannel> CreateChannel();

/**
 * Create a set of end device nodes, install a LoraNetDevice on each, and configure their MAC
 * layer via LorawanMacHelper.
 *
 * @param nDevices Number of end devices to create.
 * @param mobility The mobility helper to install on the devices.
 * @param channel The LoraChannel to attach the devices' PHYs to.
 * @param deviceType The device class to configure (LorawanMacHelper::ED_A, ::ED_C, or ::ED_B).
 * Defaults to ED_A to preserve existing callers.
 * @return The created end device nodes.
 */
NodeContainer CreateEndDevices(int nDevices,
                               MobilityHelper mobility,
                               Ptr<LoraChannel> channel,
                               LorawanMacHelper::DeviceType deviceType = LorawanMacHelper::ED_A);

NodeContainer CreateGateways(int nGateways, MobilityHelper mobility, Ptr<LoraChannel> channel);

Ptr<Node> CreateNetworkServer(NodeContainer endDevices, NodeContainer gateways);

template <typename T>
Ptr<T>
GetMacLayerFromNode(Ptr<Node> n)
{
    return DynamicCast<T>(DynamicCast<LoraNetDevice>(n->GetDevice(0))->GetMac());
}

/**
 * Set up a network with some devices and some gateways, and return the created nodes through a
 * NetworkComponents struct.
 *
 * @param nDevices Number of end devices to create.
 * @param nGateways Number of gateways to create.
 * @param deviceType The device class to configure the end devices with (LorawanMacHelper::ED_A,
 * ::ED_C, or ::ED_B). Defaults to ED_A to preserve existing callers.
 * @return The created network components.
 */
NetworkComponents InitializeNetwork(
    int nDevices,
    int nGateways,
    LorawanMacHelper::DeviceType deviceType = LorawanMacHelper::ED_A);

} // namespace lorawan
} // namespace ns3

#endif /* TEST_UTILITIES_H */
