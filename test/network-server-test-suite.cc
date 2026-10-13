/*
 * Copyright (c) 2018 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 */

/*
 * This file includes testing for the following components:
 * - NetworkServer
 */

#include "utilities.h"

// An essential include is test.h
#include "ns3/application.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

// Include headers of classes to test
#include "ns3/class-a-end-device-lorawan-mac.h"
#include "ns3/class-b-end-device-lorawan-mac.h"
#include "ns3/class-c-end-device-lorawan-mac.h"
#include "ns3/end-device-status.h"
#include "ns3/lora-helper.h"
#include "ns3/lora-phy-helper.h"
#include "ns3/lorawan-mac-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/network-server.h"
#include "ns3/network-status.h"

using namespace ns3;
using namespace lorawan;

NS_LOG_COMPONENT_DEFINE("NetworkServerTestSuite");

/**
 * @ingroup lorawan
 *
 * It verifies that the NetworkServer application can receive packets sent in uplink by devices
 */
class UplinkPacketTest : public TestCase
{
  public:
    UplinkPacketTest();           //!< Default constructor
    ~UplinkPacketTest() override; //!< Destructor

    /**
     * Callback for tracing ReceivedPacket.
     *
     * @param packet The packet received.
     */
    void ReceivedPacket(Ptr<const Packet> packet);

    /**
     * Send a packet from the input end device.
     *
     * @param endDevice A pointer to the end device Node.
     */
    void SendPacket(Ptr<Node> endDevice);

  private:
    void DoRun() override;

    bool m_receivedPacket = false; //!< Set to true if a packet is received by the server
};

// Add some help text to this case to describe what it is intended to test
UplinkPacketTest::UplinkPacketTest()
    : TestCase("Verify that the NetworkServer application can receive"
               " packets sent in the uplink by devices")
{
}

// Reminder that the test case should clean up after itself
UplinkPacketTest::~UplinkPacketTest()
{
}

void
UplinkPacketTest::ReceivedPacket(Ptr<const Packet> packet)
{
    NS_LOG_DEBUG("Received a packet at the network server");
    m_receivedPacket = true;
}

void
UplinkPacketTest::SendPacket(Ptr<Node> endDevice)
{
    endDevice->GetDevice(0)->Send(Create<Packet>(20), Address(), 0);
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
UplinkPacketTest::DoRun()
{
    NS_LOG_DEBUG("UplinkPacketTest");

    // Create a bunch of actual devices
    NetworkComponents components = InitializeNetwork(1, 1);

    Ptr<LoraChannel> channel = components.channel;
    NodeContainer endDevices = components.endDevices;
    NodeContainer gateways = components.gateways;
    Ptr<Node> nsNode = components.nsNode;

    // Connect the trace source for received packets
    nsNode->GetApplication(0)->TraceConnectWithoutContext(
        "ReceivedPacket",
        MakeCallback(&UplinkPacketTest::ReceivedPacket, this));

    // Send a packet
    Simulator::Schedule(Seconds(1), &UplinkPacketTest::SendPacket, this, endDevices.Get(0));

    Simulator::Stop(Seconds(5));
    Simulator::Run();
    Simulator::Destroy();

    // Check that we received the packet
    NS_ASSERT(m_receivedPacket == true);
}

/**
 * @ingroup lorawan
 *
 * It verifies that devices requesting an acknowledgment receive a reply from the network server
 */
class DownlinkPacketTest : public TestCase
{
  public:
    DownlinkPacketTest();           //!< Default constructor
    ~DownlinkPacketTest() override; //!< Destructor

    /**
     * Trace the outcome of MAC layer's confirmed packet retransmission and acknowledgement.
     *
     * This trace sink is only used here to determine whether an ack was received by the end device
     * after sending a package requiring an acknowledgement.
     *
     * @param txCount Number of transmissions attempted during the process.
     * @param ack Whether the retransmission process led to acknowledgement.
     * @param firstAttempt Timestamp of the initial transmission attempt.
     * @param packet The packet being retransmitted.
     */
    void ReceivedPacketAtEndDevice(uint8_t txCount,
                                   bool ack,
                                   Time firstAttempt,
                                   Ptr<Packet> packet);

    /**
     * Send a packet from the input end device.
     *
     * @param endDevice A pointer to the end device Node.
     * @param requestAck Whether to require an acknowledgement from the server.
     */
    void SendPacket(Ptr<Node> endDevice, bool requestAck);

  private:
    void DoRun() override;

    bool m_receivedPacketAtEd = false; //!< Set to true if a packet is received by the end device
};

// Add some help text to this case to describe what it is intended to test
DownlinkPacketTest::DownlinkPacketTest()
    : TestCase("Verify that devices requesting an acknowledgment receive"
               " a reply from the network server.")
{
}

// Reminder that the test case should clean up after itself
DownlinkPacketTest::~DownlinkPacketTest()
{
}

void
DownlinkPacketTest::ReceivedPacketAtEndDevice(uint8_t txCount,
                                              bool ack,
                                              Time firstAttempt,
                                              Ptr<Packet> packet)
{
    NS_LOG_DEBUG("Received a packet at the end device");
    m_receivedPacketAtEd = ack;
}

void
DownlinkPacketTest::SendPacket(Ptr<Node> endDevice, bool requestAck)
{
    if (requestAck)
    {
        DynamicCast<EndDeviceLorawanMac>(
            DynamicCast<LoraNetDevice>(endDevice->GetDevice(0))->GetMac())
            ->SetFType(LorawanMacHeader::CONFIRMED_DATA_UP);
    }
    endDevice->GetDevice(0)->Send(Create<Packet>(20), Address(), 0);
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
DownlinkPacketTest::DoRun()
{
    NS_LOG_DEBUG("DownlinkPacketTest");

    // Create a bunch of actual devices
    NetworkComponents components = InitializeNetwork(1, 1);

    Ptr<LoraChannel> channel = components.channel;
    NodeContainer endDevices = components.endDevices;
    NodeContainer gateways = components.gateways;
    Ptr<Node> nsNode = components.nsNode;

    // Connect the end device's trace source for received packets
    DynamicCast<EndDeviceLorawanMac>(
        DynamicCast<LoraNetDevice>(endDevices.Get(0)->GetDevice(0))->GetMac())
        ->TraceConnectWithoutContext(
            "ConfirmedTransmissionOutcome",
            MakeCallback(&DownlinkPacketTest::ReceivedPacketAtEndDevice, this));

    // Send a packet in uplink
    Simulator::Schedule(Seconds(1), &DownlinkPacketTest::SendPacket, this, endDevices.Get(0), true);

    Simulator::Stop(Seconds(10)); // Allow for time to receive a downlink packet
    Simulator::Run();
    Simulator::Destroy();

    NS_ASSERT(m_receivedPacketAtEd);
}

/**
 * @ingroup lorawan
 *
 * It verifies that the NetworkServer application correctly responds to LinkCheck requests
 */
class LinkCheckTest : public TestCase
{
  public:
    LinkCheckTest();           //!< Default constructor
    ~LinkCheckTest() override; //!< Destructor

    /**
     * Trace changes in the last known gateway count variable (updated on reception of
     * LinkCheckAns MAC commands) of an end device.
     *
     * @param newValue The updated value.
     * @param oldValue The previous value.
     */
    void LastKnownGatewayCount(uint8_t newValue, uint8_t oldValue);

    /**
     * Send a packet containing a LinkCheckReq MAC command from the input end device.
     *
     * @param endDevice A pointer to the end device Node.
     * @param requestAck Whether to require an acknowledgement from the server.
     */
    void SendPacket(Ptr<Node> endDevice, bool requestAck);

  private:
    void DoRun() override;
    bool m_receivedPacketAtEd = false; //!< Set to true if a packet containing a LinkCheckAns MAC
                                       //!< command is received by the end device
    uint8_t m_numberOfGatewaysThatReceivedPacket =
        0; //!< Stores the number of gateways that received the last packet carrying a
           //!< LinkCheckReq MAC command
};

// Add some help text to this case to describe what it is intended to test
LinkCheckTest::LinkCheckTest()
    : TestCase("Verify that the NetworkServer application correctly responds to "
               "LinkCheck requests")
{
}

// Reminder that the test case should clean up after itself
LinkCheckTest::~LinkCheckTest()
{
}

void
LinkCheckTest::LastKnownGatewayCount(uint8_t newValue, uint8_t oldValue)
{
    NS_LOG_DEBUG("Updated gateway count");
    m_receivedPacketAtEd = true;

    m_numberOfGatewaysThatReceivedPacket = newValue;
}

void
LinkCheckTest::SendPacket(Ptr<Node> endDevice, bool requestAck)
{
    Ptr<EndDeviceLorawanMac> macLayer = DynamicCast<EndDeviceLorawanMac>(
        DynamicCast<LoraNetDevice>(endDevice->GetDevice(0))->GetMac());

    if (requestAck)
    {
        macLayer->SetFType(LorawanMacHeader::CONFIRMED_DATA_UP);
    }

    macLayer->AddMacCommand(Create<LinkCheckReq>());

    endDevice->GetDevice(0)->Send(Create<Packet>(20), Address(), 0);
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
LinkCheckTest::DoRun()
{
    NS_LOG_DEBUG("LinkCheckTest");

    // Create a bunch of actual devices
    NetworkComponents components = InitializeNetwork(1, 1);

    Ptr<LoraChannel> channel = components.channel;
    NodeContainer endDevices = components.endDevices;
    NodeContainer gateways = components.gateways;
    Ptr<Node> nsNode = components.nsNode;

    // Connect the end device's trace source for last known gateway count
    DynamicCast<EndDeviceLorawanMac>(
        DynamicCast<LoraNetDevice>(endDevices.Get(0)->GetDevice(0))->GetMac())
        ->TraceConnectWithoutContext("LastKnownGatewayCount",
                                     MakeCallback(&LinkCheckTest::LastKnownGatewayCount, this));

    // Send a packet in uplink
    Simulator::Schedule(Seconds(1), &LinkCheckTest::SendPacket, this, endDevices.Get(0), true);

    Simulator::Stop(Seconds(10)); // Allow for time to receive a downlink packet
    Simulator::Run();
    Simulator::Destroy();

    NS_ASSERT(m_receivedPacketAtEd);
}

/**
 * @ingroup lorawan
 *
 * It verifies that the NetworkServer application correctly responds to PingSlotInfoReq
 * requests from a Class B device with a PingSlotInfoAns.
 */
class PingSlotInfoTest : public TestCase
{
  public:
    PingSlotInfoTest();           //!< Default constructor
    ~PingSlotInfoTest() override; //!< Destructor

    /**
     * Trace callback for the end device's ReceivedPacket trace source, called whenever a
     * downlink packet is correctly received at the MAC layer.
     *
     * @param packet The received packet.
     */
    void ReceivedPacket(Ptr<const Packet> packet);

    /**
     * Send a packet containing a PingSlotInfoReq MAC command from the input end device.
     *
     * @param endDevice A pointer to the end device Node.
     */
    void SendPacket(Ptr<Node> endDevice);

  private:
    void DoRun() override;
    bool m_receivedPingSlotInfoAns =
        false; //!< Set to true if a downlink carrying a PingSlotInfoAns MAC command is
               //!< received by the end device
};

// Add some help text to this case to describe what it is intended to test
PingSlotInfoTest::PingSlotInfoTest()
    : TestCase("Verify that the NetworkServer application correctly responds to "
               "PingSlotInfoReq requests")
{
}

// Reminder that the test case should clean up after itself
PingSlotInfoTest::~PingSlotInfoTest()
{
}

void
PingSlotInfoTest::ReceivedPacket(Ptr<const Packet> packet)
{
    NS_LOG_DEBUG("Received a downlink packet");

    Ptr<Packet> myPacket = packet->Copy();
    LorawanMacHeader mHdr;
    LoraFrameHeader fHdr;
    fHdr.SetAsDownlink();
    myPacket->RemoveHeader(mHdr);
    myPacket->RemoveHeader(fHdr);

    if (fHdr.GetMacCommand<PingSlotInfoAns>())
    {
        m_receivedPingSlotInfoAns = true;
    }
}

void
PingSlotInfoTest::SendPacket(Ptr<Node> endDevice)
{
    Ptr<EndDeviceLorawanMac> macLayer = DynamicCast<EndDeviceLorawanMac>(
        DynamicCast<LoraNetDevice>(endDevice->GetDevice(0))->GetMac());

    macLayer->AddMacCommand(Create<PingSlotInfoReq>(6));

    endDevice->GetDevice(0)->Send(Create<Packet>(20), Address(), 0);
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
PingSlotInfoTest::DoRun()
{
    NS_LOG_DEBUG("PingSlotInfoTest");

    // Build a minimal one-ED (Class B), one-GW network, mirroring ClassCRegistrationTest's
    // setup (CreateEndDevices() only supports ED_A).
    Ptr<LoraChannel> channel = CreateChannel();

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator",
                                  "rho",
                                  DoubleValue(1000),
                                  "X",
                                  DoubleValue(0.0),
                                  "Y",
                                  DoubleValue(0.0));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    NodeContainer endDevices;
    endDevices.Create(1);
    mobility.Install(endDevices);
    LoraPhyHelper edPhyHelper;
    edPhyHelper.SetChannel(channel);
    edPhyHelper.SetDeviceType(LoraPhyHelper::ED);
    LorawanMacHelper edMacHelper;
    edMacHelper.SetDeviceType(LorawanMacHelper::ED_B);
    LoraHelper().Install(edPhyHelper, edMacHelper, endDevices);

    NodeContainer gateways = CreateGateways(1, mobility, channel);

    Ptr<Node> nsNode = CreateNetworkServer(endDevices, gateways);

    // Connect the end device's trace source for received downlink packets
    DynamicCast<EndDeviceLorawanMac>(
        DynamicCast<LoraNetDevice>(endDevices.Get(0)->GetDevice(0))->GetMac())
        ->TraceConnectWithoutContext("ReceivedPacket",
                                     MakeCallback(&PingSlotInfoTest::ReceivedPacket, this));

    // Send a packet in uplink
    Simulator::Schedule(Seconds(1), &PingSlotInfoTest::SendPacket, this, endDevices.Get(0));

    Simulator::Stop(Seconds(10)); // Allow for time to receive a downlink packet at RX1/RX2
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_receivedPingSlotInfoAns,
                          true,
                          "A PingSlotInfoAns should appear in the next reply after a "
                          "PingSlotInfoReq is received");
}

/**
 * @ingroup lorawan
 *
 * It verifies that the NetworkServer application responds to uplinks with the ADRACKReq bit set
 */
class AdrAckReqTest : public TestCase
{
  public:
    AdrAckReqTest();           //!< Default constructor
    ~AdrAckReqTest() override; //!< Destructor

  private:
    void DoRun() override;

    /**
     * Callback for packet reception by the end device MAC layer
     *
     * @param packet The received packet
     */
    void OnReception(Ptr<const Packet> packet);

    bool m_adrAckReceived = false; //!< Set to true if a downlink packet is received by the end
                                   //!< device after setting the uplink ADRACKReq bit
};

AdrAckReqTest::AdrAckReqTest()
    : TestCase("Verify that the NetworkServer responds to uplinks with the ADRACKReq bit set")
{
}

AdrAckReqTest::~AdrAckReqTest()
{
}

void
AdrAckReqTest::OnReception(Ptr<const Packet> packet)
{
    m_adrAckReceived = true;
}

void
AdrAckReqTest::DoRun()
{
    NS_LOG_DEBUG("AdrAckReqTest");
    auto components = InitializeNetwork(1, 1);
    auto ed = components.endDevices.Get(0);
    auto netdev = DynamicCast<LoraNetDevice>(ed->GetDevice(0));
    auto mac = DynamicCast<ClassAEndDeviceLorawanMac>(netdev->GetMac());
    // Turn-off ADR downlinks from the server, ADRACKReq mechanism still works
    mac->SetUplinkAdrBit(false);
    auto cb = MakeCallback(&AdrAckReqTest::OnReception, this);
    mac->TraceConnectWithoutContext("ReceivedPacket", cb);
    // Trigger ADRACKReq bit set
    for (uint16_t fCnt = 0; fCnt <= EndDeviceLorawanMac::ADR_ACK_LIMIT; ++fCnt)
    {
        Simulator::Schedule(Minutes(20),
                            &NetDevice::Send,
                            ed->GetDevice(0),
                            Create<Packet>(20),
                            Address(),
                            0);
        Simulator::Run();
    }
    NS_TEST_EXPECT_MSG_EQ(m_adrAckReceived, true, "No downlink received by the end device");
}

/**
 * @ingroup lorawan
 *
 * It verifies that a Class C device can be registered with a NetworkServer application, tracked
 * in its NetworkStatus, the same way a Class A device can.
 */
class ClassCRegistrationTest : public TestCase
{
  public:
    ClassCRegistrationTest();           //!< Default constructor
    ~ClassCRegistrationTest() override; //!< Destructor

  private:
    void DoRun() override;
};

ClassCRegistrationTest::ClassCRegistrationTest()
    : TestCase("Verify that a Class C device can be registered with a NetworkServer")
{
}

ClassCRegistrationTest::~ClassCRegistrationTest()
{
}

void
ClassCRegistrationTest::DoRun()
{
    NS_LOG_DEBUG("ClassCRegistrationTest");

    // Build a minimal one-ED (Class C), one-GW network, mirroring InitializeNetwork() but with
    // the end device configured as Class C instead of Class A (CreateEndDevices() only supports
    // ED_A).
    Ptr<LoraChannel> channel = CreateChannel();

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator",
                                  "rho",
                                  DoubleValue(1000),
                                  "X",
                                  DoubleValue(0.0),
                                  "Y",
                                  DoubleValue(0.0));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    NodeContainer endDevices;
    endDevices.Create(1);
    mobility.Install(endDevices);
    LoraPhyHelper edPhyHelper;
    edPhyHelper.SetChannel(channel);
    edPhyHelper.SetDeviceType(LoraPhyHelper::ED);
    LorawanMacHelper edMacHelper;
    edMacHelper.SetDeviceType(LorawanMacHelper::ED_C);
    LoraHelper().Install(edPhyHelper, edMacHelper, endDevices);

    NodeContainer gateways = CreateGateways(1, mobility, channel);

    Ptr<Node> nsNode = CreateNetworkServer(endDevices, gateways);

    Ptr<NetworkServer> app = DynamicCast<NetworkServer>(nsNode->GetApplication(0));
    NS_TEST_ASSERT_MSG_NE(app, nullptr, "Failed to find the installed NetworkServer application");

    Ptr<NetworkStatus> status = app->GetNetworkStatus();
    NS_TEST_EXPECT_MSG_EQ(status->CountEndDevices(),
                          1,
                          "Expected exactly one device registered with the NetworkServer");

    Ptr<ClassCEndDeviceLorawanMac> edMac =
        GetMacLayerFromNode<ClassCEndDeviceLorawanMac>(endDevices.Get(0));
    NS_TEST_ASSERT_MSG_NE(edMac, nullptr, "Failed to find the installed Class C MAC layer");

    Ptr<EndDeviceStatus> edStatus = status->GetEndDeviceStatus(edMac->GetDeviceAddress());
    NS_TEST_ASSERT_MSG_NE(edStatus,
                          nullptr,
                          "The Class C device should be tracked in the NetworkServer's "
                          "NetworkStatus");
    NS_TEST_EXPECT_MSG_NE(DynamicCast<ClassCEndDeviceLorawanMac>(edStatus->GetMac()),
                          nullptr,
                          "The tracked EndDeviceStatus should reference the Class C MAC");
}

/**
 * @ingroup lorawan
 *
 * The TestSuite class names the TestSuite, identifies what type of TestSuite, and enables the
 * TestCases to be run. Typically, only the constructor for this class must be defined
 */
class NetworkServerTestSuite : public TestSuite
{
  public:
    NetworkServerTestSuite(); //!< Default constructor
};

NetworkServerTestSuite::NetworkServerTestSuite()
    : TestSuite("network-server", Type::UNIT)
{
    // Activate only at need, as these can create problems among test suites when running ./test.py
    // LogComponentEnable("NetworkServerTestSuite", LOG_LEVEL_DEBUG);
    // LogComponentEnable("NetworkServer", LOG_LEVEL_ALL);
    // LogComponentEnable("NetworkStatus", LOG_LEVEL_ALL);
    // LogComponentEnable("NetworkScheduler", LOG_LEVEL_ALL);
    // LogComponentEnable("NetworkController", LOG_LEVEL_ALL);
    // LogComponentEnable("NetworkControllerComponent", LOG_LEVEL_ALL);
    // LogComponentEnable("LoraNetDevice", LOG_LEVEL_ALL);
    // LogComponentEnable("GatewayLorawanMac", LOG_LEVEL_ALL);
    // LogComponentEnable("EndDeviceLorawanMac", LOG_LEVEL_ALL);
    // LogComponentEnable("EndDeviceLoraPhy", LOG_LEVEL_ALL);
    // LogComponentEnable("EndDeviceStatus", LOG_LEVEL_ALL);
    // LogComponentEnableAll(LOG_PREFIX_FUNC);
    // LogComponentEnableAll(LOG_PREFIX_NODE);
    // LogComponentEnableAll(LOG_PREFIX_TIME);

    AddTestCase(new UplinkPacketTest, Duration::QUICK);
    AddTestCase(new DownlinkPacketTest, Duration::QUICK);
    AddTestCase(new LinkCheckTest, Duration::QUICK);
    AddTestCase(new PingSlotInfoTest, Duration::QUICK);
    AddTestCase(new AdrAckReqTest, Duration::QUICK);
    AddTestCase(new ClassCRegistrationTest, Duration::QUICK);
}

// Do not forget to allocate an instance of this TestSuite
static NetworkServerTestSuite lorawanTestSuite;
