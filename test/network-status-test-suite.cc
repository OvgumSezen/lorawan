/*
 * Copyright (c) 2018 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 */

/*
 * This file includes testing for the following components:
 * - EndDeviceStatus
 * - GatewayStatus
 * - NetworkStatus
 */

#include "utilities.h"

// An essential include is test.h
#include "ns3/simulator.h"
#include "ns3/test.h"

// Include headers of classes to test
#include "ns3/class-a-end-device-lorawan-mac.h"
#include "ns3/class-b-end-device-lorawan-mac.h"
#include "ns3/class-c-end-device-lorawan-mac.h"
#include "ns3/end-device-lorawan-mac.h"
#include "ns3/lora-frame-header.h"
#include "ns3/lorawan-mac-header.h"
#include "ns3/mac-command.h"
#include "ns3/network-server.h"
#include "ns3/network-status.h"

using namespace ns3;
using namespace lorawan;

NS_LOG_COMPONENT_DEFINE("NetworkStatusTestSuite");

/**
 * @ingroup lorawan
 *
 * It tests the constructor of the EndDeviceStatus class
 */
class EndDeviceStatusTest : public TestCase
{
  public:
    EndDeviceStatusTest();           //!< Default constructor
    ~EndDeviceStatusTest() override; //!< Destructor

  private:
    void DoRun() override;
};

// Add some help text to this case to describe what it is intended to test
EndDeviceStatusTest::EndDeviceStatusTest()
    : TestCase("Verify correct behavior of the EndDeviceStatus object")
{
}

// Reminder that the test case should clean up after itself
EndDeviceStatusTest::~EndDeviceStatusTest()
{
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
EndDeviceStatusTest::DoRun()
{
    NS_LOG_DEBUG("EndDeviceStatusTest");

    // Create an EndDeviceStatus object
    EndDeviceStatus eds = EndDeviceStatus();
}

/**
 * @ingroup lorawan
 *
 * It tests EndDeviceStatus::IsClassC() correctly distinguishes Class A from Class C devices.
 */
class IsClassCTest : public TestCase
{
  public:
    IsClassCTest();           //!< Default constructor
    ~IsClassCTest() override; //!< Destructor

  private:
    void DoRun() override;
};

// Add some help text to this case to describe what it is intended to test
IsClassCTest::IsClassCTest()
    : TestCase("Verify that EndDeviceStatus::IsClassC() correctly reports the device class")
{
}

// Reminder that the test case should clean up after itself
IsClassCTest::~IsClassCTest()
{
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
IsClassCTest::DoRun()
{
    NS_LOG_DEBUG("IsClassCTest");

    Ptr<ClassAEndDeviceLorawanMac> classAMac = CreateObject<ClassAEndDeviceLorawanMac>();
    EndDeviceStatus classAStatus(LoraDeviceAddress(0), classAMac);
    NS_TEST_EXPECT_MSG_EQ(classAStatus.IsClassC(),
                          false,
                          "A Class A device's EndDeviceStatus should not report IsClassC()");

    Ptr<ClassCEndDeviceLorawanMac> classCMac = CreateObject<ClassCEndDeviceLorawanMac>();
    EndDeviceStatus classCStatus(LoraDeviceAddress(0), classCMac);
    NS_TEST_EXPECT_MSG_EQ(classCStatus.IsClassC(),
                          true,
                          "A Class C device's EndDeviceStatus should report IsClassC()");
}

/**
 * @ingroup lorawan
 *
 * It tests EndDeviceStatus::IsClassB() correctly distinguishes Class B devices from Class A and
 * Class C devices, and that IsClassC()/IsClassB() never both report true for the same device.
 */
class IsClassBTest : public TestCase
{
  public:
    IsClassBTest();           //!< Default constructor
    ~IsClassBTest() override; //!< Destructor

  private:
    void DoRun() override;
};

// Add some help text to this case to describe what it is intended to test
IsClassBTest::IsClassBTest()
    : TestCase("Verify that EndDeviceStatus::IsClassB() correctly reports the device class")
{
}

// Reminder that the test case should clean up after itself
IsClassBTest::~IsClassBTest()
{
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
IsClassBTest::DoRun()
{
    NS_LOG_DEBUG("IsClassBTest");

    Ptr<ClassAEndDeviceLorawanMac> classAMac = CreateObject<ClassAEndDeviceLorawanMac>();
    EndDeviceStatus classAStatus(LoraDeviceAddress(0), classAMac);
    NS_TEST_EXPECT_MSG_EQ(classAStatus.IsClassB(),
                          false,
                          "A Class A device's EndDeviceStatus should not report IsClassB()");

    Ptr<ClassCEndDeviceLorawanMac> classCMac = CreateObject<ClassCEndDeviceLorawanMac>();
    EndDeviceStatus classCStatus(LoraDeviceAddress(0), classCMac);
    NS_TEST_EXPECT_MSG_EQ(classCStatus.IsClassB(),
                          false,
                          "A Class C device's EndDeviceStatus should not report IsClassB()");

    Ptr<ClassBEndDeviceLorawanMac> classBMac = CreateObject<ClassBEndDeviceLorawanMac>();
    EndDeviceStatus classBStatus(LoraDeviceAddress(0), classBMac);
    NS_TEST_EXPECT_MSG_EQ(classBStatus.IsClassB(),
                          true,
                          "A Class B device's EndDeviceStatus should report IsClassB()");
    NS_TEST_EXPECT_MSG_EQ(classBStatus.IsClassC(),
                          false,
                          "A Class B device's EndDeviceStatus should not report IsClassC()");

    Simulator::Destroy();
}

/**
 * @ingroup lorawan
 *
 * It tests the function NetworkStatus::AddNode
 */
class NetworkStatusTest : public TestCase
{
  public:
    NetworkStatusTest();           //!< Default constructor
    ~NetworkStatusTest() override; //!< Destructor

  private:
    void DoRun() override;
};

// Add some help text to this case to describe what it is intended to test
NetworkStatusTest::NetworkStatusTest()
    : TestCase("Verify correct behavior of the NetworkStatus object")
{
}

// Reminder that the test case should clean up after itself
NetworkStatusTest::~NetworkStatusTest()
{
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
NetworkStatusTest::DoRun()
{
    NS_LOG_DEBUG("NetworkStatusTest");

    // Create a NetworkStatus object
    NetworkStatus ns = NetworkStatus();

    // Create a bunch of actual devices
    NetworkComponents components = InitializeNetwork(1, 1);

    Ptr<LoraChannel> channel = components.channel;
    NodeContainer endDevices = components.endDevices;
    NodeContainer gateways = components.gateways;

    ns.AddNode(GetMacLayerFromNode<ClassAEndDeviceLorawanMac>(endDevices.Get(0)));
}

/**
 * @ingroup lorawan
 *
 * It tests NetworkStatus::SendDownlinkToDevice(): a Class C device receives a spontaneously
 * queued downlink without needing to send an uplink of its own, unlike a Class A device, which
 * only receives it reactively (not exercised further here -- see the "no uplink is ever sent in
 * this scenario" comment below -- since Class A's reactive delivery is already covered by
 * NetworkServerTestSuite's DownlinkPacketTest). A Class B device also receives it after only a
 * bootstrap uplink (needed so the server learns which gateway(s) can reach it at all -- the same
 * reason Class C's own scenario bootstraps below; unrelated to Class B's own beacon/ping-slot
 * schedule, which needs no uplink to get going), but -- unlike Class C's immediate delivery --
 * only once its next ping-slot occurrence comes around; that occurrence is independently
 * recomputed here (from the same public PingNbSlots()/ComputePingOffset() formula
 * AttemptClassBDelivery() itself uses) and checked against the time delivery actually happens.
 */
class SendDownlinkToDeviceTest : public TestCase
{
  public:
    SendDownlinkToDeviceTest();           //!< Default constructor
    ~SendDownlinkToDeviceTest() override; //!< Destructor

    /**
     * Callback registered as the end device MAC's ReceiveCallback.
     *
     * @param packet The received packet.
     */
    void OnReceived(Ptr<Packet> packet);

    /**
     * Independently compute the earliest ping-slot occurrence at or after a given instant, for
     * a Class B device at a given periodicity -- mirroring, but not calling,
     * NetworkStatus::NextPingSlotTime()'s algorithm, using only the same public building blocks
     * a real network server would have access to.
     *
     * @param edAddress The device's address.
     * @param periodicity The device's ping slot periodicity.
     * @param notBefore The earliest acceptable instant for the returned occurrence.
     * @return The earliest ping-slot occurrence at or after notBefore.
     */
    static Time ExpectedNextPingSlot(LoraDeviceAddress edAddress,
                                     uint8_t periodicity,
                                     Time notBefore);

  private:
    void DoRun() override;

    bool m_received;     //!< Set by OnReceived() when a downlink reaches the end device
    Time m_receivedTime; //!< Simulation time at which OnReceived() last fired
};

// Add some help text to this case to describe what it is intended to test
SendDownlinkToDeviceTest::SendDownlinkToDeviceTest()
    : TestCase("Verify that NetworkStatus::SendDownlinkToDevice() delivers correctly across "
               "device classes (A/B/C)")
{
}

// Reminder that the test case should clean up after itself
SendDownlinkToDeviceTest::~SendDownlinkToDeviceTest()
{
}

void
SendDownlinkToDeviceTest::OnReceived(Ptr<Packet> packet)
{
    m_received = true;
    m_receivedTime = Simulator::Now();
}

Time
SendDownlinkToDeviceTest::ExpectedNextPingSlot(LoraDeviceAddress edAddress,
                                               uint8_t periodicity,
                                               Time notBefore)
{
    uint16_t pingNbSlots = ClassBEndDeviceLorawanMac::PingNbSlots(periodicity);
    Time beaconInterval = ClassBEndDeviceLorawanMac::BEACON_INTERVAL;
    Time pingPeriod = beaconInterval / pingNbSlots;

    int64_t periodIndex = notBefore.GetNanoSeconds() / beaconInterval.GetNanoSeconds();
    Time periodStart = beaconInterval * periodIndex;

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
            return firstSlot + pingPeriod * k;
        }

        periodStart += beaconInterval;
    }
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
SendDownlinkToDeviceTest::DoRun()
{
    NS_LOG_DEBUG("SendDownlinkToDeviceTest");

    // Scenario A: a Class A device does not receive a spontaneously-queued downlink -- no uplink
    // is ever sent in this scenario, so NetworkScheduler never gets a chance to deliver it.
    {
        NetworkComponents components = InitializeNetwork(1, 1); // ED_A
        auto edMac = GetMacLayerFromNode<EndDeviceLorawanMac>(components.endDevices.Get(0));
        m_received = false;
        edMac->SetReceiveCallback(MakeCallback(&SendDownlinkToDeviceTest::OnReceived, this));

        Ptr<NetworkServer> app = DynamicCast<NetworkServer>(components.nsNode->GetApplication(0));
        Ptr<NetworkStatus> status = app->GetNetworkStatus();

        Simulator::Schedule(Seconds(1),
                            &NetworkStatus::SendDownlinkToDevice,
                            status,
                            edMac->GetDeviceAddress(),
                            Create<Packet>(5));

        Simulator::Stop(Seconds(3));
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_EXPECT_MSG_EQ(m_received,
                              false,
                              "A Class A device should not receive a spontaneously-queued "
                              "downlink without first sending an uplink of its own");
    }

    // Scenario B: a Class C device receives a spontaneously-queued downlink without any further
    // uplink, once it is continuously listening. A freshly-installed device only starts
    // continuously listening once it completes its own first RX1/RX2 ack sequence (no Join procedure),
    // so an initial "bootstrap" uplink is sent first; no second uplink is sent afterward.
    {
        NetworkComponents components = InitializeNetwork(1, 1, LorawanMacHelper::ED_C);
        auto edMac = GetMacLayerFromNode<EndDeviceLorawanMac>(components.endDevices.Get(0));
        m_received = false;
        edMac->SetReceiveCallback(MakeCallback(&SendDownlinkToDeviceTest::OnReceived, this));

        Ptr<NetworkServer> app = DynamicCast<NetworkServer>(components.nsNode->GetApplication(0));
        Ptr<NetworkStatus> status = app->GetNetworkStatus();

        // Bootstrap uplink (unconfirmed, no reply expected from the server)
        Simulator::Schedule(Seconds(1),
                            &NetDevice::Send,
                            components.endDevices.Get(0)->GetDevice(0),
                            Create<Packet>(20),
                            Address(),
                            0);

        // Pushed well after the bootstrap cycle has concluded; no further uplink is sent
        Simulator::Schedule(Seconds(5),
                            &NetworkStatus::SendDownlinkToDevice,
                            status,
                            edMac->GetDeviceAddress(),
                            Create<Packet>(5));

        Simulator::Stop(Seconds(10));
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_EXPECT_MSG_EQ(m_received,
                              true,
                              "A continuously-listening Class C device should receive a "
                              "spontaneously-queued downlink without any further uplink");
    }

    // Scenario C: a Class B device receives a spontaneously-queued downlink at its next ping
    // slot, without any *further* uplink. Beacon acquisition itself starts unconditionally at
    // device construction and needs no uplink -- but GetBestGatewayForDevice()
    // still needs to already know which gateway(s) can reach this device at all, which it only
    // learns from a prior uplink (the same reason Scenario B bootstraps a Class C device above);
    // this precondition is orthogonal to, and unrelated to, Class B's beacon/ping-slot schedule.
    {
        NetworkComponents components = InitializeNetwork(1, 1, LorawanMacHelper::ED_B);
        LorawanMacHelper macHelper;
        macHelper.EnableBeaconing(components.gateways);

        auto edMac = GetMacLayerFromNode<ClassBEndDeviceLorawanMac>(components.endDevices.Get(0));
        m_received = false;
        edMac->SetReceiveCallback(MakeCallback(&SendDownlinkToDeviceTest::OnReceived, this));

        Ptr<NetworkServer> app = DynamicCast<NetworkServer>(components.nsNode->GetApplication(0));
        Ptr<NetworkStatus> status = app->GetNetworkStatus();

        // Bootstrap uplink (unconfirmed, no reply expected from the server)
        Simulator::Schedule(Seconds(1),
                            &NetDevice::Send,
                            components.endDevices.Get(0)->GetDevice(0),
                            Create<Packet>(20),
                            Address(),
                            0);

        // Pushed well after the device should have acquired the beacon and become synchronized
        // (same BEACON_INTERVAL-based margin BeaconTransmissionTest itself relies on).
        Time pushTime = ClassBEndDeviceLorawanMac::BEACON_INTERVAL + Seconds(1);
        Simulator::Schedule(pushTime,
                            &NetworkStatus::SendDownlinkToDevice,
                            status,
                            edMac->GetDeviceAddress(),
                            Create<Packet>(5));

        Simulator::Stop(2 * ClassBEndDeviceLorawanMac::BEACON_INTERVAL);
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_EXPECT_MSG_EQ(m_received,
                              true,
                              "A Class B device should receive a spontaneously-queued downlink "
                              "at its next ping slot, without any uplink of its own");

        Time expectedSlot = ExpectedNextPingSlot(edMac->GetDeviceAddress(),
                                                 edMac->GetPingSlotPeriodicity(),
                                                 pushTime);
        NS_TEST_EXPECT_MSG_GT_OR_EQ(m_receivedTime,
                                    expectedSlot,
                                    "Delivery should not happen before the device's computed "
                                    "next ping slot (i.e., not immediately upon pushing)");
        NS_TEST_EXPECT_MSG_LT(m_receivedTime,
                              expectedSlot + Seconds(1),
                              "Delivery should land inside the targeted ping slot, not at some "
                              "later, arbitrary retry cadence");
    }
}

/**
 * @ingroup lorawan
 *
 * Regression test for a latent crash in EndDeviceStatus::GetPowerGatewayMap(): it used to call
 * m_receivedPacketList.back() unconditionally, which is undefined behavior (observed as a
 * SIGSEGV) when this device has never sent an uplink and the list is therefore empty. This is
 * reachable from NetworkStatus::GetBestGatewayForDevice(), itself called by
 * AttemptClassCDelivery()/AttemptClassBDelivery() whenever SendDownlinkToDevice() targets a Class
 * B/C device before it has ever sent an uplink (Class A never calls GetBestGatewayForDevice() from
 * this path, so it is unaffected). Verifies that, with the empty-list guard in place, pushing a
 * downlink to such a device does not crash: delivery simply never happens (retried forever, since
 * no gateway is known to be able to reach the device) instead of corrupting memory.
 */
class NoUplinkGetBestGatewayForDeviceTest : public TestCase
{
  public:
    NoUplinkGetBestGatewayForDeviceTest();           //!< Default constructor
    ~NoUplinkGetBestGatewayForDeviceTest() override; //!< Destructor

    /**
     * Callback registered as the end device MAC's ReceiveCallback.
     *
     * @param packet The received packet.
     */
    void OnReceived(Ptr<Packet> packet);

  private:
    void DoRun() override;

    bool m_received; //!< Set by OnReceived() when a downlink reaches the end device
};

// Add some help text to this case to describe what it is intended to test
NoUplinkGetBestGatewayForDeviceTest::NoUplinkGetBestGatewayForDeviceTest()
    : TestCase("Verify that pushing a downlink to a Class B or Class C device that has never "
               "sent an uplink does not crash")
{
}

// Reminder that the test case should clean up after itself
NoUplinkGetBestGatewayForDeviceTest::~NoUplinkGetBestGatewayForDeviceTest()
{
}

void
NoUplinkGetBestGatewayForDeviceTest::OnReceived(Ptr<Packet> packet)
{
    m_received = true;
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
NoUplinkGetBestGatewayForDeviceTest::DoRun()
{
    NS_LOG_DEBUG("NoUplinkGetBestGatewayForDeviceTest");

    // Scenario A: a Class C device that has never sent an uplink. AttemptClassCDelivery() calls
    // GetBestGatewayForDevice() right away, hitting the empty-list case on the very first attempt.
    {
        NetworkComponents components = InitializeNetwork(1, 1, LorawanMacHelper::ED_C);
        auto edMac = GetMacLayerFromNode<EndDeviceLorawanMac>(components.endDevices.Get(0));
        m_received = false;
        edMac->SetReceiveCallback(
            MakeCallback(&NoUplinkGetBestGatewayForDeviceTest::OnReceived, this));

        Ptr<NetworkServer> app = DynamicCast<NetworkServer>(components.nsNode->GetApplication(0));
        Ptr<NetworkStatus> status = app->GetNetworkStatus();

        // No uplink is ever sent in this scenario: the server has no gateway on record for this
        // device at all.
        Simulator::Schedule(Seconds(1),
                            &NetworkStatus::SendDownlinkToDevice,
                            status,
                            edMac->GetDeviceAddress(),
                            Create<Packet>(5));

        Simulator::Stop(Seconds(5));
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_EXPECT_MSG_EQ(m_received,
                              false,
                              "A Class C device that has never sent an uplink has no known "
                              "gateway, so it should never receive the pushed downlink, but "
                              "attempting delivery must not crash the simulation");
    }

    // Scenario B: a Class B device that has never sent an uplink. Beacon acquisition needs no
    // uplink, so the device still reaches its ping slot and AttemptClassBDelivery() still calls
    // GetBestGatewayForDevice(), hitting the same empty-list case.
    {
        NetworkComponents components = InitializeNetwork(1, 1, LorawanMacHelper::ED_B);
        LorawanMacHelper macHelper;
        macHelper.EnableBeaconing(components.gateways);

        auto edMac = GetMacLayerFromNode<ClassBEndDeviceLorawanMac>(components.endDevices.Get(0));
        m_received = false;
        edMac->SetReceiveCallback(
            MakeCallback(&NoUplinkGetBestGatewayForDeviceTest::OnReceived, this));

        Ptr<NetworkServer> app = DynamicCast<NetworkServer>(components.nsNode->GetApplication(0));
        Ptr<NetworkStatus> status = app->GetNetworkStatus();

        // No uplink is ever sent in this scenario. Pushed once the device should already be
        // synchronized to the beacon (same margin used elsewhere in this file).
        Simulator::Schedule(ClassBEndDeviceLorawanMac::BEACON_INTERVAL + Seconds(1),
                            &NetworkStatus::SendDownlinkToDevice,
                            status,
                            edMac->GetDeviceAddress(),
                            Create<Packet>(5));

        Simulator::Stop(2 * ClassBEndDeviceLorawanMac::BEACON_INTERVAL);
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_EXPECT_MSG_EQ(m_received,
                              false,
                              "A Class B device that has never sent an uplink has no known "
                              "gateway, so it should never receive the pushed downlink, but "
                              "attempting delivery at its ping slot must not crash the "
                              "simulation");
    }
}

/**
 * @ingroup lorawan
 *
 * Regression test for a latent crash in NetworkStatus::GetEndDeviceStatus(Ptr<const Packet>): the
 * LoraFrameHeader it deserializes was never marked uplink via SetAsUplink(), so Deserialize() read
 * an indeterminate m_isUplink and could pick the downlink CID table for what is actually an uplink
 * packet. Reproduced deterministically with a DlChannelAns MAC command (CID 0x0A), which exists
 * only in the uplink CID table -- under the (wrong) downlink table it used to fall into
 * Deserialize()'s unhandled-CID case, leaving a null Ptr<MacCommand> that got dereferenced right
 * after (observed upstream as a SIGABRT under heavy Class B ping-slot MAC command traffic, where
 * PingSlotInfoReq is re-sent on every uplink until acknowledged).
 */
class GetEndDeviceStatusUplinkFrameHeaderTest : public TestCase
{
  public:
    GetEndDeviceStatusUplinkFrameHeaderTest();           //!< Default constructor
    ~GetEndDeviceStatusUplinkFrameHeaderTest() override; //!< Destructor

  private:
    void DoRun() override;
};

// Add some help text to this case to describe what it is intended to test
GetEndDeviceStatusUplinkFrameHeaderTest::GetEndDeviceStatusUplinkFrameHeaderTest()
    : TestCase("Verify that NetworkStatus::GetEndDeviceStatus(Ptr<const Packet>) does not crash "
               "parsing an uplink frame header that carries an uplink-only MAC command")
{
}

// Reminder that the test case should clean up after itself
GetEndDeviceStatusUplinkFrameHeaderTest::~GetEndDeviceStatusUplinkFrameHeaderTest()
{
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
GetEndDeviceStatusUplinkFrameHeaderTest::DoRun()
{
    NS_LOG_DEBUG("GetEndDeviceStatusUplinkFrameHeaderTest");

    NetworkStatus ns = NetworkStatus();
    NetworkComponents components = InitializeNetwork(1, 1);
    auto edMac = GetMacLayerFromNode<ClassAEndDeviceLorawanMac>(components.endDevices.Get(0));
    ns.AddNode(edMac);

    // Hand-build a real uplink packet whose FOpts carries a DlChannelAns (CID 0x0A). That CID is
    // only defined in the uplink table, so if the frame header being parsed is ever mistaken for
    // downlink again, it desyncs to a bogus CID.
    LorawanMacHeader mHdr;
    mHdr.SetFType(LorawanMacHeader::UNCONFIRMED_DATA_UP);
    mHdr.SetMajor(1);

    LoraFrameHeader fHdr;
    fHdr.SetAsUplink();
    fHdr.SetAddress(edMac->GetDeviceAddress());
    fHdr.SetFCnt(0);
    fHdr.AddCommand(Create<DlChannelAns>());

    Ptr<Packet> packet = Create<Packet>(10);
    packet->AddHeader(fHdr);
    packet->AddHeader(mHdr);

    Ptr<EndDeviceStatus> status = ns.GetEndDeviceStatus(packet);

    NS_TEST_ASSERT_MSG_NE(status,
                          nullptr,
                          "GetEndDeviceStatus should find the registered device instead of "
                          "crashing while parsing its uplink frame header");
}

/**
 * @ingroup lorawan
 *
 * The TestSuite class names the TestSuite, identifies what type of TestSuite, and enables the
 * TestCases to be run. Typically, only the constructor for this class must be defined
 */
class NetworkStatusTestSuite : public TestSuite
{
  public:
    NetworkStatusTestSuite(); //!< Default constructor
};

NetworkStatusTestSuite::NetworkStatusTestSuite()
    : TestSuite("network-status", Type::UNIT)
{
    // LogComponentEnable("NetworkStatusTestSuite", LOG_LEVEL_DEBUG);

    AddTestCase(new EndDeviceStatusTest, Duration::QUICK);
    AddTestCase(new IsClassCTest, Duration::QUICK);
    AddTestCase(new IsClassBTest, Duration::QUICK);
    AddTestCase(new NetworkStatusTest, Duration::QUICK);
    AddTestCase(new SendDownlinkToDeviceTest, Duration::QUICK);
    AddTestCase(new NoUplinkGetBestGatewayForDeviceTest, Duration::QUICK);
    AddTestCase(new GetEndDeviceStatusUplinkFrameHeaderTest, Duration::QUICK);
}

// Do not forget to allocate an instance of this TestSuite
static NetworkStatusTestSuite lorawanTestSuite;
