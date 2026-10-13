/*
 * Copyright (c) 2018 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 */

#include "utilities.h"

// An essential include is test.h
#include "ns3/simulator.h"
#include "ns3/test.h"

// Include headers of classes to test
#include "ns3/class-a-end-device-lorawan-mac.h"
#include "ns3/class-c-end-device-lorawan-mac.h"
#include "ns3/gateway-status.h"
#include "ns3/lorawan-mac-header.h"
#include "ns3/network-scheduler.h"
#include "ns3/network-server.h"
#include "ns3/network-status.h"

using namespace ns3;
using namespace lorawan;

NS_LOG_COMPONENT_DEFINE("NetworkSchedulerTestSuite");

/**
 * @ingroup lorawan
 *
 * It tests the correct functionality of the NetworkScheduler component class of the network server
 */
class NetworkSchedulerTest : public TestCase
{
  public:
    NetworkSchedulerTest();           //!< Default constructor
    ~NetworkSchedulerTest() override; //!< Destructor

  private:
    void DoRun() override;
};

// Add some help text to this case to describe what it is intended to test
NetworkSchedulerTest::NetworkSchedulerTest()
    : TestCase("Verify correct behavior of the NetworkScheduler object")
{
}

// Reminder that the test case should clean up after itself
NetworkSchedulerTest::~NetworkSchedulerTest()
{
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
NetworkSchedulerTest::DoRun()
{
    NS_LOG_DEBUG("NetworkSchedulerTest");

    // If a packet is received at the network server, a reply event should be
    // scheduled to happen 1 second after the reception.
}

/**
 * @ingroup lorawan
 *
 * It verifies that, when no gateway is available at either receive window, a Class A device's
 * reply is abandoned (matching the pre-existing "give up" behavior), while a Class C device's
 * reply is instead retried until a gateway becomes available, since its second receive window
 * never closes.
 */
class ClassCRetryTest : public TestCase
{
  public:
    ClassCRetryTest();           //!< Default constructor
    ~ClassCRetryTest() override; //!< Destructor

    /**
     * Trace the outcome of the MAC layer's confirmed packet retransmission and acknowledgement
     * process, used to determine whether an ack was ever received by the end device.
     *
     * @param txCount Number of transmissions attempted during the process.
     * @param ack Whether the retransmission process led to acknowledgement.
     * @param firstAttempt Timestamp of the initial transmission attempt.
     * @param packet The packet being retransmitted.
     */
    void OnConfirmedTxOutcome(uint8_t txCount, bool ack, Time firstAttempt, Ptr<Packet> packet);

  private:
    void DoRun() override;

    /**
     * Run one busy-gateway-then-free scenario against a freshly built one-ED/one-GW network of
     * the given device type, and return whether the end device ever received an ack.
     *
     * @param deviceType The end device class to build the network with.
     * @return Whether an ack was received by the end device before the simulation ended.
     */
    bool RunScenario(LorawanMacHelper::DeviceType deviceType);

    bool m_ackReceived; //!< Set by OnConfirmedTxOutcome() when ack is true
};

// Add some help text to this case to describe what it is intended to test
ClassCRetryTest::ClassCRetryTest()
    : TestCase("Verify that a Class C device retries (instead of giving up) when no gateway is "
               "available at either receive window")
{
}

// Reminder that the test case should clean up after itself
ClassCRetryTest::~ClassCRetryTest()
{
}

void
ClassCRetryTest::OnConfirmedTxOutcome(uint8_t txCount,
                                      bool ack,
                                      Time firstAttempt,
                                      Ptr<Packet> packet)
{
    if (ack)
    {
        m_ackReceived = true;
    }
}

bool
ClassCRetryTest::RunScenario(LorawanMacHelper::DeviceType deviceType)
{
    NetworkComponents components = InitializeNetwork(1, 1, deviceType);

    Ptr<EndDeviceLorawanMac> edMac =
        GetMacLayerFromNode<EndDeviceLorawanMac>(components.endDevices.Get(0));
    edMac->SetFType(LorawanMacHeader::CONFIRMED_DATA_UP);

    m_ackReceived = false;
    edMac->TraceConnectWithoutContext("ConfirmedTransmissionOutcome",
                                      MakeCallback(&ClassCRetryTest::OnConfirmedTxOutcome, this));

    // Book the sole gateway busy well past both RX1/RX2 opportunities (uplink at t=1s, so RX1
    // and RX2 opportunities land at roughly t=2s and t=3s), then let it free up at t=4.5s.
    Ptr<NetworkServer> app = DynamicCast<NetworkServer>(components.nsNode->GetApplication(0));
    Ptr<GatewayStatus> gwStatus = app->GetNetworkStatus()->m_gatewayStatuses.begin()->second;
    gwStatus->SetNextTransmissionTime(Seconds(4.5));

    Simulator::Schedule(Seconds(1),
                        &NetDevice::Send,
                        components.endDevices.Get(0)->GetDevice(0),
                        Create<Packet>(20),
                        Address(),
                        0);

    Simulator::Stop(Seconds(10));
    Simulator::Run();
    Simulator::Destroy();

    return m_ackReceived;
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
ClassCRetryTest::DoRun()
{
    NS_LOG_DEBUG("ClassCRetryTest");

    NS_TEST_EXPECT_MSG_EQ(RunScenario(LorawanMacHelper::ED_A),
                          false,
                          "A Class A device should give up (no ack) when the gateway stays busy "
                          "through both RX1/RX2, even though it frees up later");
    NS_TEST_EXPECT_MSG_EQ(RunScenario(LorawanMacHelper::ED_C),
                          true,
                          "A Class C device should keep retrying and eventually receive the ack "
                          "once the gateway frees up");
}

/**
 * @ingroup lorawan
 *
 * The TestSuite class names the TestSuite, identifies what type of TestSuite, and enables the
 * TestCases to be run. Typically, only the constructor for this class must be defined
 */
class NetworkSchedulerTestSuite : public TestSuite
{
  public:
    NetworkSchedulerTestSuite(); //!< Default constructor
};

NetworkSchedulerTestSuite::NetworkSchedulerTestSuite()
    : TestSuite("network-scheduler", Type::UNIT)
{
    // LogComponentEnable("NetworkSchedulerTestSuite", LOG_LEVEL_DEBUG);

    AddTestCase(new NetworkSchedulerTest, Duration::QUICK);
    AddTestCase(new ClassCRetryTest, Duration::QUICK);
}

// Do not forget to allocate an instance of this TestSuite
static NetworkSchedulerTestSuite lorawanTestSuite;
