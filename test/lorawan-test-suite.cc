/*
 * Copyright (c) 2017 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 */

// An essential include is test.h
#include "ns3/constant-position-mobility-model.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

// Include headers of classes to test
#include "ns3/integer.h"
#include "ns3/lorawan-module.h"

// Shared test scaffolding (CreateChannel, CreateGateways, GetMacLayerFromNode, ...)
#include "utilities.h"

#include <algorithm>
#include <cmath>

using namespace ns3;
using namespace lorawan;

NS_LOG_COMPONENT_DEFINE("LorawanTestSuite");

/**
 * @ingroup lorawan
 *
 * It tests interference computations in a number of possible scenarios using the
 * LoraInterferenceHelper class
 */
class InterferenceTest : public TestCase
{
  public:
    InterferenceTest();           //!< Default constructor
    ~InterferenceTest() override; //!< Destructor

  private:
    void DoRun() override;
};

// Add some help text to this case to describe what it is intended to test
InterferenceTest::InterferenceTest()
    : TestCase("Verify that LoraInterferenceHelper works as expected")
{
}

// Reminder that the test case should clean up after itself
InterferenceTest::~InterferenceTest()
{
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
InterferenceTest::DoRun()
{
    NS_LOG_DEBUG("InterferenceTest");

    LoraInterferenceHelper interferenceHelper;

    uint32_t frequencyHz = 868100000;
    uint32_t differentFrequencyHz = 868300000;

    Ptr<LoraInterferenceHelper::Event> event;
    Ptr<LoraInterferenceHelper::Event> event1;

    // Test overlap duration
    event = interferenceHelper.Add(Seconds(2), 14, 7, nullptr, frequencyHz);
    event1 = interferenceHelper.Add(Seconds(1), 14, 12, nullptr, frequencyHz);
    NS_TEST_EXPECT_MSG_EQ(interferenceHelper.GetOverlapTime(event, event1),
                          Seconds(1),
                          "Overlap computation didn't give the expected result");
    interferenceHelper.ClearAllEvents();

    event = interferenceHelper.Add(Seconds(2), 14, 7, nullptr, frequencyHz);
    event1 = interferenceHelper.Add(Seconds(1.5), 14, 12, nullptr, frequencyHz);
    NS_TEST_EXPECT_MSG_EQ(interferenceHelper.GetOverlapTime(event, event1),
                          Seconds(1.5),
                          "Overlap computation didn't give the expected result");
    interferenceHelper.ClearAllEvents();

    event = interferenceHelper.Add(Seconds(2), 14, 7, nullptr, frequencyHz);
    event1 = interferenceHelper.Add(Seconds(3), 14, 12, nullptr, frequencyHz);
    NS_TEST_EXPECT_MSG_EQ(interferenceHelper.GetOverlapTime(event, event1),
                          Seconds(2),
                          "Overlap computation didn't give the expected result");
    interferenceHelper.ClearAllEvents();

    event = interferenceHelper.Add(Seconds(2), 14, 7, nullptr, frequencyHz);
    event1 = interferenceHelper.Add(Seconds(2), 14, 12, nullptr, frequencyHz);
    // Because of some strange behavior, this test would get stuck if we used the same syntax of the
    // previous ones. This works instead.
    bool retval = interferenceHelper.GetOverlapTime(event, event1) == Seconds(2);
    NS_TEST_EXPECT_MSG_EQ(retval, true, "Overlap computation didn't give the expected result");
    interferenceHelper.ClearAllEvents();

    // Perfect overlap, packet survives
    event = interferenceHelper.Add(Seconds(2), 14, 7, nullptr, frequencyHz);
    interferenceHelper.Add(Seconds(2), 14, 12, nullptr, frequencyHz);
    NS_TEST_EXPECT_MSG_EQ(interferenceHelper.IsDestroyedByInterference(event),
                          0,
                          "Packet did not survive interference as expected");
    interferenceHelper.ClearAllEvents();

    // Perfect overlap, packet survives
    event = interferenceHelper.Add(Seconds(2), 14, 7, nullptr, frequencyHz);
    interferenceHelper.Add(Seconds(2), 14 - 7, 7, nullptr, frequencyHz);
    NS_TEST_EXPECT_MSG_EQ(interferenceHelper.IsDestroyedByInterference(event),
                          0,
                          "Packet did not survive interference as expected");
    interferenceHelper.ClearAllEvents();

    // Perfect overlap, packet destroyed
    event = interferenceHelper.Add(Seconds(2), 14, 7, nullptr, frequencyHz);
    interferenceHelper.Add(Seconds(2), 14 - 6, 7, nullptr, frequencyHz);
    NS_TEST_EXPECT_MSG_EQ(interferenceHelper.IsDestroyedByInterference(event),
                          7,
                          "Packet was not destroyed by interference as expected");
    interferenceHelper.ClearAllEvents();

    // Partial overlap, packet survives
    event = interferenceHelper.Add(Seconds(2), 14, 7, nullptr, frequencyHz);
    interferenceHelper.Add(Seconds(1), 14 - 6, 7, nullptr, frequencyHz);
    NS_TEST_EXPECT_MSG_EQ(interferenceHelper.IsDestroyedByInterference(event),
                          0,
                          "Packet did not survive interference as expected");
    interferenceHelper.ClearAllEvents();

    // Different frequencys
    // Packet would be destroyed if they were on the same frequency, but survives
    // since they are on different frequencies
    event = interferenceHelper.Add(Seconds(2), 14, 7, nullptr, frequencyHz);
    interferenceHelper.Add(Seconds(2), 14, 7, nullptr, differentFrequencyHz);
    NS_TEST_EXPECT_MSG_EQ(interferenceHelper.IsDestroyedByInterference(event),
                          0,
                          "Packet did not survive interference as expected");
    interferenceHelper.ClearAllEvents();

    // Different SFs
    // Packet would be destroyed if they both were SF7, but survives thanks to spreading factor
    // semi-orthogonality
    event = interferenceHelper.Add(Seconds(2), 14, 7, nullptr, frequencyHz);
    interferenceHelper.Add(Seconds(2), 14 + 16, 8, nullptr, frequencyHz);
    NS_TEST_EXPECT_MSG_EQ(interferenceHelper.IsDestroyedByInterference(event),
                          0,
                          "Packet did not survive interference as expected");
    interferenceHelper.ClearAllEvents();

    // Spreading factor imperfect orthogonality
    // Different SFs are orthogonal only up to a point
    event = interferenceHelper.Add(Seconds(2), 14, 7, nullptr, frequencyHz);
    interferenceHelper.Add(Seconds(2), 14 + 17, 8, nullptr, frequencyHz);
    NS_TEST_EXPECT_MSG_EQ(interferenceHelper.IsDestroyedByInterference(event),
                          8,
                          "Packet was not destroyed by interference as expected");
    interferenceHelper.ClearAllEvents();

    // If a more 'distant' spreading factor is used, isolation gets better
    event = interferenceHelper.Add(Seconds(2), 14, 7, nullptr, frequencyHz);
    interferenceHelper.Add(Seconds(2), 14 + 17, 10, nullptr, frequencyHz);
    NS_TEST_EXPECT_MSG_EQ(interferenceHelper.IsDestroyedByInterference(event),
                          0,
                          "Packet was destroyed by interference while it should have survived");
    interferenceHelper.ClearAllEvents();

    // Cumulative interference
    // Same spreading factor interference is cumulative
    event = interferenceHelper.Add(Seconds(2), 14, 7, nullptr, frequencyHz);
    interferenceHelper.Add(Seconds(2), 14 + 16, 8, nullptr, frequencyHz);
    interferenceHelper.Add(Seconds(2), 14 + 16, 8, nullptr, frequencyHz);
    interferenceHelper.Add(Seconds(2), 14 + 16, 8, nullptr, frequencyHz);
    NS_TEST_EXPECT_MSG_EQ(interferenceHelper.IsDestroyedByInterference(event),
                          8,
                          "Packet was not destroyed by interference as expected");
    interferenceHelper.ClearAllEvents();

    // Cumulative interference
    // Interference is not cumulative between different SFs
    event = interferenceHelper.Add(Seconds(2), 14, 7, nullptr, frequencyHz);
    interferenceHelper.Add(Seconds(2), 14 + 16, 8, nullptr, frequencyHz);
    interferenceHelper.Add(Seconds(2), 14 + 16, 9, nullptr, frequencyHz);
    interferenceHelper.Add(Seconds(2), 14 + 16, 10, nullptr, frequencyHz);
    NS_TEST_EXPECT_MSG_EQ(interferenceHelper.IsDestroyedByInterference(event),
                          0,
                          "Packet did not survive interference as expected");
    interferenceHelper.ClearAllEvents();
}

/**
 * @ingroup lorawan
 *
 * It tests LoraDeviceAddress comparison operators overrides and generation of new addresses with
 * LoraDeviceAddressGenerator
 */
class AddressTest : public TestCase
{
  public:
    AddressTest();           //!< Default constructor
    ~AddressTest() override; //!< Destructor

  private:
    void DoRun() override;
};

// Add some help text to this case to describe what it is intended to test
AddressTest::AddressTest()
    : TestCase("Verify that LoraDeviceAddress works as expected")
{
}

// Reminder that the test case should clean up after itself
AddressTest::~AddressTest()
{
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
AddressTest::DoRun()
{
    NS_LOG_DEBUG("AddressTest");

    //////////////////////////////////////
    // Test the LoraDeviceAddress class //
    //////////////////////////////////////

    // Address equality
    LoraDeviceAddress firstAddress(0xFFFFFFFF);
    LoraDeviceAddress secondAddress(0xFFFFFFFF);
    NS_TEST_EXPECT_MSG_EQ((firstAddress == secondAddress), true, "Addresses don't match");

    // Address ordering
    LoraDeviceAddress bigAddress(0xFFFFFF00);
    LoraDeviceAddress smallAddress(0xFFF00000);
    NS_TEST_EXPECT_MSG_EQ((bigAddress > smallAddress),
                          true,
                          "> function for addresses doesn't work correctly");

    // Setting and getting
    LoraDeviceAddress referenceAddress(0xFFFFFFFF);
    LoraDeviceAddress address(0x00000000);
    NS_TEST_EXPECT_MSG_EQ((address != referenceAddress), true, "Different addresses match!");
    address.SetNwkAddr(0xFFFFFFF);
    address.SetNwkID(0b1111111);
    NS_TEST_EXPECT_MSG_EQ((address == referenceAddress),
                          true,
                          "Addresses set to be equal don't match");

    // Serialization and deserialization
    uint8_t buffer[4];
    LoraDeviceAddress toSerialize(0x0F0F0F0F);
    toSerialize.Serialize(buffer);
    LoraDeviceAddress deserialized = LoraDeviceAddress::Deserialize(buffer);
    NS_TEST_EXPECT_MSG_EQ((toSerialize == deserialized),
                          true,
                          "Serialization + Deserialization doesn't yield an equal address");

    ///////////////////////////////////
    // Test the address generator class
    ///////////////////////////////////

    LoraDeviceAddressGenerator addressGenerator;
    for (int i = 0; i < 200; i++)
    {
        addressGenerator.NextAddress();
    }
    // After 200 iterations, the address should be 0xC9
    NS_TEST_EXPECT_MSG_EQ((addressGenerator.GetNextAddress() == LoraDeviceAddress(0xC9)),
                          true,
                          "LoraDeviceAddressGenerator doesn't increment as expected");
}

/**
 * @ingroup lorawan
 *
 * It tests serialization/deserialization of LoRaWAN headers (the LorawanMacHeader and
 * LoraFrameHeader classes) on packets
 */
class HeaderTest : public TestCase
{
  public:
    HeaderTest();           //!< Default constructor
    ~HeaderTest() override; //!< Destructor

  private:
    void DoRun() override;
};

// Add some help text to this case to describe what it is intended to test
HeaderTest::HeaderTest()
    : TestCase("Verify that LorawanMacHeader and LoraFrameHeader work as expected")
{
}

// Reminder that the test case should clean up after itself
HeaderTest::~HeaderTest()
{
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
HeaderTest::DoRun()
{
    NS_LOG_DEBUG("HeaderTest");

    //////////////////////////////////
    // Test the LorawanMacHeader class //
    //////////////////////////////////
    LorawanMacHeader macHdr;
    macHdr.SetFType(LorawanMacHeader::CONFIRMED_DATA_DOWN);
    macHdr.SetMajor(1);

    Buffer macBuf;
    macBuf.AddAtStart(100);
    Buffer::Iterator macSerialized = macBuf.Begin();
    macHdr.Serialize(macSerialized);

    macHdr.Deserialize(macSerialized);

    NS_TEST_EXPECT_MSG_EQ((macHdr.GetFType() == LorawanMacHeader::CONFIRMED_DATA_DOWN),
                          true,
                          "FType changes in the serialization/deserialization process");
    NS_TEST_EXPECT_MSG_EQ((macHdr.GetMajor() == 1),
                          true,
                          "FType changes in the serialization/deserialization process");

    ////////////////////////////////////
    // Test the LoraFrameHeader class //
    ////////////////////////////////////
    LoraFrameHeader frameHdr;
    frameHdr.SetAsDownlink();
    frameHdr.SetAck(true);
    frameHdr.SetAdr(false);
    frameHdr.SetFCnt(1);
    frameHdr.SetAddress(LoraDeviceAddress(56, 1864));
    frameHdr.AddLinkCheckAns(10, 1);

    // Serialization
    Buffer buf;
    buf.AddAtStart(100);
    Buffer::Iterator serialized = buf.Begin();
    frameHdr.Serialize(serialized);

    // Deserialization
    frameHdr.Deserialize(serialized);

    Ptr<LinkCheckAns> command = DynamicCast<LinkCheckAns>(frameHdr.GetCommands().at(0));
    uint8_t margin = command->GetMargin();
    uint8_t gwCnt = command->GetGwCnt();

    NS_TEST_EXPECT_MSG_EQ(frameHdr.GetAck(),
                          true,
                          "ACK bit changes in the serialization/deserialization process");
    NS_TEST_EXPECT_MSG_EQ(frameHdr.GetAdr(),
                          false,
                          "ADR bit changes in the serialization/deserialization process");
    NS_TEST_EXPECT_MSG_EQ(frameHdr.GetFCnt(),
                          1,
                          "FCnt changes in the serialization/deserialization process");
    NS_TEST_EXPECT_MSG_EQ((frameHdr.GetAddress() == LoraDeviceAddress(56, 1864)),
                          true,
                          "Address changes in the serialization/deserialization process");
    NS_TEST_EXPECT_MSG_EQ(margin,
                          10,
                          "Margin changes in the serialization/deserialization process");
    NS_TEST_EXPECT_MSG_EQ(gwCnt, 1, "GwCnt changes in the serialization/deserialization process");

    /////////////////////////////////////////////////
    // Test a combination of the two above classes //
    /////////////////////////////////////////////////
    Ptr<Packet> pkt = Create<Packet>(10);
    pkt->AddHeader(frameHdr);
    pkt->AddHeader(macHdr);

    // Length = Payload + FrameHeader + MacHeader
    //        = 10 + (8+3) + 1 = 22
    NS_TEST_EXPECT_MSG_EQ((pkt->GetSize()), 22, "Wrong size of packet + headers");

    LorawanMacHeader macHdr1;

    pkt->RemoveHeader(macHdr1);

    NS_TEST_EXPECT_MSG_EQ((pkt->GetSize()), 21, "Wrong size of packet + headers - macHeader");

    LoraFrameHeader frameHdr1;
    frameHdr1.SetAsDownlink();

    pkt->RemoveHeader(frameHdr1);
    Ptr<LinkCheckAns> linkCheckAns = DynamicCast<LinkCheckAns>(frameHdr1.GetCommands().at(0));

    NS_TEST_EXPECT_MSG_EQ((pkt->GetSize()),
                          10,
                          "Wrong size of packet + headers - macHeader - frameHeader");

    // Verify contents of removed MAC header
    NS_TEST_EXPECT_MSG_EQ(macHdr1.GetFType(),
                          macHdr.GetFType(),
                          "Removed header contents don't match");
    NS_TEST_EXPECT_MSG_EQ(macHdr1.GetMajor(),
                          macHdr.GetMajor(),
                          "Removed header contents don't match");

    // Verify contents of removed frame header
    NS_TEST_EXPECT_MSG_EQ(frameHdr1.GetAck(),
                          frameHdr.GetAck(),
                          "Removed header contents don't match");
    NS_TEST_EXPECT_MSG_EQ(frameHdr1.GetAdr(),
                          frameHdr.GetAdr(),
                          "Removed header contents don't match");
    NS_TEST_EXPECT_MSG_EQ(frameHdr1.GetFCnt(),
                          frameHdr.GetFCnt(),
                          "Removed header contents don't match");
    NS_TEST_EXPECT_MSG_EQ((frameHdr1.GetAddress() == frameHdr.GetAddress()),
                          true,
                          "Removed header contents don't match");
    NS_TEST_EXPECT_MSG_EQ(linkCheckAns->GetMargin(),
                          10,
                          "Removed header's MAC command contents don't match");
    NS_TEST_EXPECT_MSG_EQ(linkCheckAns->GetGwCnt(),
                          1,
                          "Removed header's MAC command contents don't match");
}

/**
 * @ingroup lorawan
 *
 * It tests that LoraBeaconHeader serializes and deserializes correctly (Class B)
 */
class BeaconHeaderTest : public TestCase
{
  public:
    BeaconHeaderTest();           //!< Default constructor
    ~BeaconHeaderTest() override; //!< Destructor

  private:
    void DoRun() override;
};

// Add some help text to this case to describe what it is intended to test
BeaconHeaderTest::BeaconHeaderTest()
    : TestCase("Verify that LoraBeaconHeader works as expected")
{
}

// Reminder that the test case should clean up after itself
BeaconHeaderTest::~BeaconHeaderTest()
{
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
BeaconHeaderTest::DoRun()
{
    NS_LOG_DEBUG("BeaconHeaderTest");

    // A few representative Time field values, including the edges of the uint32_t range
    std::vector<uint32_t> timesToTest = {0, 128, 1234567, 4294967295u};

    for (uint32_t timeSecs : timesToTest)
    {
        LoraBeaconHeader beaconHdr;
        beaconHdr.SetTime(timeSecs);

        NS_TEST_EXPECT_MSG_EQ(beaconHdr.GetSerializedSize(),
                              4,
                              "LoraBeaconHeader is expected to serialize to 4 bytes");

        Buffer buf;
        buf.AddAtStart(100);
        Buffer::Iterator serialized = buf.Begin();
        beaconHdr.Serialize(serialized);

        LoraBeaconHeader deserializedHdr;
        uint32_t consumed = deserializedHdr.Deserialize(serialized);

        NS_TEST_EXPECT_MSG_EQ(consumed, 4, "LoraBeaconHeader is expected to consume 4 bytes");
        NS_TEST_EXPECT_MSG_EQ(deserializedHdr.GetTime(),
                              timeSecs,
                              "Time field changes in the serialization/deserialization process");
    }

    // Also verify it round-trips correctly when attached to and removed from a real Packet,
    // the same way it will actually be used (see gateway-lorawan-mac.cc's SendBeacon)
    LoraBeaconHeader beaconHdr;
    beaconHdr.SetTime(128);

    Ptr<Packet> pkt = Create<Packet>(0);
    pkt->AddHeader(beaconHdr);

    NS_TEST_EXPECT_MSG_EQ(pkt->GetSize(), 4, "Wrong size of packet + beacon header");

    LoraBeaconHeader beaconHdr1;
    pkt->RemoveHeader(beaconHdr1);

    NS_TEST_EXPECT_MSG_EQ(pkt->GetSize(), 0, "Wrong size of packet after removing beacon header");
    NS_TEST_EXPECT_MSG_EQ(beaconHdr1.GetTime(),
                          beaconHdr.GetTime(),
                          "Removed header contents don't match");
}

/**
 * @ingroup lorawan
 *
 * It tests that the PingSlotInfoReq/PingSlotInfoAns MAC command pair serializes and
 * deserializes correctly through a LoraFrameHeader (Class B)
 */
class PingSlotInfoCommandTest : public TestCase
{
  public:
    PingSlotInfoCommandTest();           //!< Default constructor
    ~PingSlotInfoCommandTest() override; //!< Destructor

  private:
    void DoRun() override;
};

// Add some help text to this case to describe what it is intended to test
PingSlotInfoCommandTest::PingSlotInfoCommandTest()
    : TestCase("Verify that PingSlotInfoReq and PingSlotInfoAns work as expected")
{
}

// Reminder that the test case should clean up after itself
PingSlotInfoCommandTest::~PingSlotInfoCommandTest()
{
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
PingSlotInfoCommandTest::DoRun()
{
    NS_LOG_DEBUG("PingSlotInfoCommandTest");

    ///////////////////////////////////////////////////////////////
    // PingSlotInfoReq: sent uplink (by the end device), a few    //
    // representative periodicity values including the edges      //
    ///////////////////////////////////////////////////////////////
    for (uint8_t periodicity : {uint8_t(0), uint8_t(4), uint8_t(7)})
    {
        LoraFrameHeader frameHdr;
        frameHdr.SetAsUplink();
        frameHdr.SetAddress(LoraDeviceAddress(1, 1));
        frameHdr.AddPingSlotInfoReq(periodicity);

        NS_TEST_EXPECT_MSG_EQ(unsigned(frameHdr.GetFOptsLen()),
                              2,
                              "PingSlotInfoReq is expected to serialize to 2 bytes");

        Buffer buf;
        buf.AddAtStart(100);
        Buffer::Iterator serialized = buf.Begin();
        frameHdr.Serialize(serialized);

        LoraFrameHeader deserializedHdr;
        deserializedHdr.SetAsUplink();
        deserializedHdr.Deserialize(serialized);

        Ptr<PingSlotInfoReq> command = deserializedHdr.GetMacCommand<PingSlotInfoReq>();
        NS_TEST_ASSERT_MSG_NE(command, nullptr, "PingSlotInfoReq not found after deserialization");
        NS_TEST_EXPECT_MSG_EQ(unsigned(command->GetPeriodicity()),
                              unsigned(periodicity),
                              "Periodicity changes in the serialization/deserialization process");
    }

    ///////////////////////////////////////////////////////////////////////
    // PingSlotInfoAns: sent downlink (by the network server), no fields //
    ///////////////////////////////////////////////////////////////////////
    LoraFrameHeader ansFrameHdr;
    ansFrameHdr.SetAsDownlink();
    ansFrameHdr.AddPingSlotInfoAns();

    NS_TEST_EXPECT_MSG_EQ(unsigned(ansFrameHdr.GetFOptsLen()),
                          1,
                          "PingSlotInfoAns is expected to serialize to 1 byte");

    Buffer ansBuf;
    ansBuf.AddAtStart(100);
    Buffer::Iterator ansSerialized = ansBuf.Begin();
    ansFrameHdr.Serialize(ansSerialized);

    LoraFrameHeader deserializedAnsHdr;
    deserializedAnsHdr.SetAsDownlink();
    deserializedAnsHdr.Deserialize(ansSerialized);

    Ptr<PingSlotInfoAns> ansCommand = deserializedAnsHdr.GetMacCommand<PingSlotInfoAns>();
    NS_TEST_ASSERT_MSG_NE(ansCommand, nullptr, "PingSlotInfoAns not found after deserialization");
}

/**
 * @ingroup lorawan
 *
 * Regression test pinning down the exact mechanism behind a latent crash in
 * LoraFrameHeader::Deserialize(): a caller that forgets to call SetAsUplink()/SetAsDownlink()
 * before deserializing picks the wrong one of the two (non-interchangeable) CID tables. Uses
 * DlChannelAns (CID 0x0A), which exists only in the uplink table, so deserializing it under the
 * downlink table used to fall into Deserialize()'s unhandled-CID case, leaving a null
 * Ptr<MacCommand> that was dereferenced unconditionally right after (observed upstream as a
 * SIGABRT -- see NetworkStatus::GetEndDeviceStatus(Ptr<const Packet>), which used to be missing
 * exactly this SetAsUplink() call). Deserialize() now also refuses to dereference a null command
 * (breaking out of the FOpts loop instead), so this also verifies that a still-mismarked header
 * degrades to "parsed zero commands" rather than crashing.
 */
class FrameHeaderDirectionRequiredForCidTableTest : public TestCase
{
  public:
    FrameHeaderDirectionRequiredForCidTableTest();           //!< Default constructor
    ~FrameHeaderDirectionRequiredForCidTableTest() override; //!< Destructor

  private:
    void DoRun() override;
};

// Add some help text to this case to describe what it is intended to test
FrameHeaderDirectionRequiredForCidTableTest::FrameHeaderDirectionRequiredForCidTableTest()
    : TestCase("Verify that LoraFrameHeader::Deserialize() needs SetAsUplink() to correctly parse "
               "an uplink-only MAC command, and does not crash when that call is missing")
{
}

// Reminder that the test case should clean up after itself
FrameHeaderDirectionRequiredForCidTableTest::~FrameHeaderDirectionRequiredForCidTableTest()
{
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
FrameHeaderDirectionRequiredForCidTableTest::DoRun()
{
    NS_LOG_DEBUG("FrameHeaderDirectionRequiredForCidTableTest");

    LoraFrameHeader frameHdr;
    frameHdr.SetAsUplink();
    frameHdr.SetAddress(LoraDeviceAddress(1, 1));
    frameHdr.AddCommand(Create<DlChannelAns>());

    Buffer buf;
    buf.AddAtStart(100);
    Buffer::Iterator serialized = buf.Begin();
    frameHdr.Serialize(serialized);

    // Correctly marked uplink: CID 0x0A resolves via the uplink table.
    LoraFrameHeader uplinkHdr;
    uplinkHdr.SetAsUplink();
    uplinkHdr.Deserialize(buf.Begin());
    Ptr<DlChannelAns> command = uplinkHdr.GetMacCommand<DlChannelAns>();
    NS_TEST_ASSERT_MSG_NE(command,
                          nullptr,
                          "DlChannelAns not found after deserializing with SetAsUplink()");

    // Left at its default (downlink): CID 0x0A has no match in the downlink table. Must not
    // crash, and must parse zero MAC commands rather than silently misinterpreting the bytes.
    LoraFrameHeader mismarkedHdr;
    mismarkedHdr.Deserialize(buf.Begin());
    NS_TEST_EXPECT_MSG_EQ(mismarkedHdr.GetCommands().size(),
                          0u,
                          "An uplink-only CID parsed under the downlink table should be dropped, "
                          "not misinterpreted");
}

/**
 * @ingroup lorawan
 *
 * It tests ClassBEndDeviceLorawanMac::PingNbSlots() and ::ComputePingOffset() (Class B)
 */
class PingSlotOffsetTest : public TestCase
{
  public:
    PingSlotOffsetTest();           //!< Default constructor
    ~PingSlotOffsetTest() override; //!< Destructor

  private:
    void DoRun() override;
};

// Add some help text to this case to describe what it is intended to test
PingSlotOffsetTest::PingSlotOffsetTest()
    : TestCase("Verify that ClassBEndDeviceLorawanMac's ping slot offset computation works as "
               "expected")
{
}

// Reminder that the test case should clean up after itself
PingSlotOffsetTest::~PingSlotOffsetTest()
{
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
PingSlotOffsetTest::DoRun()
{
    NS_LOG_DEBUG("PingSlotOffsetTest");

    // PingNbSlots(): 2^(7 - periodicity)
    NS_TEST_EXPECT_MSG_EQ(ClassBEndDeviceLorawanMac::PingNbSlots(0),
                          128,
                          "Unexpected PingNbSlots for periodicity 0");
    NS_TEST_EXPECT_MSG_EQ(ClassBEndDeviceLorawanMac::PingNbSlots(4),
                          8,
                          "Unexpected PingNbSlots for periodicity 4 (the module's own default)");
    NS_TEST_EXPECT_MSG_EQ(ClassBEndDeviceLorawanMac::PingNbSlots(7),
                          1,
                          "Unexpected PingNbSlots for periodicity 7");

    // ComputePingOffset(): determinism and range. The offset is deliberately bounded by
    // pingPeriod itself (not, e.g., by a slot count) -- see the Doxygen comment -- so that a
    // period's last ping slot can never land at or past the next beacon's due time.
    Time pingPeriod = Seconds(16); // BEACON_INTERVAL / PingNbSlots(4), the module's own default
    LoraDeviceAddress addr(42);
    Time o1 = ClassBEndDeviceLorawanMac::ComputePingOffset(addr, 128, pingPeriod);
    Time o2 = ClassBEndDeviceLorawanMac::ComputePingOffset(addr, 128, pingPeriod);
    NS_TEST_EXPECT_MSG_EQ(o1, o2, "ComputePingOffset should be deterministic for the same inputs");
    NS_TEST_EXPECT_MSG_EQ((o1 >= Time(0) && o1 < pingPeriod),
                          true,
                          "Offset should be within [0, pingPeriod)");

    // A different beacon time should (almost always) produce a different offset -- not asserted
    // as an absolute guarantee (a hash collision is possible), just checked as a sanity signal.
    Time o3 = ClassBEndDeviceLorawanMac::ComputePingOffset(addr, 256, pingPeriod);
    NS_LOG_DEBUG("Offset for beaconTime=128: " << o1.As(Time::S)
                                               << ", for beaconTime=256: " << o3.As(Time::S));

    // Statistical spread: many DevAddrs against a fixed beacon time should land roughly
    // uniformly across [0, pingPeriod), not bunched into a handful of buckets. Discretized into
    // 16 equal-width buckets across pingPeriod for this check.
    const uint32_t nBuckets = 16;
    const uint32_t nAddresses = 4096;
    std::vector<uint32_t> bucketCounts(nBuckets, 0);
    for (uint32_t a = 1; a <= nAddresses; ++a)
    {
        Time offset =
            ClassBEndDeviceLorawanMac::ComputePingOffset(LoraDeviceAddress(a), 128, pingPeriod);
        NS_TEST_ASSERT_MSG_EQ((offset >= Time(0) && offset < pingPeriod),
                              true,
                              "Offset out of range");
        uint32_t bucket =
            uint32_t(offset.GetNanoSeconds() * nBuckets / pingPeriod.GetNanoSeconds());
        bucketCounts.at(std::min(bucket, nBuckets - 1))++;
    }
    uint32_t minCount = *std::min_element(bucketCounts.begin(), bucketCounts.end());
    uint32_t maxCount = *std::max_element(bucketCounts.begin(), bucketCounts.end());
    NS_LOG_DEBUG("Bucket counts range: [" << minCount << ", " << maxCount << "], expected average "
                                          << (nAddresses / nBuckets));
    NS_TEST_EXPECT_MSG_EQ((minCount > 0),
                          true,
                          "Every bucket should have gotten at least one device out of "
                              << nAddresses << " samples if the hash is reasonably well-mixed");
    NS_TEST_EXPECT_MSG_EQ((maxCount < 4 * (nAddresses / nBuckets)),
                          true,
                          "No single bucket should be wildly over-represented (>4x the "
                          "expected average) if the hash is reasonably well-mixed");
}

/**
 * @ingroup lorawan
 *
 * It tests a number of cases related to SimpleGatewayLoraPhy's parallel reception paths
 */
class ReceivePathTest : public TestCase
{
  public:
    ReceivePathTest();           //!< Default constructor
    ~ReceivePathTest() override; //!< Destructor

  private:
    void DoRun() override;
    /**
     * Reset counters and gateway PHY for new sub test case.
     *
     * @param rxPathNb Number of reception paths to be created on the gateway PHY
     */
    void Reset(uint8_t rxPathNb);
    /**
     * Callback for tracing OccupiedReceptionPaths.
     *
     * @param oldValue The old value.
     * @param newValue The new value.
     */
    void OccupiedReceptionPaths(int oldValue, int newValue);
    /**
     * Callback for tracing LostPacketBecauseNoMoreReceivers.
     *
     * @param packet The packet lost.
     * @param node The receiver node id if any, 0 otherwise.
     */
    void NoMoreDemodulators(Ptr<const Packet> packet, uint32_t node);
    /**
     * Callback for tracing LostPacketBecauseInterference.
     *
     * @param packet The packet lost.
     * @param node The receiver node id if any, 0 otherwise.
     */
    void Interference(Ptr<const Packet> packet, uint32_t node);
    /**
     * Callback for tracing ReceivedPacket.
     *
     * @param packet The packet received.
     * @param node The receiver node id if any, 0 otherwise.
     */
    void ReceivedPacket(Ptr<const Packet> packet, uint32_t node);

    Ptr<SimpleGatewayLoraPhy> gatewayPhy; //!< PHY layer of a gateway to be tested

    int m_noMoreDemodulatorsCalls = 0;   //!< Counter for LostPacketBecauseNoMoreReceivers calls
    int m_interferenceCalls = 0;         //!< Counter for LostPacketBecauseInterference calls
    int m_receivedPacketCalls = 0;       //!< Counter for ReceivedPacket calls
    int m_maxOccupiedReceptionPaths = 0; //!< Max number of concurrent OccupiedReceptionPaths
};

// Add some help text to this case to describe what it is intended to test
ReceivePathTest::ReceivePathTest()
    : TestCase("Verify that ReceivePaths work as expected")
{
}

// Reminder that the test case should clean up after itself
ReceivePathTest::~ReceivePathTest()
{
}

void
ReceivePathTest::Reset(uint8_t rxPathNb)
{
    m_noMoreDemodulatorsCalls = 0;
    m_interferenceCalls = 0;
    m_receivedPacketCalls = 0;
    m_maxOccupiedReceptionPaths = 0;

    // The following tests are designed around GOURSAUD signal-to-interference matrix
    LoraInterferenceHelper::collisionMatrix = LoraInterferenceHelper::GOURSAUD;

    gatewayPhy = CreateObject<SimpleGatewayLoraPhy>();

    gatewayPhy->AddFrequency(868'100'000);
    gatewayPhy->AddFrequency(868'300'000);
    gatewayPhy->AddFrequency(868'500'000);

    for (uint8_t i = 0; i < rxPathNb; i++)
    {
        gatewayPhy->AddReceptionPath();
    }

    // From GatewayLoraPhy
    gatewayPhy->TraceConnectWithoutContext(
        "LostPacketBecauseNoMoreReceivers",
        MakeCallback(&ReceivePathTest::NoMoreDemodulators, this));
    gatewayPhy->TraceConnectWithoutContext(
        "OccupiedReceptionPaths",
        MakeCallback(&ReceivePathTest::OccupiedReceptionPaths, this));

    // From LoraPhy
    gatewayPhy->TraceConnectWithoutContext("LostPacketBecauseInterference",
                                           MakeCallback(&ReceivePathTest::Interference, this));
    gatewayPhy->TraceConnectWithoutContext("ReceivedPacket",
                                           MakeCallback(&ReceivePathTest::ReceivedPacket, this));
}

void
ReceivePathTest::OccupiedReceptionPaths(int oldValue, int newValue)
{
    NS_LOG_FUNCTION(oldValue << newValue);

    if (m_maxOccupiedReceptionPaths < newValue)
    {
        m_maxOccupiedReceptionPaths = newValue;
    }
}

void
ReceivePathTest::NoMoreDemodulators(Ptr<const Packet> packet, uint32_t node)
{
    NS_LOG_FUNCTION(packet << node);

    m_noMoreDemodulatorsCalls++;
}

void
ReceivePathTest::Interference(Ptr<const Packet> packet, uint32_t node)
{
    NS_LOG_FUNCTION(packet << node);

    m_interferenceCalls++;
}

void
ReceivePathTest::ReceivedPacket(Ptr<const Packet> packet, uint32_t node)
{
    NS_LOG_FUNCTION(packet << node);

    m_receivedPacketCalls++;
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
ReceivePathTest::DoRun()
{
    NS_LOG_DEBUG("ReceivePathTest");

    Ptr<Packet> packet = Create<Packet>();

    ///////////////////////////////////////////////////////////
    // If no ReceptionPath is configured, no packet is received
    ///////////////////////////////////////////////////////////

    Reset(0);

    Simulator::Schedule(Seconds(1),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        7,
                        14,
                        Seconds(1));

    Simulator::Stop(Hours(2));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_noMoreDemodulatorsCalls, 1, "Unexpected value");

    //////////////////////////////////////////////////////////////////////////////
    // A ReceptionPath can receive a packet of any SF without any preconfiguration
    //////////////////////////////////////////////////////////////////////////////

    Reset(1);

    Simulator::Schedule(Seconds(1),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        7,
                        14,
                        Seconds(1));
    Simulator::Schedule(Seconds(3),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        8,
                        14,
                        Seconds(1));
    Simulator::Schedule(Seconds(5),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        9,
                        14,
                        Seconds(1));
    Simulator::Schedule(Seconds(7),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        10,
                        14,
                        Seconds(1));
    Simulator::Schedule(Seconds(9),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        11,
                        14,
                        Seconds(1));
    Simulator::Schedule(Seconds(11),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        12,
                        14,
                        Seconds(1));

    Simulator::Stop(Hours(2));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_noMoreDemodulatorsCalls, 0, "Unexpected value");
    NS_TEST_EXPECT_MSG_EQ(m_receivedPacketCalls, 6, "Unexpected value");

    ///////////////////////////////////////////////////////////////////////////////////////
    // Schedule two overlapping reception events. Each packet should be received correctly.
    ///////////////////////////////////////////////////////////////////////////////////////

    Reset(2);

    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        7,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(3),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        9,
                        14,
                        Seconds(4));

    Simulator::Stop(Hours(2));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_noMoreDemodulatorsCalls, 0, "Unexpected value");
    NS_TEST_EXPECT_MSG_EQ(m_receivedPacketCalls, 2, "Unexpected value");
    NS_TEST_EXPECT_MSG_EQ(m_maxOccupiedReceptionPaths, 2, "Unexpected value");

    //////////////////////////////////////////////////////////////////////////////////
    // Interference between packets on the same frequency and different ReceptionPaths
    //////////////////////////////////////////////////////////////////////////////////

    Reset(2);

    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        7,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(3),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        7,
                        14,
                        Seconds(4));

    Simulator::Stop(Hours(2));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_noMoreDemodulatorsCalls, 0, "Unexpected value");
    NS_TEST_EXPECT_MSG_EQ(m_interferenceCalls, 2, "Unexpected value");

    /////////////////////////////////////////////////////////////
    // Three receptions where only two receivePaths are available
    /////////////////////////////////////////////////////////////

    Reset(2);

    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        7,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        7,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(3),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        7,
                        14,
                        Seconds(4));

    Simulator::Stop(Hours(2));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_noMoreDemodulatorsCalls, 1, "Unexpected value");

    ///////////////////////////////////////////////////////////////////////////
    // Packets that are on different frequencys do not interfere
    ///////////////////////////////////////////////////////////////////////////

    Reset(2);

    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        7,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'300'000,
                        IQPolarity::UP,
                        7,
                        14,
                        Seconds(4));

    Simulator::Stop(Hours(2));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_interferenceCalls, 0, "Unexpected value");

    ///////////////////////////////////////////////////////////////////////////
    // Full capacity (siw packets, on six SFs, distributed over 3 frequencies)
    ///////////////////////////////////////////////////////////////////////////

    Reset(6);

    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        7,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        8,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'300'000,
                        IQPolarity::UP,
                        9,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'300'000,
                        IQPolarity::UP,
                        10,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'500'000,
                        IQPolarity::UP,
                        11,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'500'000,
                        IQPolarity::UP,
                        12,
                        14,
                        Seconds(4));

    Simulator::Stop(Hours(2));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_noMoreDemodulatorsCalls, 0, "Unexpected value");
    NS_TEST_EXPECT_MSG_EQ(m_interferenceCalls, 0, "Unexpected value");
    NS_TEST_EXPECT_MSG_EQ(m_receivedPacketCalls, 6, "Unexpected value");

    ///////////////////////////////////////////////////////////////////////////
    // Full capacity + 1
    ///////////////////////////////////////////////////////////////////////////

    Reset(6);

    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        7,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        8,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'300'000,
                        IQPolarity::UP,
                        9,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'300'000,
                        IQPolarity::UP,
                        10,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'500'000,
                        IQPolarity::UP,
                        11,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'500'000,
                        IQPolarity::UP,
                        12,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'500'000,
                        IQPolarity::UP,
                        10,
                        14,
                        Seconds(4));

    Simulator::Stop(Hours(2));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_noMoreDemodulatorsCalls, 1, "Unexpected value");
    NS_TEST_EXPECT_MSG_EQ(m_interferenceCalls, 0, "Unexpected value");
    NS_TEST_EXPECT_MSG_EQ(m_receivedPacketCalls, 6, "Unexpected value");

    ////////////////////////////////////
    // Receive Paths are correctly freed
    ////////////////////////////////////

    Reset(6);

    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        7,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        8,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'300'000,
                        IQPolarity::UP,
                        9,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'300'000,
                        IQPolarity::UP,
                        10,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'500'000,
                        IQPolarity::UP,
                        11,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'500'000,
                        IQPolarity::UP,
                        12,
                        14,
                        Seconds(4));

    Simulator::Schedule(Seconds(8),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        7,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(8),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        8,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(8),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'300'000,
                        IQPolarity::UP,
                        9,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(8),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'300'000,
                        IQPolarity::UP,
                        10,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(8),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'500'000,
                        IQPolarity::UP,
                        11,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(8),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'500'000,
                        IQPolarity::UP,
                        12,
                        14,
                        Seconds(4));

    Simulator::Stop(Hours(2));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_noMoreDemodulatorsCalls, 0, "Unexpected value");
    NS_TEST_EXPECT_MSG_EQ(m_interferenceCalls, 0, "Unexpected value");
    NS_TEST_EXPECT_MSG_EQ(m_receivedPacketCalls, 12, "Unexpected value");

    /////////////////////////////////////////////////////////////
    // Receive Paths stay occupied exactly for the necessary time
    /////////////////////////////////////////////////////////////

    Reset(2);

    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        7,
                        14,
                        Seconds(4));
    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        8,
                        14,
                        Seconds(4));

    // This packet will find no free ReceptionPaths
    Simulator::Schedule(Seconds(2 + 4) - NanoSeconds(1),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        9,
                        14,
                        Seconds(4));

    // This packet will find a free ReceptionPath
    Simulator::Schedule(Seconds(2 + 4) + NanoSeconds(1),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        10,
                        14,
                        Seconds(4));

    Simulator::Stop(Hours(2));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_noMoreDemodulatorsCalls, 1, "Unexpected value");
    NS_TEST_EXPECT_MSG_EQ(m_interferenceCalls, 0, "Unexpected value");
    NS_TEST_EXPECT_MSG_EQ(m_receivedPacketCalls, 3, "Unexpected value");

    ////////////////////////////////////////////////////
    // Only one ReceivePath locks on the incoming packet
    ////////////////////////////////////////////////////

    Reset(6);

    Simulator::Schedule(Seconds(2),
                        &SimpleGatewayLoraPhy::StartReceive,
                        gatewayPhy,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        7,
                        14,
                        Seconds(4));

    Simulator::Stop(Hours(2));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_noMoreDemodulatorsCalls, 0, "Unexpected value");
    NS_TEST_EXPECT_MSG_EQ(m_interferenceCalls, 0, "Unexpected value");
    NS_TEST_EXPECT_MSG_EQ(m_receivedPacketCalls, 1, "Unexpected value");
    NS_TEST_EXPECT_MSG_EQ(m_maxOccupiedReceptionPaths, 1, "Unexpected value");
}

/**
 * @ingroup lorawan
 *
 * It tests functionality of the LogicalLoraChannel, SubBand and LogicalLoraChannelHelper classes
 */
class LogicalLoraChannelTest : public TestCase
{
  public:
    LogicalLoraChannelTest();           //!< Default constructor
    ~LogicalLoraChannelTest() override; //!< Destructor

  private:
    void DoRun() override;
};

// Add some help text to this case to describe what it is intended to test
LogicalLoraChannelTest::LogicalLoraChannelTest()
    : TestCase("Verify that LogicalLoraChannel and LogicalLoraChannelHelper work as expected")
{
}

// Reminder that the test case should clean up after itself
LogicalLoraChannelTest::~LogicalLoraChannelTest()
{
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
LogicalLoraChannelTest::DoRun()
{
    NS_LOG_DEBUG("LogicalLoraChannelTest");

    /////////////////////////////
    // Test LogicalLoraChannel //
    /////////////////////////////

    // Setup
    Ptr<LogicalLoraChannel> channel1 = Create<LogicalLoraChannel>(868000000, 0, 5);
    Ptr<LogicalLoraChannel> channel2 = Create<LogicalLoraChannel>(868000000, 0, 5);
    Ptr<LogicalLoraChannel> channel3 = Create<LogicalLoraChannel>(868100000, 0, 5);
    Ptr<LogicalLoraChannel> channel4 = Create<LogicalLoraChannel>(868001000, 0, 5);

    // Equality between channels
    // Test the == and != operators
    NS_TEST_EXPECT_MSG_EQ(channel1, channel2, "== operator doesn't work as expected");
    NS_TEST_EXPECT_MSG_NE(channel1, channel3, "!= operator doesn't work as expected");
    NS_TEST_EXPECT_MSG_NE(channel1, channel4, "!= operator doesn't work as expected");

    //////////////////
    // Test SubBand //
    //////////////////
    // Setup

    auto subBand = Create<SubBand>(868000000, 868600000, 0.01, 14);
    Ptr<LogicalLoraChannel> channel5 = Create<LogicalLoraChannel>(870000000, 0, 5);

    // Test Contains
    NS_TEST_EXPECT_MSG_EQ(subBand->Contains(channel3),
                          true,
                          "Contains does not behave as expected");
    NS_TEST_EXPECT_MSG_EQ(subBand->Contains(channel3->GetFrequency()),
                          true,
                          "Contains does not behave as expected");
    NS_TEST_EXPECT_MSG_EQ(subBand->Contains(channel5),
                          false,
                          "Contains does not behave as expected");

    ///////////////////////////////////
    // Test LogicalLoraChannelHelper //
    ///////////////////////////////////

    // Setup
    auto channelHelper = Create<LogicalLoraChannelHelper>(16);
    auto subBand1 = Create<SubBand>(869400000, 869650000, 0.10, 27);
    channel1 = Create<LogicalLoraChannel>(868100000, 0, 5);
    channel2 = Create<LogicalLoraChannel>(868300000, 0, 5);
    channel3 = Create<LogicalLoraChannel>(869525000, 0, 5);

    // Channel diagram
    //
    // Channels      1     2                              3
    // SubBands  868 ----- 1% ----- 868.6      869 ----- 10% ----- 869.4

    // Add SubBands and LogicalLoraChannels to the helper
    channelHelper->AddSubBand(subBand);
    channelHelper->AddSubBand(subBand1);
    channelHelper->SetChannel(0, channel1);
    channelHelper->SetChannel(1, channel2);
    channelHelper->SetChannel(2, channel3);

    // Duty Cycle tests
    // (high level duty cycle behavior)
    ///////////////////////////////////

    channelHelper->AddEvent(Seconds(2), channel1);
    Time expectedTimeOff = Seconds(2 / 0.01);

    // Wait time is computed correctly
    NS_TEST_EXPECT_MSG_EQ(channelHelper->GetWaitTime(channel1),
                          expectedTimeOff,
                          "Wait time doesn't behave as expected");

    // Duty Cycle involves the whole SubBand, not just a channel
    NS_TEST_EXPECT_MSG_EQ(channelHelper->GetWaitTime(channel2),
                          expectedTimeOff,
                          "Wait time doesn't behave as expected");

    // Other bands are not affected by this transmission
    NS_TEST_EXPECT_MSG_EQ(channelHelper->GetWaitTime(channel3),
                          Time(0),
                          "Wait time affects other subbands");
}

/**
 * @ingroup lorawan
 *
 * It tests the correctness of the LoraPhy::GetTimeOnAir calculator against a number of pre-sourced
 * time values of known scenarios
 */
class TimeOnAirTest : public TestCase
{
  public:
    TimeOnAirTest();           //!< Default constructor
    ~TimeOnAirTest() override; //!< Destructor

  private:
    void DoRun() override;
};

// Add some help text to this case to describe what it is intended to test
TimeOnAirTest::TimeOnAirTest()
    : TestCase(
          "Verify that LoraPhy's function to compute the time on air of a packet works as expected")
{
}

// Reminder that the test case should clean up after itself
TimeOnAirTest::~TimeOnAirTest()
{
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
TimeOnAirTest::DoRun()
{
    NS_LOG_DEBUG("TimeOnAirTest");

    Ptr<Packet> packet;
    Time duration;

    // Starting parameters
    packet = Create<Packet>(10);
    LoraTxParameters txParams;
    txParams.spreadingFactor = 7;
    txParams.bandwidthHz = 125'000;
    txParams.codingRate = CodingRate::CR_4_5;
    txParams.lowDataRateOptimize = false;
    txParams.preambleLenSymb = 8;
    txParams.implicitHeader = false;
    txParams.crcEnabled = true;

    duration = LoraPhy::GetTimeOnAir(packet->GetSize(), txParams);
    NS_TEST_EXPECT_MSG_EQ_TOL(duration.GetSeconds(), 0.041216, 0.0001, "Unexpected duration");

    txParams.spreadingFactor = 8;
    duration = LoraPhy::GetTimeOnAir(packet->GetSize(), txParams);
    NS_TEST_EXPECT_MSG_EQ_TOL(duration.GetSeconds(), 0.072192, 0.0001, "Unexpected duration");

    txParams.implicitHeader = true;
    duration = LoraPhy::GetTimeOnAir(packet->GetSize(), txParams);
    NS_TEST_EXPECT_MSG_EQ_TOL(duration.GetSeconds(), 0.072192, 0.0001, "Unexpected duration");

    txParams.codingRate = CodingRate::CR_4_6;
    duration = LoraPhy::GetTimeOnAir(packet->GetSize(), txParams);
    NS_TEST_EXPECT_MSG_EQ_TOL(duration.GetSeconds(), 0.078336, 0.0001, "Unexpected duration");

    txParams.preambleLenSymb = 10;
    duration = LoraPhy::GetTimeOnAir(packet->GetSize(), txParams);
    NS_TEST_EXPECT_MSG_EQ_TOL(duration.GetSeconds(), 0.082432, 0.0001, "Unexpected duration");

    txParams.lowDataRateOptimize = true;
    duration = LoraPhy::GetTimeOnAir(packet->GetSize(), txParams);
    NS_TEST_EXPECT_MSG_EQ_TOL(duration.GetSeconds(), 0.082432, 0.0001, "Unexpected duration");

    txParams.spreadingFactor = 10;
    duration = LoraPhy::GetTimeOnAir(packet->GetSize(), txParams);
    NS_TEST_EXPECT_MSG_EQ_TOL(duration.GetSeconds(), 0.280576, 0.0001, "Unexpected duration");

    txParams.bandwidthHz = 250000;
    duration = LoraPhy::GetTimeOnAir(packet->GetSize(), txParams);
    NS_TEST_EXPECT_MSG_EQ_TOL(duration.GetSeconds(), 0.14028, 0.0001, "Unexpected duration");

    txParams.bandwidthHz = 500000;
    duration = LoraPhy::GetTimeOnAir(packet->GetSize(), txParams);
    NS_TEST_EXPECT_MSG_EQ_TOL(duration.GetSeconds(), 0.070144, 0.0001, "Unexpected duration");

    txParams.implicitHeader = false;
    duration = LoraPhy::GetTimeOnAir(packet->GetSize(), txParams);
    NS_TEST_EXPECT_MSG_EQ_TOL(duration.GetSeconds(), 0.082432, 0.0001, "Unexpected duration");

    txParams.preambleLenSymb = 8;
    duration = LoraPhy::GetTimeOnAir(packet->GetSize(), txParams);
    NS_TEST_EXPECT_MSG_EQ_TOL(duration.GetSeconds(), 0.078336, 0.0001, "Unexpected duration");

    txParams.spreadingFactor = 12;
    duration = LoraPhy::GetTimeOnAir(packet->GetSize(), txParams);
    NS_TEST_EXPECT_MSG_EQ_TOL(duration.GetSeconds(), 0.264192, 0.0001, "Unexpected duration");

    packet = Create<Packet>(50);
    duration = LoraPhy::GetTimeOnAir(packet->GetSize(), txParams);
    NS_TEST_EXPECT_MSG_EQ_TOL(duration.GetSeconds(), 0.657408, 0.0001, "Unexpected duration");

    txParams.bandwidthHz = 125000;
    duration = LoraPhy::GetTimeOnAir(packet->GetSize(), txParams);
    NS_TEST_EXPECT_MSG_EQ_TOL(duration.GetSeconds(), 2.629632, 0.0001, "Unexpected duration");

    txParams.codingRate = CodingRate::CR_4_5;
    duration = LoraPhy::GetTimeOnAir(packet->GetSize(), txParams);
    NS_TEST_EXPECT_MSG_EQ_TOL(duration.GetSeconds(), 2.301952, 0.0001, "Unexpected duration");
}

/**
 * @ingroup lorawan
 *
 * It tests sending packets over a LoRa physical channel between multiple devices and the resulting
 * possible outcomes
 */
class PhyConnectivityTest : public TestCase
{
  public:
    PhyConnectivityTest();           //!< Default constructor
    ~PhyConnectivityTest() override; //!< Destructor

    /**
     * Reset counters and end devices' PHYs for new sub test case.
     */
    void Reset();

    /**
     * Callback for tracing ReceivedPacket.
     *
     * @param packet The packet received.
     * @param node The receiver node id if any, 0 otherwise.
     */
    void ReceivedPacket(Ptr<const Packet> packet, uint32_t node);

    /**
     * Callback for tracing LostPacketBecauseUnderSensitivity.
     *
     * @param packet The packet lost.
     * @param node The receiver node id if any, 0 otherwise.
     */
    void UnderSensitivity(Ptr<const Packet> packet, uint32_t node);

    /**
     * Callback for tracing LostPacketBecauseInterference.
     *
     * @param packet The packet lost.
     * @param node The receiver node id if any, 0 otherwise.
     */
    void Interference(Ptr<const Packet> packet, uint32_t node);

    /**
     * Callback for tracing LostPacketBecauseWrongFrequency.
     *
     * @param packet The packet lost.
     * @param node The receiver node id if any, 0 otherwise.
     */
    void WrongFrequency(Ptr<const Packet> packet, uint32_t node);

    /**
     * Callback for tracing LostPacketBecauseWrongSpreadingFactor.
     *
     * @param packet The packet lost.
     * @param node The receiver node id if any, 0 otherwise.
     */
    void WrongSf(Ptr<const Packet> packet, uint32_t node);

    /**
     * Compare two packets to check if they are equal.
     *
     * @param packet1 A first packet.
     * @param packet2 A second packet.
     * @return True if their unique identifiers are equal,
     * @return false otherwise.
     */
    bool IsSamePacket(Ptr<Packet> packet1, Ptr<Packet> packet2);

  private:
    void DoRun() override;

    Ptr<LoraChannel> channel;           //!< The LoRa channel used for tests
    Ptr<SimpleEndDeviceLoraPhy> edPhy1; //!< The first end device's PHY layer used in tests
    Ptr<SimpleEndDeviceLoraPhy> edPhy2; //!< The second end device's PHY layer used in tests
    Ptr<SimpleEndDeviceLoraPhy> edPhy3; //!< The third end device's PHY layer used in tests

    Ptr<Packet> m_latestReceivedPacket; //!< Pointer to track the last received packet
    int m_receivedPacketCalls = 0;      //!< Counter for ReceivedPacket calls
    int m_underSensitivityCalls = 0;    //!< Counter for LostPacketBecauseUnderSensitivity calls
    int m_interferenceCalls = 0;        //!< Counter for LostPacketBecauseInterference calls
    int m_wrongSfCalls = 0;             //!< Counter for LostPacketBecauseWrongSpreadingFactor calls
    int m_wrongFrequencyCalls = 0;      //!< Counter for LostPacketBecauseWrongFrequency calls
};

// Add some help text to this case to describe what it is intended to test
PhyConnectivityTest::PhyConnectivityTest()
    : TestCase("Verify that PhyConnectivity works as expected")
{
}

// Reminder that the test case should clean up after itself
PhyConnectivityTest::~PhyConnectivityTest()
{
}

void
PhyConnectivityTest::ReceivedPacket(Ptr<const Packet> packet, uint32_t node)
{
    NS_LOG_FUNCTION(packet << node);

    m_receivedPacketCalls++;

    m_latestReceivedPacket = packet->Copy();
}

void
PhyConnectivityTest::UnderSensitivity(Ptr<const Packet> packet, uint32_t node)
{
    NS_LOG_FUNCTION(packet << node);

    m_underSensitivityCalls++;
}

void
PhyConnectivityTest::Interference(Ptr<const Packet> packet, uint32_t node)
{
    NS_LOG_FUNCTION(packet << node);

    m_interferenceCalls++;
}

void
PhyConnectivityTest::WrongSf(Ptr<const Packet> packet, uint32_t node)
{
    NS_LOG_FUNCTION(packet << node);

    m_wrongSfCalls++;
}

void
PhyConnectivityTest::WrongFrequency(Ptr<const Packet> packet, uint32_t node)
{
    NS_LOG_FUNCTION(packet << node);

    m_wrongFrequencyCalls++;
}

bool
PhyConnectivityTest::IsSamePacket(Ptr<Packet> packet1, Ptr<Packet> packet2)
{
    return packet1->GetUid() == packet2->GetUid();
}

void
PhyConnectivityTest::Reset()
{
    m_receivedPacketCalls = 0;
    m_underSensitivityCalls = 0;
    m_interferenceCalls = 0;
    m_wrongSfCalls = 0;
    m_wrongFrequencyCalls = 0;

    Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel>();
    loss->SetPathLossExponent(3.76);
    loss->SetReference(1, 7.7);

    Ptr<PropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel>();

    // Create the channel
    channel = CreateObject<LoraChannel>(loss, delay);

    // Connect PHYs
    edPhy1 = CreateObject<SimpleEndDeviceLoraPhy>();
    edPhy2 = CreateObject<SimpleEndDeviceLoraPhy>();
    edPhy3 = CreateObject<SimpleEndDeviceLoraPhy>();

    Ptr<ConstantPositionMobilityModel> mob1 = CreateObject<ConstantPositionMobilityModel>();
    Ptr<ConstantPositionMobilityModel> mob2 = CreateObject<ConstantPositionMobilityModel>();
    Ptr<ConstantPositionMobilityModel> mob3 = CreateObject<ConstantPositionMobilityModel>();

    mob1->SetPosition(Vector(0.0, 0.0, 0.0));
    mob2->SetPosition(Vector(10.0, 0.0, 0.0));
    mob3->SetPosition(Vector(20.0, 0.0, 0.0));

    edPhy1->SetMobility(mob1);
    edPhy2->SetMobility(mob2);
    edPhy3->SetMobility(mob3);

    channel->Add(edPhy1);
    channel->Add(edPhy2);
    channel->Add(edPhy3);

    edPhy1->SetChannel(channel);
    edPhy2->SetChannel(channel);
    edPhy3->SetChannel(channel);

    edPhy1->TraceConnectWithoutContext("ReceivedPacket",
                                       MakeCallback(&PhyConnectivityTest::ReceivedPacket, this));
    edPhy2->TraceConnectWithoutContext("ReceivedPacket",
                                       MakeCallback(&PhyConnectivityTest::ReceivedPacket, this));
    edPhy3->TraceConnectWithoutContext("ReceivedPacket",
                                       MakeCallback(&PhyConnectivityTest::ReceivedPacket, this));

    edPhy1->TraceConnectWithoutContext("LostPacketBecauseUnderSensitivity",
                                       MakeCallback(&PhyConnectivityTest::UnderSensitivity, this));
    edPhy2->TraceConnectWithoutContext("LostPacketBecauseUnderSensitivity",
                                       MakeCallback(&PhyConnectivityTest::UnderSensitivity, this));
    edPhy3->TraceConnectWithoutContext("LostPacketBecauseUnderSensitivity",
                                       MakeCallback(&PhyConnectivityTest::UnderSensitivity, this));

    edPhy1->TraceConnectWithoutContext("LostPacketBecauseInterference",
                                       MakeCallback(&PhyConnectivityTest::Interference, this));
    edPhy2->TraceConnectWithoutContext("LostPacketBecauseInterference",
                                       MakeCallback(&PhyConnectivityTest::Interference, this));
    edPhy3->TraceConnectWithoutContext("LostPacketBecauseInterference",
                                       MakeCallback(&PhyConnectivityTest::Interference, this));

    edPhy1->TraceConnectWithoutContext("LostPacketBecauseWrongFrequency",
                                       MakeCallback(&PhyConnectivityTest::WrongFrequency, this));
    edPhy2->TraceConnectWithoutContext("LostPacketBecauseWrongFrequency",
                                       MakeCallback(&PhyConnectivityTest::WrongFrequency, this));
    edPhy3->TraceConnectWithoutContext("LostPacketBecauseWrongFrequency",
                                       MakeCallback(&PhyConnectivityTest::WrongFrequency, this));

    edPhy1->TraceConnectWithoutContext("LostPacketBecauseWrongSpreadingFactor",
                                       MakeCallback(&PhyConnectivityTest::WrongSf, this));
    edPhy2->TraceConnectWithoutContext("LostPacketBecauseWrongSpreadingFactor",
                                       MakeCallback(&PhyConnectivityTest::WrongSf, this));
    edPhy3->TraceConnectWithoutContext("LostPacketBecauseWrongSpreadingFactor",
                                       MakeCallback(&PhyConnectivityTest::WrongSf, this));
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
PhyConnectivityTest::DoRun()
{
    NS_LOG_DEBUG("PhyConnectivityTest");

    // Setup
    ////////

    uint8_t buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    Ptr<Packet> packet = Create<Packet>(buffer, 10);

    LoraTxParameters txParams;
    txParams.spreadingFactor = 12;
    txParams.bandwidthHz = 125'000;
    txParams.codingRate = CodingRate::CR_4_5;
    txParams.lowDataRateOptimize = true;
    txParams.preambleLenSymb = 8;
    txParams.implicitHeader = false;
    txParams.crcEnabled = true;

    Reset();

    // Testing
    //////////

    // Basic packet delivery test
    /////////////////////////////

    Simulator::Schedule(Seconds(2),
                        &EndDeviceLoraPhy::ReceiveSingle,
                        edPhy1,
                        868'100'000,
                        IQPolarity::UP,
                        txParams.spreadingFactor,
                        txParams.bandwidthHz,
                        8,
                        EndDeviceLoraPhy::RxTimeoutCallback());

    Simulator::Schedule(Seconds(2),
                        &EndDeviceLoraPhy::ReceiveSingle,
                        edPhy2,
                        868'100'000,
                        IQPolarity::UP,
                        txParams.spreadingFactor,
                        txParams.bandwidthHz,
                        8,
                        EndDeviceLoraPhy::RxTimeoutCallback());

    Simulator::Schedule(Seconds(2) + MilliSeconds(1),
                        &SimpleEndDeviceLoraPhy::Send,
                        edPhy3,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        txParams,
                        14);

    Simulator::Stop(Hours(2));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(
        m_receivedPacketCalls,
        2,
        "Channel skipped some PHYs when delivering a packet"); // All PHYs except the sender

    Reset();

    // Sleeping PHYs do not receive the packet

    edPhy1->Sleep();

    Simulator::Schedule(Seconds(2),
                        &EndDeviceLoraPhy::ReceiveSingle,
                        edPhy2,
                        868'100'000,
                        IQPolarity::UP,
                        txParams.spreadingFactor,
                        txParams.bandwidthHz,
                        8,
                        EndDeviceLoraPhy::RxTimeoutCallback());

    Simulator::Schedule(Seconds(2) + MilliSeconds(1),
                        &SimpleEndDeviceLoraPhy::Send,
                        edPhy3,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        txParams,
                        14);

    Simulator::Stop(Hours(2));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(
        m_receivedPacketCalls,
        1,
        "Packet was received by a PHY in SLEEP mode"); // All PHYs in Rx except the sender

    Reset();

    // Packet that arrives under sensitivity is received correctly if the spreading factor increases

    txParams.spreadingFactor = 7;

    DynamicCast<ConstantPositionMobilityModel>(edPhy1->GetMobility())
        ->SetPosition(Vector(2990, 0, 0));

    Simulator::Schedule(Seconds(2),
                        &EndDeviceLoraPhy::ReceiveSingle,
                        edPhy1,
                        868'100'000,
                        IQPolarity::UP,
                        txParams.spreadingFactor,
                        txParams.bandwidthHz,
                        8,
                        EndDeviceLoraPhy::RxTimeoutCallback());

    Simulator::Schedule(Seconds(2) + MilliSeconds(1),
                        &SimpleEndDeviceLoraPhy::Send,
                        edPhy2,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        txParams,
                        14);

    Simulator::Stop(Hours(2));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(
        m_underSensitivityCalls,
        1,
        "Packet that should have been lost because of low receive power was received");

    Reset();

    // Try again using a packet with higher spreading factor
    txParams.spreadingFactor = 8;

    DynamicCast<ConstantPositionMobilityModel>(edPhy1->GetMobility())
        ->SetPosition(Vector(2990, 0, 0));

    Simulator::Schedule(Seconds(2),
                        &EndDeviceLoraPhy::ReceiveSingle,
                        edPhy1,
                        868'100'000,
                        IQPolarity::UP,
                        txParams.spreadingFactor,
                        txParams.bandwidthHz,
                        8,
                        EndDeviceLoraPhy::RxTimeoutCallback());

    Simulator::Schedule(Seconds(2) + MilliSeconds(1),
                        &SimpleEndDeviceLoraPhy::Send,
                        edPhy2,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        txParams,
                        14);

    Simulator::Stop(Hours(2));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_underSensitivityCalls,
                          0,
                          "Packets that should have arrived above sensitivity were under it");

    Reset();

    // Packets can be destroyed by interference

    txParams.spreadingFactor = 12;

    Simulator::Schedule(Seconds(2),
                        &EndDeviceLoraPhy::ReceiveSingle,
                        edPhy2,
                        868'100'000,
                        IQPolarity::UP,
                        txParams.spreadingFactor,
                        txParams.bandwidthHz,
                        8,
                        EndDeviceLoraPhy::RxTimeoutCallback());

    Simulator::Schedule(Seconds(2) + MilliSeconds(1),
                        &SimpleEndDeviceLoraPhy::Send,
                        edPhy1,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        txParams,
                        14);

    Simulator::Schedule(Seconds(2) + MilliSeconds(1),
                        &SimpleEndDeviceLoraPhy::Send,
                        edPhy3,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        txParams,
                        14);

    Simulator::Stop(Hours(2));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_interferenceCalls,
                          1,
                          "Packets that should be destroyed by interference weren't");

    Reset();

    // Packets can be lost because the PHY is not listening on the right frequency

    Simulator::Schedule(Seconds(2),
                        &EndDeviceLoraPhy::ReceiveSingle,
                        edPhy1,
                        868'100'000,
                        IQPolarity::UP,
                        txParams.spreadingFactor,
                        txParams.bandwidthHz,
                        8,
                        EndDeviceLoraPhy::RxTimeoutCallback());

    Simulator::Schedule(Seconds(2) + MilliSeconds(1),
                        &SimpleEndDeviceLoraPhy::Send,
                        edPhy2,
                        packet,
                        868'300'000,
                        IQPolarity::UP,
                        txParams,
                        14);

    Simulator::Stop(Hours(2));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_wrongFrequencyCalls,
                          1,
                          "Packets were received even though PHY was on a different frequency");

    Reset();

    // Packets can be lost because the PHY is not listening for the right spreading factor

    txParams.spreadingFactor = 8; // Send with 8, listening for 12

    Simulator::Schedule(Seconds(2),
                        &EndDeviceLoraPhy::ReceiveSingle,
                        edPhy1,
                        868'100'000,
                        IQPolarity::UP,
                        12,
                        txParams.bandwidthHz,
                        8,
                        EndDeviceLoraPhy::RxTimeoutCallback());

    Simulator::Schedule(Seconds(2) + MilliSeconds(1),
                        &SimpleEndDeviceLoraPhy::Send,
                        edPhy2,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        txParams,
                        14);

    Simulator::Stop(Hours(2));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(
        m_wrongSfCalls,
        1,
        "Packets were received even though PHY was listening for a different spreading factor.");

    Reset();

    // Sending of packets
    /////////////////////

    // The very same packet arrives at the other PHY

    Simulator::Schedule(Seconds(2),
                        &EndDeviceLoraPhy::ReceiveSingle,
                        edPhy1,
                        868'100'000,
                        IQPolarity::UP,
                        txParams.spreadingFactor,
                        txParams.bandwidthHz,
                        8,
                        EndDeviceLoraPhy::RxTimeoutCallback());

    Simulator::Schedule(Seconds(2) + MilliSeconds(1),
                        &SimpleEndDeviceLoraPhy::Send,
                        edPhy2,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        txParams,
                        14);

    Simulator::Stop(Hours(2));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(IsSamePacket(packet, m_latestReceivedPacket),
                          true,
                          "Packet changed contents when going through the channel");

    Reset();

    // Correct state transitions
    ////////////////////////////

    // PHY switches to STANDBY after TX and RX

    Simulator::Schedule(Seconds(2),
                        &EndDeviceLoraPhy::ReceiveSingle,
                        edPhy1,
                        868'100'000,
                        IQPolarity::UP,
                        txParams.spreadingFactor,
                        txParams.bandwidthHz,
                        8,
                        EndDeviceLoraPhy::RxTimeoutCallback());

    Simulator::Schedule(Seconds(2) + MilliSeconds(1),
                        &SimpleEndDeviceLoraPhy::Send,
                        edPhy2,
                        packet,
                        868'100'000,
                        IQPolarity::UP,
                        txParams,
                        14);

    Simulator::Stop(Hours(2));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(edPhy1->GetState(),
                          SimpleEndDeviceLoraPhy::State::STANDBY,
                          "State didn't switch to STANDBY as expected");
    NS_TEST_EXPECT_MSG_EQ(edPhy2->GetState(),
                          SimpleEndDeviceLoraPhy::State::STANDBY,
                          "State didn't switch to STANDBY as expected");
}

/**
 * @ingroup lorawan
 *
 * It tests EndDeviceLoraPhy's continuous reception mode (RXCONTINUOUS), used to implement Class C
 * end device behavior.
 */
class ContinuousReceiveTest : public TestCase
{
  public:
    ContinuousReceiveTest();           //!< Default constructor
    ~ContinuousReceiveTest() override; //!< Destructor

  private:
    void DoRun() override;

    /**
     * Record the state of the input PHY at the time this is called.
     *
     * @param phy The PHY to record the state of.
     */
    void RecordState(Ptr<SimpleEndDeviceLoraPhy> phy);

    /**
     * Callback for tracing ReceivedPacket, used to count successful receptions.
     *
     * @param packet The packet received.
     * @param node The receiver node id if any, 0 otherwise.
     */
    void CountReceivedPacket(Ptr<const Packet> packet, uint32_t node);

    EndDeviceLoraPhy::State m_recordedState; //!< State recorded by the last RecordState() call
    int m_receivedPacketCalls = 0;           //!< Counter for CountReceivedPacket calls
};

// Add some help text to this case to describe what it is intended to test
ContinuousReceiveTest::ContinuousReceiveTest()
    : TestCase("Verify that EndDeviceLoraPhy's continuous reception mode works as expected")
{
}

// Reminder that the test case should clean up after itself
ContinuousReceiveTest::~ContinuousReceiveTest()
{
}

void
ContinuousReceiveTest::RecordState(Ptr<SimpleEndDeviceLoraPhy> phy)
{
    m_recordedState = phy->GetState();
}

void
ContinuousReceiveTest::CountReceivedPacket(Ptr<const Packet> packet, uint32_t node)
{
    NS_LOG_FUNCTION(packet << node);
    m_receivedPacketCalls++;
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
ContinuousReceiveTest::DoRun()
{
    NS_LOG_DEBUG("ContinuousReceiveTest");

    // Part 1: continuous reception never times out on its own
    ///////////////////////////////////////////////////////////

    Ptr<SimpleEndDeviceLoraPhy> phy = CreateObject<SimpleEndDeviceLoraPhy>();
    NS_TEST_ASSERT_MSG_EQ(phy->GetState(),
                          EndDeviceLoraPhy::State::STANDBY,
                          "A newly created PHY should start in STANDBY");

    phy->ReceiveContinuous(868'100'000, IQPolarity::DOWN, 7, 125'000);
    NS_TEST_ASSERT_MSG_EQ(phy->GetState(),
                          EndDeviceLoraPhy::State::RX_ENABLED,
                          "ReceiveContinuous should immediately switch the PHY to RX_ENABLED");

    // Nothing else is scheduled. The default symbNumTimeout (100 symbols) would, at SF7/125kHz,
    // cause a RXSINGLE-style reception to time out after roughly 100 ms: if continuous reception
    // erroneously armed the same timeout, the PHY would already be back in STANDBY well before
    // the 1 hour mark below.
    Simulator::Schedule(Hours(1), &ContinuousReceiveTest::RecordState, this, phy);
    Simulator::Stop(Hours(1) + Seconds(1));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_recordedState,
                          EndDeviceLoraPhy::State::RX_ENABLED,
                          "Continuous reception should not time out");

    // Part 2: listening automatically resumes (RX_ENABLED, not STANDBY) after a real reception,
    // and the PHY is able to receive a second packet without any manual re-arming in between.
    // (Regression note: single/ReceiveSingle mode behavior after a real reception is already
    // covered, unchanged, by PhyConnectivityTest above.)
    ////////////////////////////////////////////////////////////////////////////////////////////

    m_receivedPacketCalls = 0;

    Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel>();
    loss->SetPathLossExponent(3.76);
    loss->SetReference(1, 7.7);
    Ptr<PropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel>();
    Ptr<LoraChannel> channel = CreateObject<LoraChannel>(loss, delay);

    Ptr<SimpleEndDeviceLoraPhy> rxPhy = CreateObject<SimpleEndDeviceLoraPhy>();
    Ptr<SimpleEndDeviceLoraPhy> txPhy = CreateObject<SimpleEndDeviceLoraPhy>();

    Ptr<ConstantPositionMobilityModel> rxMob = CreateObject<ConstantPositionMobilityModel>();
    Ptr<ConstantPositionMobilityModel> txMob = CreateObject<ConstantPositionMobilityModel>();
    rxMob->SetPosition(Vector(0.0, 0.0, 0.0));
    txMob->SetPosition(Vector(10.0, 0.0, 0.0));
    rxPhy->SetMobility(rxMob);
    txPhy->SetMobility(txMob);

    channel->Add(rxPhy);
    channel->Add(txPhy);
    rxPhy->SetChannel(channel);
    txPhy->SetChannel(channel);

    rxPhy->TraceConnectWithoutContext(
        "ReceivedPacket",
        MakeCallback(&ContinuousReceiveTest::CountReceivedPacket, this));

    LoraTxParameters txParams;
    txParams.spreadingFactor = 7;
    txParams.bandwidthHz = 125'000;
    txParams.codingRate = CodingRate::CR_4_5;
    txParams.preambleLenSymb = 8;
    txParams.crcEnabled = true;

    uint8_t buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    rxPhy->ReceiveContinuous(868'100'000, IQPolarity::UP, 7, 125'000);

    // First transmission
    Simulator::Schedule(Seconds(1),
                        &SimpleEndDeviceLoraPhy::Send,
                        txPhy,
                        Create<Packet>(buffer, 10),
                        868'100'000,
                        IQPolarity::UP,
                        txParams,
                        14);

    // Check the state right after the first reception should have concluded
    Simulator::Schedule(Seconds(2), &ContinuousReceiveTest::RecordState, this, rxPhy);

    // Second transmission, to confirm listening resumed on its own (no manual re-arming)
    Simulator::Schedule(Seconds(3),
                        &SimpleEndDeviceLoraPhy::Send,
                        txPhy,
                        Create<Packet>(buffer, 10),
                        868'100'000,
                        IQPolarity::UP,
                        txParams,
                        14);

    Simulator::Stop(Seconds(5));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_recordedState,
                          EndDeviceLoraPhy::State::RX_ENABLED,
                          "PHY should return to RX_ENABLED (not STANDBY) after a continuous-mode "
                          "reception concludes");
    NS_TEST_EXPECT_MSG_EQ(m_receivedPacketCalls,
                          2,
                          "PHY should receive both packets without manual re-arming in between");

    // Part 3: StopReceiveContinuous()
    ///////////////////////////////////

    // 3a. Success path: from idle continuous listening (RX_ENABLED, no packet locked), reverts
    // to STANDBY.
    Ptr<SimpleEndDeviceLoraPhy> stopPhy = CreateObject<SimpleEndDeviceLoraPhy>();
    stopPhy->ReceiveContinuous(868'100'000, IQPolarity::DOWN, 7, 125'000);
    NS_TEST_ASSERT_MSG_EQ(stopPhy->GetState(),
                          EndDeviceLoraPhy::State::RX_ENABLED,
                          "Setup: PHY should be RX_ENABLED before testing StopReceiveContinuous");
    stopPhy->StopReceiveContinuous();
    NS_TEST_EXPECT_MSG_EQ(stopPhy->GetState(),
                          EndDeviceLoraPhy::State::STANDBY,
                          "StopReceiveContinuous should revert idle RX_ENABLED to STANDBY");

    // 3b. Rejected path: calling StopReceiveContinuous() while actively receiving a packet
    // (RX_ACTIVE) must be a no-op, since an ongoing reception cannot be aborted.
    m_receivedPacketCalls = 0;

    Ptr<LogDistancePropagationLossModel> loss2 = CreateObject<LogDistancePropagationLossModel>();
    loss2->SetPathLossExponent(3.76);
    loss2->SetReference(1, 7.7);
    Ptr<PropagationDelayModel> delay2 = CreateObject<ConstantSpeedPropagationDelayModel>();
    Ptr<LoraChannel> channel2 = CreateObject<LoraChannel>(loss2, delay2);

    Ptr<SimpleEndDeviceLoraPhy> rxPhy2 = CreateObject<SimpleEndDeviceLoraPhy>();
    Ptr<SimpleEndDeviceLoraPhy> txPhy2 = CreateObject<SimpleEndDeviceLoraPhy>();
    Ptr<ConstantPositionMobilityModel> rxMob2 = CreateObject<ConstantPositionMobilityModel>();
    Ptr<ConstantPositionMobilityModel> txMob2 = CreateObject<ConstantPositionMobilityModel>();
    rxMob2->SetPosition(Vector(0.0, 0.0, 0.0));
    txMob2->SetPosition(Vector(10.0, 0.0, 0.0));
    rxPhy2->SetMobility(rxMob2);
    txPhy2->SetMobility(txMob2);
    channel2->Add(rxPhy2);
    channel2->Add(txPhy2);
    rxPhy2->SetChannel(channel2);
    txPhy2->SetChannel(channel2);
    rxPhy2->TraceConnectWithoutContext(
        "ReceivedPacket",
        MakeCallback(&ContinuousReceiveTest::CountReceivedPacket, this));

    Time timeOnAir = LoraPhy::GetTimeOnAir(10, txParams);

    rxPhy2->ReceiveContinuous(868'100'000, IQPolarity::UP, 7, 125'000);
    Simulator::Schedule(Seconds(1),
                        &SimpleEndDeviceLoraPhy::Send,
                        txPhy2,
                        Create<Packet>(buffer, 10),
                        868'100'000,
                        IQPolarity::UP,
                        txParams,
                        14);
    // Attempt to stop continuous reception while the packet is still mid-flight (RX_ACTIVE):
    // must be rejected (no-op). Scheduled at the same simulated instant as the state recording
    // right below, and before it, so the recorded state reflects the outcome of this call.
    Simulator::Schedule(Seconds(1) + timeOnAir / 2,
                        &SimpleEndDeviceLoraPhy::StopReceiveContinuous,
                        rxPhy2);
    Simulator::Schedule(Seconds(1) + timeOnAir / 2,
                        &ContinuousReceiveTest::RecordState,
                        this,
                        rxPhy2);

    Simulator::Stop(Seconds(1) + timeOnAir + Seconds(1));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_recordedState,
                          EndDeviceLoraPhy::State::RX_ACTIVE,
                          "StopReceiveContinuous should be rejected (no-op) while RX_ACTIVE");
    NS_TEST_EXPECT_MSG_EQ(m_receivedPacketCalls,
                          1,
                          "The in-progress reception should complete normally despite the "
                          "rejected StopReceiveContinuous call");
}

/**
 * @ingroup lorawan
 *
 * It tests the ClassCEndDeviceLorawanMac class, the MAC layer implementation of Class C LoRaWAN
 * end devices.
 */
class ClassCTest : public TestCase
{
  public:
    ClassCTest();           //!< Default constructor
    ~ClassCTest() override; //!< Destructor

  private:
    void DoRun() override;

    /**
     * Create a ClassCEndDeviceLorawanMac configured with a minimal single-channel setup (mirrors
     * LorawanMacHelper's ALOHA region configuration, which isn't yet available for Class C
     * devices) and attach it to the input PHY.
     *
     * @param phy The PHY to attach the returned MAC to.
     * @return A ready-to-use MAC.
     */
    Ptr<ClassCEndDeviceLorawanMac> CreateConfiguredMac(Ptr<SimpleEndDeviceLoraPhy> phy);

    /**
     * Deliver a downlink carrying a single RxParamSetupReq MAC command directly to the input MAC
     * (as MacCommandTest::RunMacCommand() does for Class A), then trigger an uplink send and
     * return the RxParamSetupAns command it queued as a reply, if any.
     *
     * @param mac The MAC to exercise, addressed as the downlink's destination.
     * @param rx1DrOffset The RX1DROffset field of the RxParamSetupReq command.
     * @param rx2DataRate The RX2 data rate field of the RxParamSetupReq command.
     * @param frequencyHz The RX2 frequency [Hz] field of the RxParamSetupReq command.
     * @return The RxParamSetupAns command produced in reply, or nullptr if none was queued.
     */
    Ptr<RxParamSetupAns> RunRxParamSetupReq(Ptr<ClassCEndDeviceLorawanMac> mac,
                                            uint8_t rx1DrOffset,
                                            uint8_t rx2DataRate,
                                            double frequencyHz);

    /**
     * Callback for the "EndDeviceState" PHY trace source, used to detect a (possibly short-lived)
     * transition into RX_ENABLED regardless of exactly when/for how long it happens, and to check
     * PHY parameters (e.g., frequency) at the precise instant of the transition.
     *
     * @param oldState The state the PHY is transitioning from.
     * @param newState The state the PHY is transitioning to.
     */
    void OnRxEnabledTransition(EndDeviceLoraPhy::State oldState, EndDeviceLoraPhy::State newState);

    /**
     * Callback for the "EndDeviceState" PHY trace source of the gateway-side PHY's peer, used to
     * send a downlink the instant RX1 opens (rather than guessing a fixed simulated time), so
     * that it reliably lands within the ~8-symbol RX1 window regardless of TX duration.
     *
     * @param oldState The state the peer PHY is transitioning from.
     * @param newState The state the peer PHY is transitioning to.
     */
    void OnRx1OpenSendDownlink(EndDeviceLoraPhy::State oldState, EndDeviceLoraPhy::State newState);

    /**
     * Callback registered as the MAC's ReceiveCallback, used to detect a downlink reaching the
     * NetDevice layer.
     *
     * @param packet The received packet.
     */
    void OnDownlinkReceived(Ptr<Packet> packet);

    /**
     * Record the state of the input PHY at the time this is called.
     *
     * @param phy The PHY to record the state of.
     */
    void RecordState(Ptr<SimpleEndDeviceLoraPhy> phy);

    /**
     * Callback for the "EndDeviceState" PHY trace source, used to attempt to interrupt continuous
     * listening with a new uplink the instant the PHY locks onto an (unrelated) incoming
     * transmission (RX_ACTIVE), to verify that Send() correctly defers instead of corrupting PHY
     * state. Single-shot, guarded by m_interruptAttempted.
     *
     * @param oldState The state the PHY is transitioning from.
     * @param newState The state the PHY is transitioning to.
     */
    void OnRxActiveAttemptInterrupt(EndDeviceLoraPhy::State oldState,
                                    EndDeviceLoraPhy::State newState);

    /**
     * Callback for the "EndDeviceState" PHY trace source, used to detect that the deferred Send()
     * attempted by OnRxActiveAttemptInterrupt() eventually did transmit.
     *
     * @param oldState The state the PHY is transitioning from.
     * @param newState The state the PHY is transitioning to.
     */
    void OnTxAfterInterrupt(EndDeviceLoraPhy::State oldState, EndDeviceLoraPhy::State newState);

    EndDeviceLoraPhy::State m_recordedState; //!< State recorded by the last RecordState() call

    Ptr<SimpleEndDeviceLoraPhy> m_tracePhy; //!< PHY under test for OnRxEnabledTransition()
    bool m_sawRxEnabled;           //!< Whether a transition to RX_ENABLED was ever observed
    bool m_rxEnabledOnCorrectFreq; //!< Value of IsOnFrequency(868100000) at the last transition
    int m_rxEnabledCount;          //!< Number of transitions to RX_ENABLED observed
    bool m_lastRxEnabledOnRx2Freq; //!< Value of IsOnFrequency(869525000) at the last transition
                                   //!< to RX_ENABLED

    Ptr<SimpleEndDeviceLoraPhy> m_gwPhy;             //!< Sender PHY used by OnRx1OpenSendDownlink()
    Ptr<ClassCEndDeviceLorawanMac> m_edMacUnderTest; //!< Receiver MAC address target for the
                                                     //!< downlink sent by OnRx1OpenSendDownlink()
    bool m_downlinkSent;     //!< Guards OnRx1OpenSendDownlink() against sending more than once
    bool m_downlinkReceived; //!< Set by OnDownlinkReceived() when a downlink reaches the NetDevice

    bool m_interruptAttempted;  //!< Guards OnRxActiveAttemptInterrupt() against firing more than
                                //!< once; also tells OnTxAfterInterrupt() the attempt has happened
    bool m_sawTxAfterInterrupt; //!< Set by OnTxAfterInterrupt() once TX is eventually entered
};

// Add some help text to this case to describe what it is intended to test
ClassCTest::ClassCTest()
    : TestCase("Verify that ClassCEndDeviceLorawanMac works as expected")
{
}

// Reminder that the test case should clean up after itself
ClassCTest::~ClassCTest()
{
}

Ptr<ClassCEndDeviceLorawanMac>
ClassCTest::CreateConfiguredMac(Ptr<SimpleEndDeviceLoraPhy> phy)
{
    Ptr<ClassCEndDeviceLorawanMac> mac = CreateObject<ClassCEndDeviceLorawanMac>();

    // Single-channel setup, mirroring LorawanMacHelper::ApplyCommonAlohaConfigurations /
    // ConfigureForAlohaRegion (done by hand here to keep the
    // test independent of the helper).
    auto channelHelper = Create<LogicalLoraChannelHelper>(1);
    channelHelper->AddSubBand(Create<SubBand>(868000000, 868600000, 1, 14));
    Ptr<LogicalLoraChannel> lc1 = Create<LogicalLoraChannel>(868100000, 0, 5);
    channelHelper->SetChannel(0, lc1);
    mac->SetLogicalLoraChannelHelper(channelHelper);
    mac->SetSfForDataRate(std::vector<uint8_t>{12, 11, 10, 9, 8, 7});
    mac->SetBandwidthForDataRate(
        std::vector<uint32_t>{125000, 125000, 125000, 125000, 125000, 125000});
    mac->SetMaxMacPayloadForDataRate(std::vector<uint32_t>{59, 59, 59, 123, 230, 230});
    mac->SetTxDbmForTxPower(std::vector<double>{14, 12, 10, 8, 6, 4, 2, 0});
    LorawanMac::ReplyDataRateMatrix matrix = {{{{0, 0, 0, 0, 0, 0}},
                                               {{1, 0, 0, 0, 0, 0}},
                                               {{2, 1, 0, 0, 0, 0}},
                                               {{3, 2, 1, 0, 0, 0}},
                                               {{4, 3, 2, 1, 0, 0}},
                                               {{5, 4, 3, 2, 1, 0}},
                                               {{6, 5, 4, 3, 2, 1}},
                                               {{7, 6, 5, 4, 3, 2}}}};
    mac->SetReplyDataRateMatrix(matrix);
    mac->SetNPreambleSymbols(8);
    mac->SetSecondReceiveWindowDataRate(0);
    mac->SetSecondReceiveWindowFrequency(869525000);

    mac->SetPhy(phy);

    return mac;
}

Ptr<RxParamSetupAns>
ClassCTest::RunRxParamSetupReq(Ptr<ClassCEndDeviceLorawanMac> mac,
                               uint8_t rx1DrOffset,
                               uint8_t rx2DataRate,
                               double frequencyHz)
{
    Ptr<Packet> downlink = Create<Packet>(0);
    LoraFrameHeader fHdr;
    fHdr.SetAsDownlink();
    fHdr.SetAddress(mac->GetDeviceAddress());
    fHdr.AddCommand(Create<RxParamSetupReq>(rx1DrOffset, rx2DataRate, frequencyHz));
    downlink->AddHeader(fHdr);
    LorawanMacHeader mHdr;
    mHdr.SetFType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
    downlink->AddHeader(mHdr);
    DynamicCast<LorawanMac>(mac)->Receive(downlink);

    Ptr<Packet> uplink = Create<Packet>(0);
    mac->Send(uplink);
    LorawanMacHeader upMHdr;
    uplink->RemoveHeader(upMHdr);
    LoraFrameHeader upFHdr;
    upFHdr.SetAsUplink();
    uplink->RemoveHeader(upFHdr);
    auto cmds = upFHdr.GetCommands();
    return cmds.empty() ? nullptr : DynamicCast<RxParamSetupAns>(cmds.at(0));
}

void
ClassCTest::OnRxEnabledTransition(EndDeviceLoraPhy::State oldState,
                                  EndDeviceLoraPhy::State newState)
{
    if (newState == EndDeviceLoraPhy::State::RX_ENABLED)
    {
        m_sawRxEnabled = true;
        m_rxEnabledOnCorrectFreq = m_tracePhy->IsOnFrequency(868'100'000);
        m_rxEnabledCount++;
        m_lastRxEnabledOnRx2Freq = m_tracePhy->IsOnFrequency(869'525'000);
    }
}

void
ClassCTest::OnRx1OpenSendDownlink(EndDeviceLoraPhy::State oldState,
                                  EndDeviceLoraPhy::State newState)
{
    if (m_downlinkSent || newState != EndDeviceLoraPhy::State::RX_ENABLED ||
        !m_tracePhy->IsOnFrequency(868'100'000))
    {
        return;
    }
    m_downlinkSent = true;

    LoraFrameHeader fHdr;
    fHdr.SetAsDownlink();
    fHdr.SetAddress(m_edMacUnderTest->GetDeviceAddress());
    fHdr.SetAck(false);
    LorawanMacHeader mHdr;
    mHdr.SetFType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
    Ptr<Packet> pkt = Create<Packet>(5);
    pkt->AddHeader(fHdr);
    pkt->AddHeader(mHdr);

    // RX1 data rate for this test is matrix[5][0] = 5 -> SF7/125kHz (see CreateConfiguredMac)
    LoraTxParameters params;
    params.spreadingFactor = 7;
    params.bandwidthHz = 125'000;
    params.codingRate = CodingRate::CR_4_5;
    params.preambleLenSymb = 8;
    params.crcEnabled = true;

    m_gwPhy->Send(pkt, 868'100'000, IQPolarity::DOWN, params, 14);
}

void
ClassCTest::OnDownlinkReceived(Ptr<Packet> packet)
{
    m_downlinkReceived = true;
}

void
ClassCTest::RecordState(Ptr<SimpleEndDeviceLoraPhy> phy)
{
    m_recordedState = phy->GetState();
}

void
ClassCTest::OnRxActiveAttemptInterrupt(EndDeviceLoraPhy::State oldState,
                                       EndDeviceLoraPhy::State newState)
{
    if (m_interruptAttempted || newState != EndDeviceLoraPhy::State::RX_ACTIVE)
    {
        return;
    }
    m_interruptAttempted = true;
    // Defer both calls to fresh (zero-delay) events rather than calling them inline: TracedValue
    // (the "EndDeviceState" trace behind this very callback) invokes its callbacks *before*
    // committing the new value internally (see TracedValue<T>::Set()), so a live GetState() call
    // made synchronously from within this callback would still read the *previous* state. Once
    // this event finishes, the TracedValue commit has happened and GetState() is accurate again.
    Simulator::ScheduleNow(&ClassCEndDeviceLorawanMac::Send, m_edMacUnderTest, Create<Packet>(10));
    Simulator::ScheduleNow(&ClassCTest::RecordState, this, m_tracePhy);
}

void
ClassCTest::OnTxAfterInterrupt(EndDeviceLoraPhy::State oldState, EndDeviceLoraPhy::State newState)
{
    if (m_interruptAttempted && newState == EndDeviceLoraPhy::State::TX)
    {
        m_sawTxAfterInterrupt = true;
    }
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
ClassCTest::DoRun()
{
    NS_LOG_DEBUG("ClassCTest");

    // The class is registered with the TypeId system and instantiable
    Ptr<ClassCEndDeviceLorawanMac> mac = CreateObject<ClassCEndDeviceLorawanMac>();
    NS_TEST_ASSERT_MSG_EQ(mac->GetInstanceTypeId(),
                          ClassCEndDeviceLorawanMac::GetTypeId(),
                          "GetInstanceTypeId should match the class's own GetTypeId");
    NS_TEST_ASSERT_MSG_EQ(ClassCEndDeviceLorawanMac::GetTypeId().GetName(),
                          "ns3::ClassCEndDeviceLorawanMac",
                          "Unexpected TypeId name");
    NS_TEST_ASSERT_MSG_EQ(ClassCEndDeviceLorawanMac::GetTypeId().GetParent(),
                          EndDeviceLorawanMac::GetTypeId(),
                          "ClassCEndDeviceLorawanMac should derive from EndDeviceLorawanMac");

    // It is-a EndDeviceLorawanMac (and, transitively, a LorawanMac)
    NS_TEST_ASSERT_MSG_NE(DynamicCast<EndDeviceLorawanMac>(mac),
                          nullptr,
                          "ClassCEndDeviceLorawanMac should be DynamicCast-able to "
                          "EndDeviceLorawanMac");

    // GetFirstReceiveWindowDataRate() indexes m_replyDataRateMatrix[dataRate][rx1DrOffset];
    // rx1DrOffset defaults to 0, so with a known matrix this also exercises that default.
    LorawanMac::ReplyDataRateMatrix matrix = {{{{0, 0, 0, 0, 0, 0}},
                                               {{1, 0, 0, 0, 0, 0}},
                                               {{2, 1, 0, 0, 0, 0}},
                                               {{3, 2, 1, 0, 0, 0}},
                                               {{4, 3, 2, 1, 0, 0}},
                                               {{5, 4, 3, 2, 1, 0}},
                                               {{6, 5, 4, 3, 2, 1}},
                                               {{7, 6, 5, 4, 3, 2}}}};
    mac->SetReplyDataRateMatrix(matrix);
    mac->SetDataRate(3);
    NS_TEST_EXPECT_MSG_EQ(unsigned(mac->GetFirstReceiveWindowDataRate()),
                          unsigned(matrix.at(3).at(0)),
                          "GetFirstReceiveWindowDataRate should read matrix[dataRate][rx1DrOffset]"
                          "with a default RX1DROffset of 0");

    // Part 2: an uplink transmission schedules the opening of the first receive window (RX1) at
    // +m_receiveDelay1, on the frequency used for the uplink. RX1 is only open for a handful of
    // symbols (m_receiveWindowDurationInSymbols), so rather than sampling PHY state at a guessed
    // instant, a trace hooked on every state transition is used to reliably catch it regardless
    // of how short-lived it is.
    ///////////////////////////////////////////////////////////////////////////////////////////

    Ptr<SimpleEndDeviceLoraPhy> phy = CreateObject<SimpleEndDeviceLoraPhy>();
    phy->SetChannel(CreateObject<LoraChannel>());
    phy->SetMobility(CreateObject<ConstantPositionMobilityModel>());
    Ptr<ClassCEndDeviceLorawanMac> mac2 = CreateConfiguredMac(phy);
    mac2->SetDataRate(5); // SF7

    m_tracePhy = phy;
    m_sawRxEnabled = false;
    m_rxEnabledOnCorrectFreq = false;
    phy->TraceConnectWithoutContext("EndDeviceState",
                                    MakeCallback(&ClassCTest::OnRxEnabledTransition, this));

    mac2->Send(Create<Packet>(10));

    Simulator::Stop(Seconds(2));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_sawRxEnabled,
                          true,
                          "The first receive window should have opened (RX_ENABLED) at some "
                          "point after the uplink");
    NS_TEST_EXPECT_MSG_EQ(m_rxEnabledOnCorrectFreq,
                          true,
                          "The first receive window should listen on the uplink's frequency");

    // Part 3: without any downlink reply, RX1 and RX2 closing without reception falls through
    // to continuous listening on the RX2 parameters, instead of sleeping like Class A would.
    ///////////////////////////////////////////////////////////////////////////////////////////

    Ptr<SimpleEndDeviceLoraPhy> phy2 = CreateObject<SimpleEndDeviceLoraPhy>();
    phy2->SetChannel(CreateObject<LoraChannel>());
    phy2->SetMobility(CreateObject<ConstantPositionMobilityModel>());
    Ptr<ClassCEndDeviceLorawanMac> mac3 = CreateConfiguredMac(phy2);
    mac3->SetDataRate(5); // SF7

    m_tracePhy = phy2;
    m_rxEnabledCount = 0;
    m_lastRxEnabledOnRx2Freq = false;
    phy2->TraceConnectWithoutContext("EndDeviceState",
                                     MakeCallback(&ClassCTest::OnRxEnabledTransition, this));

    mac3->Send(Create<Packet>(10));

    // Run for a long time (nothing else is scheduled once continuous listening starts) to
    // confirm RX2 does not eventually time out and sleep the way RX1 did in Part 2.
    Simulator::Stop(Hours(1));
    Simulator::Run();
    Simulator::Destroy();

    // Three RX_ENABLED entries are expected: the (timed) RX1 window, the (timed) RX2 window --
    // still a normal RXSINGLE-style window right after this device's own uplink, same as Class A
    // -- and, once that closes without reception, the (continuous) RXCONTINUOUS listening that
    // follows it. The persistence check below is what actually distinguishes the third entry as
    // continuous rather than merely "another timed window".
    NS_TEST_EXPECT_MSG_EQ(m_rxEnabledCount,
                          3,
                          "Expected 3 RX_ENABLED entries: timed RX1, timed RX2, continuous RX2");
    NS_TEST_EXPECT_MSG_EQ(m_lastRxEnabledOnRx2Freq,
                          true,
                          "The last (continuous) listening period should be on the RX2 "
                          "frequency");
    NS_TEST_EXPECT_MSG_EQ(phy2->GetState(),
                          EndDeviceLoraPhy::State::RX_ENABLED,
                          "The device should still be continuously listening after a long "
                          "wait, not asleep, proving the last entry is continuous, not timed");

    // Part 4: a successful reception outside of any ack sequence (m_secondReceiveWindow already
    // expired, m_busy already false -- as is the case once continuous listening has started)
    // resumes continuous listening afterward. Invoked directly (as MacCommandTest does), which is
    // valid here since a freshly-configured MAC's PHY starts in STANDBY and its
    // m_secondReceiveWindow starts already-expired, exactly the precondition Receive() asserts.
    ///////////////////////////////////////////////////////////////////////////////////////////

    Ptr<SimpleEndDeviceLoraPhy> phy3 = CreateObject<SimpleEndDeviceLoraPhy>();
    phy3->SetChannel(CreateObject<LoraChannel>());
    phy3->SetMobility(CreateObject<ConstantPositionMobilityModel>());
    Ptr<ClassCEndDeviceLorawanMac> mac4 = CreateConfiguredMac(phy3);

    LoraFrameHeader fHdr1;
    fHdr1.SetAsDownlink();
    fHdr1.SetAddress(mac4->GetDeviceAddress());
    fHdr1.SetAck(false);
    LorawanMacHeader mHdr1;
    mHdr1.SetFType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
    Ptr<Packet> downlinkForUs = Create<Packet>(5);
    downlinkForUs->AddHeader(fHdr1);
    downlinkForUs->AddHeader(mHdr1);

    DynamicCast<LorawanMac>(mac4)->Receive(downlinkForUs);

    NS_TEST_EXPECT_MSG_EQ(phy3->GetState(),
                          EndDeviceLoraPhy::State::RX_ENABLED,
                          "A reception outside the ack sequence should resume continuous "
                          "listening, not leave the device asleep");
    NS_TEST_EXPECT_MSG_EQ(phy3->IsOnFrequency(869'525'000),
                          true,
                          "Continuous listening should resume on the RX2 frequency");

    // Part 5: a failed reception (address mismatch) outside of any ack sequence also resumes
    // continuous listening afterward, same as Part 4 but via the FailedReception() path.
    ///////////////////////////////////////////////////////////////////////////////////////////

    Ptr<SimpleEndDeviceLoraPhy> phy4 = CreateObject<SimpleEndDeviceLoraPhy>();
    phy4->SetChannel(CreateObject<LoraChannel>());
    phy4->SetMobility(CreateObject<ConstantPositionMobilityModel>());
    Ptr<ClassCEndDeviceLorawanMac> mac5 = CreateConfiguredMac(phy4);

    LoraFrameHeader fHdr2;
    fHdr2.SetAsDownlink();
    fHdr2.SetAddress(LoraDeviceAddress(mac5->GetDeviceAddress().Get() + 1)); // mismatched address
    fHdr2.SetAck(false);
    LorawanMacHeader mHdr2;
    mHdr2.SetFType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
    Ptr<Packet> downlinkForSomeoneElse = Create<Packet>(5);
    downlinkForSomeoneElse->AddHeader(fHdr2);
    downlinkForSomeoneElse->AddHeader(mHdr2);

    DynamicCast<LorawanMac>(mac5)->Receive(downlinkForSomeoneElse);

    NS_TEST_EXPECT_MSG_EQ(phy4->GetState(),
                          EndDeviceLoraPhy::State::RX_ENABLED,
                          "A failed reception outside the ack sequence should resume continuous "
                          "listening, not leave the device asleep");

    // Part 6: regression check -- a successful reception during RX1, as part of the device's own
    // ack sequence, is handled just like Class A (packet delivered, RX2 no longer needed), and
    // then (unlike Class A) resumes continuous listening instead of sleeping. Sent over a real
    // channel, timed off of the "EndDeviceState" trace (RX1 open) rather than a guessed instant,
    // since RX1 is only ~8 symbols wide (see Part 2's diary entry).
    ///////////////////////////////////////////////////////////////////////////////////////////

    Ptr<LogDistancePropagationLossModel> loss3 = CreateObject<LogDistancePropagationLossModel>();
    loss3->SetPathLossExponent(3.76);
    loss3->SetReference(1, 7.7);
    Ptr<PropagationDelayModel> delay3 = CreateObject<ConstantSpeedPropagationDelayModel>();
    Ptr<LoraChannel> channel3 = CreateObject<LoraChannel>(loss3, delay3);

    Ptr<SimpleEndDeviceLoraPhy> edPhy = CreateObject<SimpleEndDeviceLoraPhy>();
    Ptr<SimpleEndDeviceLoraPhy> gwPhy = CreateObject<SimpleEndDeviceLoraPhy>();
    Ptr<ConstantPositionMobilityModel> edMob = CreateObject<ConstantPositionMobilityModel>();
    Ptr<ConstantPositionMobilityModel> gwMob = CreateObject<ConstantPositionMobilityModel>();
    edMob->SetPosition(Vector(0.0, 0.0, 0.0));
    gwMob->SetPosition(Vector(10.0, 0.0, 0.0));
    edPhy->SetMobility(edMob);
    gwPhy->SetMobility(gwMob);
    channel3->Add(edPhy);
    channel3->Add(gwPhy);
    edPhy->SetChannel(channel3);
    gwPhy->SetChannel(channel3);

    Ptr<ClassCEndDeviceLorawanMac> mac6 = CreateConfiguredMac(edPhy);
    mac6->SetDataRate(5); // SF7

    m_tracePhy = edPhy;
    m_gwPhy = gwPhy;
    m_edMacUnderTest = mac6;
    m_downlinkSent = false;
    m_downlinkReceived = false;
    edPhy->TraceConnectWithoutContext("EndDeviceState",
                                      MakeCallback(&ClassCTest::OnRx1OpenSendDownlink, this));
    mac6->SetReceiveCallback(MakeCallback(&ClassCTest::OnDownlinkReceived, this));

    mac6->Send(Create<Packet>(10));

    Simulator::Stop(Seconds(5));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_downlinkSent, true, "Setup: the downlink should have been sent");
    NS_TEST_EXPECT_MSG_EQ(m_downlinkReceived,
                          true,
                          "The downlink sent during RX1 should have reached the NetDevice layer");
    NS_TEST_EXPECT_MSG_EQ(edPhy->GetState(),
                          EndDeviceLoraPhy::State::RX_ENABLED,
                          "After successfully receiving during RX1, the device should resume "
                          "continuous listening rather than sleeping (unlike Class A)");

    // Part 7: Send() while idly continuously listening interrupts it and transmits promptly.
    ////////////////////////////////////////////////////////////////////////////////////////

    Ptr<SimpleEndDeviceLoraPhy> phy5 = CreateObject<SimpleEndDeviceLoraPhy>();
    phy5->SetChannel(CreateObject<LoraChannel>());
    phy5->SetMobility(CreateObject<ConstantPositionMobilityModel>());
    Ptr<ClassCEndDeviceLorawanMac> mac7 = CreateConfiguredMac(phy5);
    mac7->SetDataRate(5); // SF7

    mac7->Send(Create<Packet>(10));
    // By t=3s the device has fallen through RX1(~1s)+RX2(~2s) into continuous listening with no
    // reply (nothing else transmits on this channel) -- see Part 3, which proves this holds
    // indefinitely once reached, so t=3s is a safe, already-settled anchor.
    Simulator::Schedule(Seconds(3), &ClassCEndDeviceLorawanMac::Send, mac7, Create<Packet>(10));
    Simulator::Schedule(Seconds(3) + MilliSeconds(1), &ClassCTest::RecordState, this, phy5);

    Simulator::Stop(Seconds(4));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_recordedState,
                          EndDeviceLoraPhy::State::TX,
                          "Send() should interrupt idle continuous listening and transmit "
                          "promptly, not be postponed");

    // Part 8: Send() while RX_ACTIVE (actively receiving an unrelated downlink) is deferred, not
    // allowed to corrupt PHY state; it eventually transmits once the reception concludes.
    ////////////////////////////////////////////////////////////////////////////////////////////

    Ptr<LogDistancePropagationLossModel> loss4 = CreateObject<LogDistancePropagationLossModel>();
    loss4->SetPathLossExponent(3.76);
    loss4->SetReference(1, 7.7);
    Ptr<PropagationDelayModel> delay4 = CreateObject<ConstantSpeedPropagationDelayModel>();
    Ptr<LoraChannel> channel4 = CreateObject<LoraChannel>(loss4, delay4);

    Ptr<SimpleEndDeviceLoraPhy> edPhy2 = CreateObject<SimpleEndDeviceLoraPhy>();
    Ptr<SimpleEndDeviceLoraPhy> gwPhy2 = CreateObject<SimpleEndDeviceLoraPhy>();
    Ptr<ConstantPositionMobilityModel> edMob2 = CreateObject<ConstantPositionMobilityModel>();
    Ptr<ConstantPositionMobilityModel> gwMob2 = CreateObject<ConstantPositionMobilityModel>();
    edMob2->SetPosition(Vector(0.0, 0.0, 0.0));
    gwMob2->SetPosition(Vector(10.0, 0.0, 0.0));
    edPhy2->SetMobility(edMob2);
    gwPhy2->SetMobility(gwMob2);
    channel4->Add(edPhy2);
    channel4->Add(gwPhy2);
    edPhy2->SetChannel(channel4);
    gwPhy2->SetChannel(channel4);

    Ptr<ClassCEndDeviceLorawanMac> mac8 = CreateConfiguredMac(edPhy2);
    mac8->SetDataRate(5); // SF7

    m_edMacUnderTest = mac8;
    m_tracePhy = edPhy2;
    m_interruptAttempted = false;
    m_sawTxAfterInterrupt = false;
    edPhy2->TraceConnectWithoutContext("EndDeviceState",
                                       MakeCallback(&ClassCTest::OnRxActiveAttemptInterrupt, this));
    edPhy2->TraceConnectWithoutContext("EndDeviceState",
                                       MakeCallback(&ClassCTest::OnTxAfterInterrupt, this));

    mac8->Send(Create<Packet>(10)); // original uplink, nothing replies -> continuous listening

    // At t=3s (same settled anchor as Part 7), a real, unrelated downlink arrives on the RX2
    // parameters mac8 is continuously listening on, triggering RX_ACTIVE and, via
    // OnRxActiveAttemptInterrupt, the deferred Send() attempt.
    LoraFrameHeader fHdr3;
    fHdr3.SetAsDownlink();
    fHdr3.SetAddress(mac8->GetDeviceAddress());
    fHdr3.SetAck(false);
    LorawanMacHeader mHdr3;
    mHdr3.SetFType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
    Ptr<Packet> unsolicitedDownlink = Create<Packet>(20);
    unsolicitedDownlink->AddHeader(fHdr3);
    unsolicitedDownlink->AddHeader(mHdr3);

    LoraTxParameters rx2Params;
    rx2Params.spreadingFactor = 12; // RX2 default data rate (0) -> SF12
    rx2Params.bandwidthHz = 125'000;
    rx2Params.codingRate = CodingRate::CR_4_5;
    rx2Params.preambleLenSymb = 8;
    rx2Params.crcEnabled = true;
    Simulator::Schedule(Seconds(3),
                        &SimpleEndDeviceLoraPhy::Send,
                        gwPhy2,
                        unsolicitedDownlink,
                        869'525'000,
                        IQPolarity::DOWN,
                        rx2Params,
                        14);

    // Give it: 3s anchor + SF12 demod time (a couple seconds at most) + the 5s retry backoff +
    // margin.
    Simulator::Stop(Seconds(15));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_EXPECT_MSG_EQ(m_interruptAttempted,
                          true,
                          "Setup: the interrupting Send() should have been attempted");
    NS_TEST_EXPECT_MSG_EQ(m_recordedState,
                          EndDeviceLoraPhy::State::RX_ACTIVE,
                          "Send() during RX_ACTIVE should defer, not corrupt PHY state by "
                          "transmitting immediately");
    NS_TEST_EXPECT_MSG_EQ(m_sawTxAfterInterrupt,
                          true,
                          "The deferred Send() should eventually transmit once the reception "
                          "concludes, not be lost");

    // Part 9: OnRxParamSetupReq() accepts a valid RX1/RX2 reconfiguration and rejects an invalid
    // one, mirroring Class A's equivalent behavior (MacCommandTest's RxParamSetupReq cases).
    ////////////////////////////////////////////////////////////////////////////////////////////

    {
        Ptr<SimpleEndDeviceLoraPhy> phy6 = CreateObject<SimpleEndDeviceLoraPhy>();
        phy6->SetChannel(CreateObject<LoraChannel>());
        phy6->SetMobility(CreateObject<ConstantPositionMobilityModel>());
        Ptr<ClassCEndDeviceLorawanMac> mac9 = CreateConfiguredMac(phy6);
        mac9->SetDataRate(5);

        uint8_t rx1DrOffset = 5;
        uint8_t rx2DataRate = 5;
        // Must fall within CreateConfiguredMac's single sub-band (868.0-868.6 MHz), unlike Class
        // A's equivalent test (which uses the full EU868 helper config, so it can use 863.5 MHz).
        double frequencyHz = 868'300'000;
        auto rpsa = RunRxParamSetupReq(mac9, rx1DrOffset, rx2DataRate, frequencyHz);

        NS_TEST_EXPECT_MSG_EQ(unsigned(mac9->GetFirstReceiveWindowDataRate()),
                              unsigned(5 - rx1DrOffset),
                              "Rx1DataRate does not match rx1DrOffset from RxParamSetupReq");
        NS_TEST_EXPECT_MSG_EQ(unsigned(mac9->GetSecondReceiveWindowDataRate()),
                              unsigned(rx2DataRate),
                              "Rx2DataRate does not match rx2DataRate from RxParamSetupReq");
        NS_TEST_EXPECT_MSG_EQ(mac9->GetSecondReceiveWindowFrequency(),
                              frequencyHz,
                              "Rx2 frequency does not match frequency from RxParamSetupReq");
        NS_TEST_ASSERT_MSG_NE(rpsa, nullptr, "RxParamSetupAns was expected, cmd type cast failed");
        NS_TEST_EXPECT_MSG_EQ(rpsa->GetRx1DrOffsetAck(), true, "Rx1DrOffsetAck != true");
        NS_TEST_EXPECT_MSG_EQ(rpsa->GetRx2DataRateAck(), true, "Rx2DataRateAck != true");
        NS_TEST_EXPECT_MSG_EQ(rpsa->GetChannelAck(), true, "ChannelAck expected to be true");

        // RunRxParamSetupReq()'s mac->Send() call schedules a TxFinished event on the real
        // ns-3 Simulator (bound to a raw `this`, not a ref-counted Ptr<>) that nothing here
        // ever runs or explicitly cancels -- left alone, it stays queued after mac9/phy6 go out
        // of scope and are destructed, then fires against dangling/reused memory whenever some
        // *later* TestCase calls Simulator::Run(). Destroy() discards it harmlessly instead (see
        // ClassBTest's identical Part 6 fix, added while root-causing this same bug).
        Simulator::Destroy();
    }

    { // WARNING: default values are manually set here
        Ptr<SimpleEndDeviceLoraPhy> phy7 = CreateObject<SimpleEndDeviceLoraPhy>();
        phy7->SetChannel(CreateObject<LoraChannel>());
        phy7->SetMobility(CreateObject<ConstantPositionMobilityModel>());
        Ptr<ClassCEndDeviceLorawanMac> mac10 = CreateConfiguredMac(phy7);
        mac10->SetDataRate(5);

        auto rpsa = RunRxParamSetupReq(mac10, 6, 12, 871'000'000);

        NS_TEST_EXPECT_MSG_EQ(unsigned(mac10->GetFirstReceiveWindowDataRate()),
                              5,
                              "Rx1DataRate expected to be default value");
        NS_TEST_EXPECT_MSG_EQ(unsigned(mac10->GetSecondReceiveWindowDataRate()),
                              0,
                              "Rx2DataRate expected to be default value");
        NS_TEST_EXPECT_MSG_EQ(mac10->GetSecondReceiveWindowFrequency(),
                              869'525'000,
                              "Rx2 frequency expected to be default value");
        NS_TEST_ASSERT_MSG_NE(rpsa, nullptr, "RxParamSetupAns was expected, cmd type cast failed");
        NS_TEST_EXPECT_MSG_EQ(rpsa->GetRx1DrOffsetAck(), false, "Rx1DrOffsetAck != false");
        NS_TEST_EXPECT_MSG_EQ(rpsa->GetRx2DataRateAck(), false, "Rx2DataRateAck != false");
        NS_TEST_EXPECT_MSG_EQ(rpsa->GetChannelAck(), false, "ChannelAck expected to be false");

        // Same reason as above: flush the queued TxFinished event before mac10/phy7 are
        // destructed at the end of this block.
        Simulator::Destroy();
    }

    // Part 10: LorawanMacHelper configured with ED_C produces a ClassCEndDeviceLorawanMac.
    ///////////////////////////////////////////////////////////////////////////////////////

    {
        LorawanMacHelper macHelper;
        macHelper.SetRegion(LorawanMacHelper::EU);
        macHelper.SetDeviceType(LorawanMacHelper::ED_C);
        /// @todo Install() should not require a node/device in input.
        Ptr<LorawanMac> mac = macHelper.Install(nullptr, nullptr);
        NS_TEST_ASSERT_MSG_NE(mac, nullptr, "Failed to install a MAC layer object.");
        NS_TEST_EXPECT_MSG_NE(DynamicCast<ClassCEndDeviceLorawanMac>(mac),
                              nullptr,
                              "LorawanMacHelper::Install() with ED_C should produce a "
                              "ClassCEndDeviceLorawanMac instance");
    }

    // Part 11: with an address generator set, a Class C device installed via the helper receives
    // a valid LoraDeviceAddress.
    ///////////////////////////////////////////////////////////////////////////////////////////

    {
        LorawanMacHelper macHelper;
        macHelper.SetRegion(LorawanMacHelper::EU);
        macHelper.SetDeviceType(LorawanMacHelper::ED_C);
        macHelper.SetAddressGenerator(CreateObject<LoraDeviceAddressGenerator>(0, 0));
        Ptr<ClassCEndDeviceLorawanMac> mac =
            DynamicCast<ClassCEndDeviceLorawanMac>(macHelper.Install(nullptr, nullptr));
        NS_TEST_ASSERT_MSG_NE(mac, nullptr, "Failed to install a ClassCEndDeviceLorawanMac.");
        NS_TEST_EXPECT_MSG_EQ(mac->GetDeviceAddress(),
                              LoraDeviceAddress(0, 0),
                              "Installed Class C device should have received the address "
                              "generator's first address");
    }

    // Part 12: region config (EU/SingleChannel/ALOHA) lands the expected RX2 parameters and
    // channel list on a Class C device installed via the helper.
    ///////////////////////////////////////////////////////////////////////////////////////////

    {
        LorawanMacHelper macHelper;
        macHelper.SetDeviceType(LorawanMacHelper::ED_C);

        // EU: 16 available channel slots, 3 populated with the default channels
        macHelper.SetRegion(LorawanMacHelper::EU);
        Ptr<ClassCEndDeviceLorawanMac> euMac =
            DynamicCast<ClassCEndDeviceLorawanMac>(macHelper.Install(nullptr, nullptr));
        NS_TEST_ASSERT_MSG_NE(euMac, nullptr, "Failed to install a ClassCEndDeviceLorawanMac.");
        NS_TEST_EXPECT_MSG_EQ(euMac->GetSecondReceiveWindowFrequency(),
                              869'525'000,
                              "Unexpected EU RX2 frequency");
        NS_TEST_EXPECT_MSG_EQ(unsigned(euMac->GetSecondReceiveWindowDataRate()),
                              0,
                              "Unexpected EU RX2 data rate");
        auto euChannels = euMac->GetLogicalLoraChannelHelper()->GetRawChannelArray();
        NS_TEST_EXPECT_MSG_EQ(euChannels.size(), 16, "Unexpected EU channel slot count");
        int euPopulated = 0;
        for (const auto& c : euChannels)
        {
            euPopulated += (c != nullptr);
        }
        NS_TEST_EXPECT_MSG_EQ(euPopulated, 3, "Unexpected number of EU default channels");
        NS_TEST_ASSERT_MSG_NE(euChannels.at(0), nullptr, "Missing EU default channel 0");
        NS_TEST_EXPECT_MSG_EQ(euChannels.at(0)->GetFrequency(),
                              868'100'000,
                              "Unexpected EU default channel 0 frequency");

        // SingleChannel: 1 available channel slot, populated with the single default channel
        macHelper.SetRegion(LorawanMacHelper::SingleChannel);
        Ptr<ClassCEndDeviceLorawanMac> scMac =
            DynamicCast<ClassCEndDeviceLorawanMac>(macHelper.Install(nullptr, nullptr));
        NS_TEST_ASSERT_MSG_NE(scMac, nullptr, "Failed to install a ClassCEndDeviceLorawanMac.");
        NS_TEST_EXPECT_MSG_EQ(scMac->GetSecondReceiveWindowFrequency(),
                              869'525'000,
                              "Unexpected SingleChannel RX2 frequency");
        NS_TEST_EXPECT_MSG_EQ(unsigned(scMac->GetSecondReceiveWindowDataRate()),
                              0,
                              "Unexpected SingleChannel RX2 data rate");
        auto scChannels = scMac->GetLogicalLoraChannelHelper()->GetRawChannelArray();
        NS_TEST_EXPECT_MSG_EQ(scChannels.size(), 1, "Unexpected SingleChannel channel slot count");
        NS_TEST_ASSERT_MSG_NE(scChannels.at(0), nullptr, "Missing SingleChannel default channel");
        NS_TEST_EXPECT_MSG_EQ(scChannels.at(0)->GetFrequency(),
                              868'100'000,
                              "Unexpected SingleChannel default channel frequency");

        // ALOHA: 1 available channel slot, populated with the single default channel
        macHelper.SetRegion(LorawanMacHelper::ALOHA);
        Ptr<ClassCEndDeviceLorawanMac> alohaMac =
            DynamicCast<ClassCEndDeviceLorawanMac>(macHelper.Install(nullptr, nullptr));
        NS_TEST_ASSERT_MSG_NE(alohaMac, nullptr, "Failed to install a ClassCEndDeviceLorawanMac.");
        NS_TEST_EXPECT_MSG_EQ(alohaMac->GetSecondReceiveWindowFrequency(),
                              869'525'000,
                              "Unexpected ALOHA RX2 frequency");
        NS_TEST_EXPECT_MSG_EQ(unsigned(alohaMac->GetSecondReceiveWindowDataRate()),
                              0,
                              "Unexpected ALOHA RX2 data rate");
        auto alohaChannels = alohaMac->GetLogicalLoraChannelHelper()->GetRawChannelArray();
        NS_TEST_EXPECT_MSG_EQ(alohaChannels.size(), 1, "Unexpected ALOHA channel slot count");
        NS_TEST_ASSERT_MSG_NE(alohaChannels.at(0), nullptr, "Missing ALOHA default channel");
        NS_TEST_EXPECT_MSG_EQ(alohaChannels.at(0)->GetFrequency(),
                              868'100'000,
                              "Unexpected ALOHA default channel frequency");
    }
}

/**
 * @ingroup lorawan
 *
 * It tests ClassBEndDeviceLorawanMac: the own-uplink RX1/RX2 sequence, beacon acquisition and
 * loss of synchronisation, ping-slot scheduling and reception, and TX/RX arbitration.
 */
class ClassBTest : public TestCase
{
  public:
    ClassBTest();           //!< Default constructor
    ~ClassBTest() override; //!< Destructor

  private:
    void DoRun() override;

    /**
     * Create a ClassBEndDeviceLorawanMac configured with a minimal single-channel setup (mirrors
     * LorawanMacHelper's ALOHA region configuration, done by hand to keep the test independent
     * of the helper) and attach it to the input PHY.
     *
     * @param phy The PHY to attach the returned MAC to.
     * @return A ready-to-use MAC.
     */
    Ptr<ClassBEndDeviceLorawanMac> CreateConfiguredMac(Ptr<SimpleEndDeviceLoraPhy> phy);

    /**
     * Deliver a downlink carrying a single RxParamSetupReq MAC command directly to the input MAC
     * (as MacCommandTest::RunMacCommand() does for Class A), then trigger an uplink send and
     * return the RxParamSetupAns command it queued as a reply, if any.
     *
     * @param mac The MAC to exercise, addressed as the downlink's destination.
     * @param rx1DrOffset The RX1DROffset field of the RxParamSetupReq command.
     * @param rx2DataRate The RX2 data rate field of the RxParamSetupReq command.
     * @param frequencyHz The RX2 frequency [Hz] field of the RxParamSetupReq command.
     * @return The RxParamSetupAns command produced in reply, or nullptr if none was queued.
     */
    Ptr<RxParamSetupAns> RunRxParamSetupReq(Ptr<ClassBEndDeviceLorawanMac> mac,
                                            uint8_t rx1DrOffset,
                                            uint8_t rx2DataRate,
                                            double frequencyHz);

    /**
     * Callback for the "EndDeviceState" PHY trace source, used to detect a (possibly short-lived)
     * transition into RX_ENABLED regardless of exactly when/for how long it happens, and to check
     * PHY parameters (e.g., frequency) at the precise instant of the transition.
     *
     * @param oldState The state the PHY is transitioning from.
     * @param newState The state the PHY is transitioning to.
     */
    void OnRxEnabledTransition(EndDeviceLoraPhy::State oldState, EndDeviceLoraPhy::State newState);

    /**
     * Callback for the "EndDeviceState" PHY trace source, same idea as OnRxEnabledTransition()
     * but checking the fixed beacon frequency instead of an uplink frequency, and also
     * recording the transition's timestamp.
     *
     * @param oldState The state the PHY is transitioning from.
     * @param newState The state the PHY is transitioning to.
     */
    void OnBeaconRxEnabledTransition(EndDeviceLoraPhy::State oldState,
                                     EndDeviceLoraPhy::State newState);

    /**
     * Build a minimal packet carrying just a LoraBeaconHeader and send it on the given PHY's
     * channel, at the fixed beacon frequency/data rate, right away (schedule this via
     * Simulator::Schedule() for a specific transmission time).
     *
     * @param sender The PHY to transmit from.
     * @param timeField The value to stamp the beacon's Time field with.
     */
    void SendBeaconPacket(Ptr<SimpleEndDeviceLoraPhy> sender, uint32_t timeField);

    /**
     * Record the input MAC's current IsSynchronized() value into m_syncCheckpoints at the given
     * index (schedule this via Simulator::Schedule() for a specific check time).
     *
     * @param mac The MAC to query.
     * @param index The m_syncCheckpoints index to record into.
     */
    void RecordSynchronized(Ptr<ClassBEndDeviceLorawanMac> mac, std::size_t index);

    /**
     * Callback for the "Synchronized" trace source, recording every time it fires along with
     * the simulation time it fired at.
     *
     * @param oldValue The value transitioned from.
     * @param newValue The value transitioned to.
     */
    void OnSynchronizedTrace(bool oldValue, bool newValue);

    /**
     * Callback for the "EndDeviceState" PHY trace source, recording the simulation time of
     * every transition into RX_ENABLED, regardless of frequency (used where a test needs to
     * count/space beacon *and* ping-slot windows together, since both use the same frequency).
     *
     * @param oldState The state the PHY is transitioning from.
     * @param newState The state the PHY is transitioning to.
     */
    void RecordRxEnabledTimestamp(EndDeviceLoraPhy::State oldState,
                                  EndDeviceLoraPhy::State newState);

    /**
     * Build a normal (MHDR+FHDR+payload) downlink packet addressed to dest and send it on the
     * given PHY's channel, at the fixed beacon/ping-slot frequency/data rate, right away
     * (schedule this via Simulator::Schedule() for a specific transmission time).
     *
     * @param sender The PHY to transmit from.
     * @param dest The destination device address to put in the frame header.
     */
    void SendDownlinkPacket(Ptr<SimpleEndDeviceLoraPhy> sender, LoraDeviceAddress dest);

    /**
     * Callback registered as the MAC's ReceiveCallback, used to detect a downlink reaching the
     * NetDevice layer.
     *
     * @param packet The received packet.
     */
    void OnDownlinkReceived(Ptr<Packet> packet);

    /**
     * Callback for the PHY's "StartSending" trace source, recording the simulation time of
     * every transmission the device itself starts.
     *
     * @param packet The packet that started being sent.
     * @param nodeId The node ID of the sending PHY (unused).
     */
    void OnEdStartSending(Ptr<const Packet> packet, uint32_t nodeId);

    /**
     * Callback for the "PingSlotSkipped" trace source, recording the simulation time of every
     * fire (the nominal time of the skipped slot).
     *
     * @param time The nominal time of the skipped ping slot.
     */
    void OnPingSlotSkipped(Time time);

    Ptr<SimpleEndDeviceLoraPhy> m_tracePhy; //!< PHY under test for OnRxEnabledTransition()
    bool m_sawRxEnabled;           //!< Whether a transition to RX_ENABLED was ever observed
    bool m_rxEnabledOnCorrectFreq; //!< Value of IsOnFrequency(868100000) at the last transition

    bool m_sawBeaconRxEnabled;           //!< Whether a beacon window was ever observed opening
    bool m_beaconRxEnabledOnCorrectFreq; //!< Value of IsOnFrequency(BEACON_FREQUENCY_HZ) at the
                                         //!< last beacon-window opening
    Time m_beaconRxEnabledTime;          //!< Simulation time of the last beacon-window opening

    std::array<bool, 3> m_syncCheckpoints; //!< IsSynchronized() values recorded by
                                           //!< RecordSynchronized(), one per checkpoint

    std::vector<std::pair<Time, bool>>
        m_synchronizedTraceFires; //!< (time, newValue) pairs recorded by OnSynchronizedTrace()

    std::vector<Time> m_rxEnabledTimestamps; //!< Timestamps recorded by RecordRxEnabledTimestamp()

    bool m_downlinkReceived; //!< Set by OnDownlinkReceived() when a downlink reaches the NetDevice

    std::vector<Time> m_edStartSendingTimes;  //!< Timestamps recorded by OnEdStartSending()
    std::vector<Time> m_pingSlotSkippedTimes; //!< Timestamps recorded by OnPingSlotSkipped()
};

// Add some help text to this case to describe what it is intended to test
ClassBTest::ClassBTest()
    : TestCase("Verify that ClassBEndDeviceLorawanMac works as expected")
{
}

// Reminder that the test case should clean up after itself
ClassBTest::~ClassBTest()
{
}

Ptr<ClassBEndDeviceLorawanMac>
ClassBTest::CreateConfiguredMac(Ptr<SimpleEndDeviceLoraPhy> phy)
{
    Ptr<ClassBEndDeviceLorawanMac> mac = CreateObject<ClassBEndDeviceLorawanMac>();

    // Single-channel setup, mirroring LorawanMacHelper::ApplyCommonAlohaConfigurations /
    // ConfigureForAlohaRegion (done by hand here to keep the
    // test independent of the helper).
    auto channelHelper = Create<LogicalLoraChannelHelper>(1);
    channelHelper->AddSubBand(Create<SubBand>(868000000, 868600000, 1, 14));
    Ptr<LogicalLoraChannel> lc1 = Create<LogicalLoraChannel>(868100000, 0, 5);
    channelHelper->SetChannel(0, lc1);
    mac->SetLogicalLoraChannelHelper(channelHelper);
    mac->SetSfForDataRate(std::vector<uint8_t>{12, 11, 10, 9, 8, 7});
    mac->SetBandwidthForDataRate(
        std::vector<uint32_t>{125000, 125000, 125000, 125000, 125000, 125000});
    mac->SetMaxMacPayloadForDataRate(std::vector<uint32_t>{59, 59, 59, 123, 230, 230});
    mac->SetTxDbmForTxPower(std::vector<double>{14, 12, 10, 8, 6, 4, 2, 0});
    LorawanMac::ReplyDataRateMatrix matrix = {{{{0, 0, 0, 0, 0, 0}},
                                               {{1, 0, 0, 0, 0, 0}},
                                               {{2, 1, 0, 0, 0, 0}},
                                               {{3, 2, 1, 0, 0, 0}},
                                               {{4, 3, 2, 1, 0, 0}},
                                               {{5, 4, 3, 2, 1, 0}},
                                               {{6, 5, 4, 3, 2, 1}},
                                               {{7, 6, 5, 4, 3, 2}}}};
    mac->SetReplyDataRateMatrix(matrix);
    mac->SetNPreambleSymbols(8);
    mac->SetSecondReceiveWindowDataRate(0);
    mac->SetSecondReceiveWindowFrequency(869525000);

    mac->SetPhy(phy);

    return mac;
}

Ptr<RxParamSetupAns>
ClassBTest::RunRxParamSetupReq(Ptr<ClassBEndDeviceLorawanMac> mac,
                               uint8_t rx1DrOffset,
                               uint8_t rx2DataRate,
                               double frequencyHz)
{
    Ptr<Packet> downlink = Create<Packet>(0);
    LoraFrameHeader fHdr;
    fHdr.SetAsDownlink();
    fHdr.SetAddress(mac->GetDeviceAddress());
    fHdr.AddCommand(Create<RxParamSetupReq>(rx1DrOffset, rx2DataRate, frequencyHz));
    downlink->AddHeader(fHdr);
    LorawanMacHeader mHdr;
    mHdr.SetFType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
    downlink->AddHeader(mHdr);
    DynamicCast<LorawanMac>(mac)->Receive(downlink);

    Ptr<Packet> uplink = Create<Packet>(0);
    mac->Send(uplink);
    LorawanMacHeader upMHdr;
    uplink->RemoveHeader(upMHdr);
    LoraFrameHeader upFHdr;
    upFHdr.SetAsUplink();
    uplink->RemoveHeader(upFHdr);
    auto cmds = upFHdr.GetCommands();
    return cmds.empty() ? nullptr : DynamicCast<RxParamSetupAns>(cmds.at(0));
}

void
ClassBTest::OnRxEnabledTransition(EndDeviceLoraPhy::State oldState,
                                  EndDeviceLoraPhy::State newState)
{
    if (newState == EndDeviceLoraPhy::State::RX_ENABLED)
    {
        m_sawRxEnabled = true;
        m_rxEnabledOnCorrectFreq = m_tracePhy->IsOnFrequency(868100000);
    }
}

void
ClassBTest::OnBeaconRxEnabledTransition(EndDeviceLoraPhy::State oldState,
                                        EndDeviceLoraPhy::State newState)
{
    if (newState == EndDeviceLoraPhy::State::RX_ENABLED)
    {
        m_sawBeaconRxEnabled = true;
        m_beaconRxEnabledOnCorrectFreq =
            m_tracePhy->IsOnFrequency(ClassBEndDeviceLorawanMac::BEACON_FREQUENCY_HZ);
        m_beaconRxEnabledTime = Simulator::Now();
    }
}

void
ClassBTest::SendBeaconPacket(Ptr<SimpleEndDeviceLoraPhy> sender, uint32_t timeField)
{
    LoraBeaconHeader beaconHdr;
    beaconHdr.SetTime(timeField);
    Ptr<Packet> packet = Create<Packet>(0);
    packet->AddHeader(beaconHdr);

    // SF9BW125 (DR3, ClassBEndDeviceLorawanMac::BEACON_DATA_RATE) under this test's
    // CreateConfiguredMac() data rate table ({12, 11, 10, 9, 8, 7} indexed by DR 0-5).
    LoraTxParameters params;
    params.spreadingFactor = 9;
    params.bandwidthHz = 125'000;
    params.codingRate = CodingRate::CR_4_5;
    params.preambleLenSymb = 8;
    params.crcEnabled = true;

    sender->Send(packet,
                 ClassBEndDeviceLorawanMac::BEACON_FREQUENCY_HZ,
                 IQPolarity::DOWN,
                 params,
                 14);
}

void
ClassBTest::RecordSynchronized(Ptr<ClassBEndDeviceLorawanMac> mac, std::size_t index)
{
    m_syncCheckpoints.at(index) = mac->IsSynchronized();
}

void
ClassBTest::OnSynchronizedTrace(bool oldValue, bool newValue)
{
    m_synchronizedTraceFires.emplace_back(Simulator::Now(), newValue);
}

void
ClassBTest::RecordRxEnabledTimestamp(EndDeviceLoraPhy::State oldState,
                                     EndDeviceLoraPhy::State newState)
{
    if (newState == EndDeviceLoraPhy::State::RX_ENABLED)
    {
        m_rxEnabledTimestamps.push_back(Simulator::Now());
    }
}

void
ClassBTest::SendDownlinkPacket(Ptr<SimpleEndDeviceLoraPhy> sender, LoraDeviceAddress dest)
{
    LoraFrameHeader fHdr;
    fHdr.SetAsDownlink();
    fHdr.SetAddress(dest);
    fHdr.SetAck(false);
    LorawanMacHeader mHdr;
    mHdr.SetFType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
    Ptr<Packet> packet = Create<Packet>(5);
    packet->AddHeader(fHdr);
    packet->AddHeader(mHdr);

    // SF9BW125 (DR3), matching the fixed beacon/ping-slot frequency and data rate.
    LoraTxParameters params;
    params.spreadingFactor = 9;
    params.bandwidthHz = 125'000;
    params.codingRate = CodingRate::CR_4_5;
    params.preambleLenSymb = 8;
    params.crcEnabled = true;

    sender->Send(packet,
                 ClassBEndDeviceLorawanMac::BEACON_FREQUENCY_HZ,
                 IQPolarity::DOWN,
                 params,
                 14);
}

void
ClassBTest::OnDownlinkReceived(Ptr<Packet> packet)
{
    m_downlinkReceived = true;
}

void
ClassBTest::OnEdStartSending(Ptr<const Packet> packet, uint32_t nodeId)
{
    m_edStartSendingTimes.push_back(Simulator::Now());
}

void
ClassBTest::OnPingSlotSkipped(Time time)
{
    m_pingSlotSkippedTimes.push_back(time);
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
ClassBTest::DoRun()
{
    NS_LOG_DEBUG("ClassBTest");

    // Part 1: the class is registered with the TypeId system and instantiable, derives from
    // EndDeviceLorawanMac, and exposes the PingSlotPeriodicity attribute with the expected
    // default
    {
        Ptr<ClassBEndDeviceLorawanMac> mac = CreateObject<ClassBEndDeviceLorawanMac>();
        NS_TEST_ASSERT_MSG_EQ(mac->GetInstanceTypeId(),
                              ClassBEndDeviceLorawanMac::GetTypeId(),
                              "GetInstanceTypeId should match the class's own GetTypeId");
        NS_TEST_ASSERT_MSG_EQ(ClassBEndDeviceLorawanMac::GetTypeId().GetName(),
                              "ns3::ClassBEndDeviceLorawanMac",
                              "Unexpected TypeId name");
        NS_TEST_ASSERT_MSG_EQ(ClassBEndDeviceLorawanMac::GetTypeId().GetParent(),
                              EndDeviceLorawanMac::GetTypeId(),
                              "ClassBEndDeviceLorawanMac should derive from EndDeviceLorawanMac");

        // It is-a EndDeviceLorawanMac (and, transitively, a LorawanMac)
        NS_TEST_ASSERT_MSG_NE(DynamicCast<EndDeviceLorawanMac>(mac),
                              nullptr,
                              "ClassBEndDeviceLorawanMac should be DynamicCast-able to "
                              "EndDeviceLorawanMac");

        // PingSlotPeriodicity attribute: default value, and settable via the attribute system
        UintegerValue periodicity;
        mac->GetAttribute("PingSlotPeriodicity", periodicity);
        NS_TEST_EXPECT_MSG_EQ(periodicity.Get(), 4, "Unexpected PingSlotPeriodicity default");

        mac->SetAttribute("PingSlotPeriodicity", UintegerValue(7));
        mac->GetAttribute("PingSlotPeriodicity", periodicity);
        NS_TEST_EXPECT_MSG_EQ(periodicity.Get(),
                              7,
                              "PingSlotPeriodicity was not correctly set via the attribute "
                              "system");

        // The constructor unconditionally schedules a beacon-window-open event
        // (StartBeaconSearch()) that nothing here ever runs or cancels; flush it before mac goes
        // out of scope, same reason as ClassCTest/MacCommandTest's identical fix.
        Simulator::Destroy();
    }

    // Part 2: an uplink transmission schedules the opening of the first receive window (RX1) at
    // +m_receiveDelay1, on the frequency used for the uplink. RX1 is only open for a handful of
    // symbols (m_receiveWindowDurationInSymbols), so rather than sampling PHY state at a guessed
    // instant, a trace hooked on every state transition is used to reliably catch it regardless
    // of how short-lived it is.
    ///////////////////////////////////////////////////////////////////////////////////////////
    {
        Ptr<SimpleEndDeviceLoraPhy> phy = CreateObject<SimpleEndDeviceLoraPhy>();
        phy->SetChannel(CreateObject<LoraChannel>());
        phy->SetMobility(CreateObject<ConstantPositionMobilityModel>());
        Ptr<ClassBEndDeviceLorawanMac> mac = CreateConfiguredMac(phy);
        mac->SetDataRate(5); // SF7

        m_tracePhy = phy;
        m_sawRxEnabled = false;
        m_rxEnabledOnCorrectFreq = false;
        phy->TraceConnectWithoutContext("EndDeviceState",
                                        MakeCallback(&ClassBTest::OnRxEnabledTransition, this));

        mac->Send(Create<Packet>(10));

        Simulator::Stop(Seconds(2));
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_EXPECT_MSG_EQ(m_sawRxEnabled,
                              true,
                              "The first receive window should have opened (RX_ENABLED) at some "
                              "point after the uplink");
        NS_TEST_EXPECT_MSG_EQ(m_rxEnabledOnCorrectFreq,
                              true,
                              "The first receive window should listen on the uplink's frequency");
    }

    // Part 3 (regression check): without any downlink reply, RX1 and RX2 close and the device
    // goes to sleep -- exactly like Class A, and unlike Class C's continuous listening -- since
    // the device has not acquired a beacon yet, so it has no ping slots to open.
    ///////////////////////////////////////////////////////////////////////////////////////////
    {
        Ptr<SimpleEndDeviceLoraPhy> phy2 = CreateObject<SimpleEndDeviceLoraPhy>();
        phy2->SetChannel(CreateObject<LoraChannel>());
        phy2->SetMobility(CreateObject<ConstantPositionMobilityModel>());
        Ptr<ClassBEndDeviceLorawanMac> mac2 = CreateConfiguredMac(phy2);
        mac2->SetDataRate(5); // SF7

        mac2->Send(Create<Packet>(10));

        Simulator::Stop(Seconds(5));
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_EXPECT_MSG_EQ(phy2->GetState(),
                              EndDeviceLoraPhy::State::SLEEP,
                              "With nothing received, the device should be asleep after RX1/RX2 "
                              "close, exactly like Class A (no beacon acquired yet, so no "
                              "ping slots)");
    }

    // Part 4: a successful reception (direct call, as MacCommandTest does for Class A), valid
    // since a freshly-configured MAC's PHY starts in STANDBY with m_secondReceiveWindow already
    // expired -- exactly the precondition Receive() asserts.
    ///////////////////////////////////////////////////////////////////////////////////////////
    {
        Ptr<SimpleEndDeviceLoraPhy> phy3 = CreateObject<SimpleEndDeviceLoraPhy>();
        phy3->SetChannel(CreateObject<LoraChannel>());
        phy3->SetMobility(CreateObject<ConstantPositionMobilityModel>());
        Ptr<ClassBEndDeviceLorawanMac> mac3 = CreateConfiguredMac(phy3);

        LoraFrameHeader fHdr1;
        fHdr1.SetAsDownlink();
        fHdr1.SetAddress(mac3->GetDeviceAddress());
        fHdr1.SetAck(false);
        LorawanMacHeader mHdr1;
        mHdr1.SetFType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
        Ptr<Packet> downlinkForUs = Create<Packet>(5);
        downlinkForUs->AddHeader(fHdr1);
        downlinkForUs->AddHeader(mHdr1);

        DynamicCast<LorawanMac>(mac3)->Receive(downlinkForUs);

        NS_TEST_EXPECT_MSG_EQ(phy3->GetState(),
                              EndDeviceLoraPhy::State::SLEEP,
                              "A successful reception should leave the device asleep, exactly "
                              "like Class A (unlike Class C, which would resume continuous "
                              "listening)");

        // Flush mac3's constructor-scheduled beacon-window event (see Part 1's comment).
        Simulator::Destroy();
    }

    // Part 5: a failed reception (address mismatch, direct call) also leaves the device asleep,
    // same as Part 4 but via the FailedReception() path.
    ///////////////////////////////////////////////////////////////////////////////////////////
    {
        Ptr<SimpleEndDeviceLoraPhy> phy4 = CreateObject<SimpleEndDeviceLoraPhy>();
        phy4->SetChannel(CreateObject<LoraChannel>());
        phy4->SetMobility(CreateObject<ConstantPositionMobilityModel>());
        Ptr<ClassBEndDeviceLorawanMac> mac4 = CreateConfiguredMac(phy4);

        LoraFrameHeader fHdr2;
        fHdr2.SetAsDownlink();
        fHdr2.SetAddress(
            LoraDeviceAddress(mac4->GetDeviceAddress().Get() + 1)); // mismatched address
        fHdr2.SetAck(false);
        LorawanMacHeader mHdr2;
        mHdr2.SetFType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
        Ptr<Packet> downlinkForSomeoneElse = Create<Packet>(5);
        downlinkForSomeoneElse->AddHeader(fHdr2);
        downlinkForSomeoneElse->AddHeader(mHdr2);

        DynamicCast<LorawanMac>(mac4)->Receive(downlinkForSomeoneElse);

        NS_TEST_EXPECT_MSG_EQ(phy4->GetState(),
                              EndDeviceLoraPhy::State::SLEEP,
                              "A failed reception should leave the device asleep, exactly like "
                              "Class A");

        // Flush mac4's constructor-scheduled beacon-window event (see Part 1's comment).
        Simulator::Destroy();
    }

    // Part 6: OnRxParamSetupReq() accepts a valid RX1/RX2 reconfiguration and rejects an invalid
    // one, mirroring Class A's equivalent behavior (MacCommandTest's RxParamSetupReq cases).
    ////////////////////////////////////////////////////////////////////////////////////////////
    {
        Ptr<SimpleEndDeviceLoraPhy> phy5 = CreateObject<SimpleEndDeviceLoraPhy>();
        phy5->SetChannel(CreateObject<LoraChannel>());
        phy5->SetMobility(CreateObject<ConstantPositionMobilityModel>());
        Ptr<ClassBEndDeviceLorawanMac> mac5 = CreateConfiguredMac(phy5);
        mac5->SetDataRate(5);

        uint8_t rx1DrOffset = 5;
        uint8_t rx2DataRate = 5;
        // Must fall within CreateConfiguredMac's single sub-band (868.0-868.6 MHz), unlike Class
        // A's equivalent test (which uses the full EU868 helper config, so it can use 863.5 MHz).
        double frequencyHz = 868'300'000;
        auto rpsa = RunRxParamSetupReq(mac5, rx1DrOffset, rx2DataRate, frequencyHz);

        NS_TEST_EXPECT_MSG_EQ(unsigned(mac5->GetFirstReceiveWindowDataRate()),
                              unsigned(5 - rx1DrOffset),
                              "Rx1DataRate does not match rx1DrOffset from RxParamSetupReq");
        NS_TEST_EXPECT_MSG_EQ(unsigned(mac5->GetSecondReceiveWindowDataRate()),
                              unsigned(rx2DataRate),
                              "Rx2DataRate does not match rx2DataRate from RxParamSetupReq");
        NS_TEST_EXPECT_MSG_EQ(mac5->GetSecondReceiveWindowFrequency(),
                              frequencyHz,
                              "Rx2 frequency does not match frequency from RxParamSetupReq");
        NS_TEST_ASSERT_MSG_NE(rpsa, nullptr, "RxParamSetupAns was expected, cmd type cast failed");
        NS_TEST_EXPECT_MSG_EQ(rpsa->GetRx1DrOffsetAck(), true, "Rx1DrOffsetAck != true");
        NS_TEST_EXPECT_MSG_EQ(rpsa->GetRx2DataRateAck(), true, "Rx2DataRateAck != true");
        NS_TEST_EXPECT_MSG_EQ(rpsa->GetChannelAck(), true, "ChannelAck expected to be true");

        // RunRxParamSetupReq()'s mac->Send() call schedules a TxFinished event on the real
        // ns-3 Simulator (bound to a raw `this`, not a ref-counted Ptr<>) that nothing here
        // ever runs or explicitly cancels -- left alone, it stays queued after mac5/phy5 go out
        // of scope and are destructed, then fires against dangling/reused memory whenever some
        // *later* TestCase calls Simulator::Run(). Destroy() discards it harmlessly instead.
        Simulator::Destroy();
    }

    { // WARNING: default values are manually set here
        Ptr<SimpleEndDeviceLoraPhy> phy6 = CreateObject<SimpleEndDeviceLoraPhy>();
        phy6->SetChannel(CreateObject<LoraChannel>());
        phy6->SetMobility(CreateObject<ConstantPositionMobilityModel>());
        Ptr<ClassBEndDeviceLorawanMac> mac6 = CreateConfiguredMac(phy6);
        mac6->SetDataRate(5);

        auto rpsa = RunRxParamSetupReq(mac6, 6, 12, 871'000'000);

        NS_TEST_EXPECT_MSG_EQ(unsigned(mac6->GetFirstReceiveWindowDataRate()),
                              5,
                              "Rx1DataRate expected to be default value");
        NS_TEST_EXPECT_MSG_EQ(unsigned(mac6->GetSecondReceiveWindowDataRate()),
                              0,
                              "Rx2DataRate expected to be default value");
        NS_TEST_EXPECT_MSG_EQ(mac6->GetSecondReceiveWindowFrequency(),
                              869'525'000,
                              "Rx2 frequency expected to be default value");
        NS_TEST_ASSERT_MSG_NE(rpsa, nullptr, "RxParamSetupAns was expected, cmd type cast failed");
        NS_TEST_EXPECT_MSG_EQ(rpsa->GetRx1DrOffsetAck(), false, "Rx1DrOffsetAck != false");
        NS_TEST_EXPECT_MSG_EQ(rpsa->GetRx2DataRateAck(), false, "Rx2DataRateAck != false");
        NS_TEST_EXPECT_MSG_EQ(rpsa->GetChannelAck(), false, "ChannelAck expected to be false");

        // Same reason as above: flush the queued TxFinished event before mac6/phy6 are
        // destructed at the end of this block.
        Simulator::Destroy();
    }

    // Part 7: a freshly-constructed device begins trying to acquire the beacon immediately, with
    // no uplink needed first -- the first beacon window opens at the first BEACON_INTERVAL
    // boundary from construction, on the fixed beacon frequency.
    ///////////////////////////////////////////////////////////////////////////////////////////
    {
        Ptr<SimpleEndDeviceLoraPhy> phy8 = CreateObject<SimpleEndDeviceLoraPhy>();
        phy8->SetChannel(CreateObject<LoraChannel>());
        phy8->SetMobility(CreateObject<ConstantPositionMobilityModel>());
        Ptr<ClassBEndDeviceLorawanMac> mac8 = CreateConfiguredMac(phy8);

        m_tracePhy = phy8;
        m_sawBeaconRxEnabled = false;
        m_beaconRxEnabledOnCorrectFreq = false;
        m_beaconRxEnabledTime = Time();
        phy8->TraceConnectWithoutContext(
            "EndDeviceState",
            MakeCallback(&ClassBTest::OnBeaconRxEnabledTransition, this));

        Simulator::Stop(ClassBEndDeviceLorawanMac::BEACON_INTERVAL + Seconds(1));
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_EXPECT_MSG_EQ(m_sawBeaconRxEnabled,
                              true,
                              "A beacon-acquisition window should have opened without any "
                              "uplink from the device");
        NS_TEST_EXPECT_MSG_EQ(m_beaconRxEnabledOnCorrectFreq,
                              true,
                              "The beacon window should listen on the fixed beacon frequency");
        NS_TEST_EXPECT_MSG_EQ(m_beaconRxEnabledTime,
                              ClassBEndDeviceLorawanMac::BEACON_INTERVAL,
                              "The first beacon window should open exactly at the first "
                              "BeaconInterval boundary");
    }

    // Part 8: beacon window outcomes over a real channel -- successful acquisition marks the
    // device synchronized; a single subsequent miss still leaves it synchronized; a second
    // consecutive miss marks it desynchronized. The device keeps retrying every BeaconInterval
    // regardless of (de)synchronization state.
    ///////////////////////////////////////////////////////////////////////////////////////////
    {
        Ptr<LogDistancePropagationLossModel> loss4 =
            CreateObject<LogDistancePropagationLossModel>();
        loss4->SetPathLossExponent(3.76);
        loss4->SetReference(1, 7.7);
        Ptr<PropagationDelayModel> delay4 = CreateObject<ConstantSpeedPropagationDelayModel>();
        Ptr<LoraChannel> channel4 = CreateObject<LoraChannel>(loss4, delay4);

        Ptr<SimpleEndDeviceLoraPhy> edPhy2 = CreateObject<SimpleEndDeviceLoraPhy>();
        Ptr<SimpleEndDeviceLoraPhy> senderPhy = CreateObject<SimpleEndDeviceLoraPhy>();
        Ptr<ConstantPositionMobilityModel> edMob2 = CreateObject<ConstantPositionMobilityModel>();
        Ptr<ConstantPositionMobilityModel> senderMob =
            CreateObject<ConstantPositionMobilityModel>();
        edMob2->SetPosition(Vector(0.0, 0.0, 0.0));
        senderMob->SetPosition(Vector(10.0, 0.0, 0.0));
        edPhy2->SetMobility(edMob2);
        senderPhy->SetMobility(senderMob);
        channel4->Add(edPhy2);
        channel4->Add(senderPhy);
        edPhy2->SetChannel(channel4);
        senderPhy->SetChannel(channel4);

        Ptr<ClassBEndDeviceLorawanMac> mac9 = CreateConfiguredMac(edPhy2);

        // Beacon windows open at t=128, 256, 384 (all exact BeaconInterval boundaries, whether
        // hit or missed -- see TimeUntilNextBeaconBoundary()'s doc comment). Hit the first one
        // (5ms into its ~33ms window, comfortably inside it), miss the next two on purpose by
        // simply not transmitting anything during them.
        Simulator::Schedule(Seconds(128) + MilliSeconds(5),
                            &ClassBTest::SendBeaconPacket,
                            this,
                            senderPhy,
                            128);

        // Check IsSynchronized() shortly after each window has had time to resolve (~33ms).
        Simulator::Schedule(Seconds(130), &ClassBTest::RecordSynchronized, this, mac9, 0);
        Simulator::Schedule(Seconds(258), &ClassBTest::RecordSynchronized, this, mac9, 1);
        Simulator::Schedule(Seconds(386), &ClassBTest::RecordSynchronized, this, mac9, 2);

        Simulator::Stop(Seconds(390));
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_EXPECT_MSG_EQ(m_syncCheckpoints.at(0),
                              true,
                              "Device should be synchronized after receiving a beacon");
        NS_TEST_EXPECT_MSG_EQ(m_syncCheckpoints.at(1),
                              true,
                              "A single missed beacon should not desynchronize the device");
        NS_TEST_EXPECT_MSG_EQ(m_syncCheckpoints.at(2),
                              false,
                              "Two consecutive missed beacons should desynchronize the device");
    }

    // Part 9: the "Synchronized" trace source fires exactly on synchronized/desynchronized
    // transitions, not on every beacon hit/miss -- same hit/miss/miss timeline as Part 8.
    ///////////////////////////////////////////////////////////////////////////////////////////
    {
        Ptr<LogDistancePropagationLossModel> loss5 =
            CreateObject<LogDistancePropagationLossModel>();
        loss5->SetPathLossExponent(3.76);
        loss5->SetReference(1, 7.7);
        Ptr<PropagationDelayModel> delay5 = CreateObject<ConstantSpeedPropagationDelayModel>();
        Ptr<LoraChannel> channel5 = CreateObject<LoraChannel>(loss5, delay5);

        Ptr<SimpleEndDeviceLoraPhy> edPhy3 = CreateObject<SimpleEndDeviceLoraPhy>();
        Ptr<SimpleEndDeviceLoraPhy> senderPhy2 = CreateObject<SimpleEndDeviceLoraPhy>();
        Ptr<ConstantPositionMobilityModel> edMob3 = CreateObject<ConstantPositionMobilityModel>();
        Ptr<ConstantPositionMobilityModel> senderMob2 =
            CreateObject<ConstantPositionMobilityModel>();
        edMob3->SetPosition(Vector(0.0, 0.0, 0.0));
        senderMob2->SetPosition(Vector(10.0, 0.0, 0.0));
        edPhy3->SetMobility(edMob3);
        senderPhy2->SetMobility(senderMob2);
        channel5->Add(edPhy3);
        channel5->Add(senderPhy2);
        edPhy3->SetChannel(channel5);
        senderPhy2->SetChannel(channel5);

        Ptr<ClassBEndDeviceLorawanMac> mac10 = CreateConfiguredMac(edPhy3);

        m_synchronizedTraceFires.clear();
        mac10->TraceConnectWithoutContext("Synchronized",
                                          MakeCallback(&ClassBTest::OnSynchronizedTrace, this));

        Simulator::Schedule(Seconds(128) + MilliSeconds(5),
                            &ClassBTest::SendBeaconPacket,
                            this,
                            senderPhy2,
                            128);

        Simulator::Stop(Seconds(390));
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_EXPECT_MSG_EQ(m_synchronizedTraceFires.size(),
                              2u,
                              "Expected exactly 2 Synchronized trace fires: sync on the first "
                              "hit, desync after the second consecutive miss -- not one per "
                              "beacon window");
        if (m_synchronizedTraceFires.size() == 2)
        {
            NS_TEST_EXPECT_MSG_EQ(m_synchronizedTraceFires.at(0).second,
                                  true,
                                  "First trace fire should be the synchronized transition");
            NS_TEST_EXPECT_MSG_EQ(
                (m_synchronizedTraceFires.at(0).first >= Seconds(128) &&
                 m_synchronizedTraceFires.at(0).first < Seconds(256)),
                true,
                "Synchronized transition should fire soon after the first beacon window");
            NS_TEST_EXPECT_MSG_EQ(m_synchronizedTraceFires.at(1).second,
                                  false,
                                  "Second trace fire should be the desynchronized transition");
            NS_TEST_EXPECT_MSG_EQ(
                (m_synchronizedTraceFires.at(1).first >= Seconds(384) &&
                 m_synchronizedTraceFires.at(1).first < Seconds(390)),
                true,
                "Desynchronized transition should fire soon after the second missed window");
        }
    }

    // Part 10: on successful beacon sync, exactly PingNbSlots(periodicity) ping-slot windows
    // open during the rest of that period, at the network-computable offset and spaced by
    // exactly PingPeriod() -- and once the device desynchronizes (2 consecutive missed
    // beacons), no further ping-slot windows are scheduled, even though the beacon window
    // itself keeps retrying regardless.
    ///////////////////////////////////////////////////////////////////////////////////////////
    {
        Ptr<LogDistancePropagationLossModel> loss6 =
            CreateObject<LogDistancePropagationLossModel>();
        loss6->SetPathLossExponent(3.76);
        loss6->SetReference(1, 7.7);
        Ptr<PropagationDelayModel> delay6 = CreateObject<ConstantSpeedPropagationDelayModel>();
        Ptr<LoraChannel> channel6 = CreateObject<LoraChannel>(loss6, delay6);

        Ptr<SimpleEndDeviceLoraPhy> edPhy4 = CreateObject<SimpleEndDeviceLoraPhy>();
        Ptr<SimpleEndDeviceLoraPhy> senderPhy3 = CreateObject<SimpleEndDeviceLoraPhy>();
        Ptr<ConstantPositionMobilityModel> edMob4 = CreateObject<ConstantPositionMobilityModel>();
        Ptr<ConstantPositionMobilityModel> senderMob3 =
            CreateObject<ConstantPositionMobilityModel>();
        edMob4->SetPosition(Vector(0.0, 0.0, 0.0));
        senderMob3->SetPosition(Vector(10.0, 0.0, 0.0));
        edPhy4->SetMobility(edMob4);
        senderPhy3->SetMobility(senderMob3);
        channel6->Add(edPhy4);
        channel6->Add(senderPhy3);
        edPhy4->SetChannel(channel6);
        senderPhy3->SetChannel(channel6);

        Ptr<ClassBEndDeviceLorawanMac> mac11 = CreateConfiguredMac(edPhy4);

        m_tracePhy = edPhy4;
        m_rxEnabledTimestamps.clear();
        edPhy4->TraceConnectWithoutContext(
            "EndDeviceState",
            MakeCallback(&ClassBTest::RecordRxEnabledTimestamp, this));

        // Hit at t=128; nothing sent at t=256 or t=384 -- two consecutive misses, desynchronizing
        // the device (same timeline as Parts 8-9).
        Simulator::Schedule(Seconds(128) + MilliSeconds(5),
                            &ClassBTest::SendBeaconPacket,
                            this,
                            senderPhy3,
                            128);

        Simulator::Stop(Seconds(520));
        Simulator::Run();
        Simulator::Destroy();

        const uint16_t pingNbSlots = ClassBEndDeviceLorawanMac::PingNbSlots(4); // module default
        NS_TEST_EXPECT_MSG_EQ(m_rxEnabledTimestamps.size(),
                              std::size_t(1 + pingNbSlots + 3),
                              "Unexpected total window count: 1 beacon + pingNbSlots ping slots "
                              "in the synchronized period, plus 3 more beacon-only retries (2 "
                              "misses and one more after desync, with no ping slots mixed in)");

        NS_TEST_EXPECT_MSG_EQ(m_rxEnabledTimestamps.at(0),
                              Seconds(128),
                              "First window should be the beacon itself");

        Time pingPeriod = ClassBEndDeviceLorawanMac::BEACON_INTERVAL / pingNbSlots;
        Time expectedOffset =
            ClassBEndDeviceLorawanMac::ComputePingOffset(mac11->GetDeviceAddress(),
                                                         128,
                                                         pingPeriod);
        NS_TEST_EXPECT_MSG_EQ(m_rxEnabledTimestamps.at(1),
                              Seconds(128) + expectedOffset,
                              "First ping slot should open at the network-computable offset "
                              "from the beacon");

        for (std::size_t i = 2; i <= pingNbSlots; ++i)
        {
            Time gap = m_rxEnabledTimestamps.at(i) - m_rxEnabledTimestamps.at(i - 1);
            NS_TEST_EXPECT_MSG_EQ(gap,
                                  pingPeriod,
                                  "Ping slots should be evenly spaced by PingPeriod()");
        }

        // Indices [1 + pingNbSlots, size) are beacon-only retries: t=256 (miss 1), t=384 (miss
        // 2 / desync), t=512 (still retrying despite desync) -- no ping slots interspersed,
        // confirming they stopped being scheduled once desynchronized.
        NS_TEST_EXPECT_MSG_EQ(m_rxEnabledTimestamps.at(1 + pingNbSlots),
                              Seconds(256),
                              "Unexpected beacon retry time (miss 1)");
        NS_TEST_EXPECT_MSG_EQ(m_rxEnabledTimestamps.at(2 + pingNbSlots),
                              Seconds(384),
                              "Unexpected beacon retry time (miss 2 / desync)");
        NS_TEST_EXPECT_MSG_EQ(m_rxEnabledTimestamps.at(3 + pingNbSlots),
                              Seconds(512),
                              "Unexpected beacon retry time (post-desync, still retrying)");
        NS_TEST_EXPECT_MSG_EQ(mac11->IsSynchronized(),
                              false,
                              "Device should still be desynchronized at the end");
    }

    // Part 11: a downlink delivered during a scheduled ping slot is parsed as a normal frame
    // and forwarded to the NetDevice, exactly like an RX1/RX2 reception would.
    ///////////////////////////////////////////////////////////////////////////////////////////
    {
        Ptr<LogDistancePropagationLossModel> loss7 =
            CreateObject<LogDistancePropagationLossModel>();
        loss7->SetPathLossExponent(3.76);
        loss7->SetReference(1, 7.7);
        Ptr<PropagationDelayModel> delay7 = CreateObject<ConstantSpeedPropagationDelayModel>();
        Ptr<LoraChannel> channel7 = CreateObject<LoraChannel>(loss7, delay7);

        Ptr<SimpleEndDeviceLoraPhy> edPhy5 = CreateObject<SimpleEndDeviceLoraPhy>();
        Ptr<SimpleEndDeviceLoraPhy> senderPhy4 = CreateObject<SimpleEndDeviceLoraPhy>();
        Ptr<ConstantPositionMobilityModel> edMob5 = CreateObject<ConstantPositionMobilityModel>();
        Ptr<ConstantPositionMobilityModel> senderMob4 =
            CreateObject<ConstantPositionMobilityModel>();
        edMob5->SetPosition(Vector(0.0, 0.0, 0.0));
        senderMob4->SetPosition(Vector(10.0, 0.0, 0.0));
        edPhy5->SetMobility(edMob5);
        senderPhy4->SetMobility(senderMob4);
        channel7->Add(edPhy5);
        channel7->Add(senderPhy4);
        edPhy5->SetChannel(channel7);
        senderPhy4->SetChannel(channel7);

        Ptr<ClassBEndDeviceLorawanMac> mac12 = CreateConfiguredMac(edPhy5);
        mac12->SetReceiveCallback(MakeCallback(&ClassBTest::OnDownlinkReceived, this));

        m_downlinkReceived = false;

        // First, sync the device to a beacon at t=128 (bootstraps the ping-slot schedule).
        Simulator::Schedule(Seconds(128) + MilliSeconds(5),
                            &ClassBTest::SendBeaconPacket,
                            this,
                            senderPhy4,
                            128);

        // Then deliver a real downlink during the period's first ping slot, whose time is
        // independently computed here the same way the production code does.
        Time pingPeriod =
            ClassBEndDeviceLorawanMac::BEACON_INTERVAL / ClassBEndDeviceLorawanMac::PingNbSlots(4);
        Time offset = ClassBEndDeviceLorawanMac::ComputePingOffset(mac12->GetDeviceAddress(),
                                                                   128,
                                                                   pingPeriod);
        Time firstPingSlotTime = Seconds(128) + offset;
        Simulator::Schedule(firstPingSlotTime + MilliSeconds(5),
                            &ClassBTest::SendDownlinkPacket,
                            this,
                            senderPhy4,
                            mac12->GetDeviceAddress());

        Simulator::Stop(firstPingSlotTime + Seconds(1));
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_EXPECT_MSG_EQ(m_downlinkReceived,
                              true,
                              "A downlink delivered during a scheduled ping slot should reach "
                              "the NetDevice, exactly like one delivered during RX1/RX2");
    }

    // Part 12: an app-layer Send() attempted while a ping-slot window is open (RX_ENABLED) is
    // deferred by (at least) 5s rather than corrupting PHY state, and eventually transmits
    // once the window has concluded.
    ///////////////////////////////////////////////////////////////////////////////////////////
    {
        Ptr<LogDistancePropagationLossModel> loss8 =
            CreateObject<LogDistancePropagationLossModel>();
        loss8->SetPathLossExponent(3.76);
        loss8->SetReference(1, 7.7);
        Ptr<PropagationDelayModel> delay8 = CreateObject<ConstantSpeedPropagationDelayModel>();
        Ptr<LoraChannel> channel8 = CreateObject<LoraChannel>(loss8, delay8);

        Ptr<SimpleEndDeviceLoraPhy> edPhy6 = CreateObject<SimpleEndDeviceLoraPhy>();
        Ptr<SimpleEndDeviceLoraPhy> senderPhy5 = CreateObject<SimpleEndDeviceLoraPhy>();
        Ptr<ConstantPositionMobilityModel> edMob6 = CreateObject<ConstantPositionMobilityModel>();
        Ptr<ConstantPositionMobilityModel> senderMob5 =
            CreateObject<ConstantPositionMobilityModel>();
        edMob6->SetPosition(Vector(0.0, 0.0, 0.0));
        senderMob5->SetPosition(Vector(10.0, 0.0, 0.0));
        edPhy6->SetMobility(edMob6);
        senderPhy5->SetMobility(senderMob5);
        channel8->Add(edPhy6);
        channel8->Add(senderPhy5);
        edPhy6->SetChannel(channel8);
        senderPhy5->SetChannel(channel8);

        Ptr<ClassBEndDeviceLorawanMac> mac13 = CreateConfiguredMac(edPhy6);
        mac13->SetDataRate(5); // SF7, so the eventual deferred TX is quick to observe

        m_edStartSendingTimes.clear();
        edPhy6->TraceConnectWithoutContext("StartSending",
                                           MakeCallback(&ClassBTest::OnEdStartSending, this));

        Simulator::Schedule(Seconds(128) + MilliSeconds(5),
                            &ClassBTest::SendBeaconPacket,
                            this,
                            senderPhy5,
                            128);

        Time pingPeriod8 =
            ClassBEndDeviceLorawanMac::BEACON_INTERVAL / ClassBEndDeviceLorawanMac::PingNbSlots(4);
        Time offset8 = ClassBEndDeviceLorawanMac::ComputePingOffset(mac13->GetDeviceAddress(),
                                                                    128,
                                                                    pingPeriod8);
        Time firstPingSlotTime8 = Seconds(128) + offset8;

        // Attempt to send 5ms into the ping-slot window (comfortably inside its ~33ms
        // duration), guaranteeing the PHY is RX_ENABLED at that instant.
        Time sendAttemptTime = firstPingSlotTime8 + MilliSeconds(5);
        Simulator::Schedule(sendAttemptTime,
                            &ClassBEndDeviceLorawanMac::Send,
                            mac13,
                            Create<Packet>(10));

        Simulator::Stop(sendAttemptTime + Seconds(7));
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_EXPECT_MSG_EQ(m_edStartSendingTimes.size(),
                              1u,
                              "The deferred Send() should eventually transmit exactly once, "
                              "with no PHY assertion failure along the way");
        if (!m_edStartSendingTimes.empty())
        {
            Time actualDelay = m_edStartSendingTimes.at(0) - sendAttemptTime;
            NS_TEST_EXPECT_MSG_EQ((actualDelay >= Seconds(5)),
                                  true,
                                  "The transmission should have been deferred by (at least) "
                                  "the 5s postponement");
        }
    }

    // Part 13: a ping slot's scheduled time arriving while the device is busy with its own
    // ack sequence is skipped (not corrupting PHY state), firing the PingSlotSkipped trace,
    // and the schedule continues normally afterward.
    ///////////////////////////////////////////////////////////////////////////////////////////
    {
        Ptr<LogDistancePropagationLossModel> loss9 =
            CreateObject<LogDistancePropagationLossModel>();
        loss9->SetPathLossExponent(3.76);
        loss9->SetReference(1, 7.7);
        Ptr<PropagationDelayModel> delay9 = CreateObject<ConstantSpeedPropagationDelayModel>();
        Ptr<LoraChannel> channel9 = CreateObject<LoraChannel>(loss9, delay9);

        Ptr<SimpleEndDeviceLoraPhy> edPhy7 = CreateObject<SimpleEndDeviceLoraPhy>();
        Ptr<SimpleEndDeviceLoraPhy> senderPhy6 = CreateObject<SimpleEndDeviceLoraPhy>();
        Ptr<ConstantPositionMobilityModel> edMob7 = CreateObject<ConstantPositionMobilityModel>();
        Ptr<ConstantPositionMobilityModel> senderMob6 =
            CreateObject<ConstantPositionMobilityModel>();
        edMob7->SetPosition(Vector(0.0, 0.0, 0.0));
        senderMob6->SetPosition(Vector(10.0, 0.0, 0.0));
        edPhy7->SetMobility(edMob7);
        senderPhy6->SetMobility(senderMob6);
        channel9->Add(edPhy7);
        channel9->Add(senderPhy6);
        edPhy7->SetChannel(channel9);
        senderPhy6->SetChannel(channel9);

        Ptr<ClassBEndDeviceLorawanMac> mac14 = CreateConfiguredMac(edPhy7);
        mac14->SetDataRate(5);

        m_pingSlotSkippedTimes.clear();
        mac14->TraceConnectWithoutContext("PingSlotSkipped",
                                          MakeCallback(&ClassBTest::OnPingSlotSkipped, this));

        Simulator::Schedule(Seconds(128) + MilliSeconds(5),
                            &ClassBTest::SendBeaconPacket,
                            this,
                            senderPhy6,
                            128);

        Time pingPeriod9 =
            ClassBEndDeviceLorawanMac::BEACON_INTERVAL / ClassBEndDeviceLorawanMac::PingNbSlots(4);
        Time offset9 = ClassBEndDeviceLorawanMac::ComputePingOffset(mac14->GetDeviceAddress(),
                                                                    128,
                                                                    pingPeriod9);
        Time firstPingSlotTime9 = Seconds(128) + offset9;

        // Trigger an uplink 1s before the ping slot is due; the resulting ack sequence (TX,
        // then up to RX2 close ~2s later) is still in progress when the ping slot's scheduled
        // time arrives.
        Simulator::Schedule(firstPingSlotTime9 - Seconds(1),
                            &ClassBEndDeviceLorawanMac::Send,
                            mac14,
                            Create<Packet>(10));

        Simulator::Stop(firstPingSlotTime9 + Seconds(3));
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_EXPECT_MSG_EQ(m_pingSlotSkippedTimes.size(),
                              1u,
                              "Exactly one ping slot should have been skipped (busy), with no "
                              "PHY assertion failure along the way");
        if (!m_pingSlotSkippedTimes.empty())
        {
            NS_TEST_EXPECT_MSG_EQ(m_pingSlotSkippedTimes.at(0),
                                  firstPingSlotTime9,
                                  "The skipped ping slot should fire at its originally-scheduled "
                                  "nominal time");
        }
    }

    // Part 14: a fresh device queues a PingSlotInfoReq (carrying its configured periodicity)
    // that rides along on its next outgoing uplink.
    ///////////////////////////////////////////////////////////////////////////////////////////
    {
        Ptr<SimpleEndDeviceLoraPhy> phy9 = CreateObject<SimpleEndDeviceLoraPhy>();
        phy9->SetChannel(CreateObject<LoraChannel>());
        phy9->SetMobility(CreateObject<ConstantPositionMobilityModel>());
        Ptr<ClassBEndDeviceLorawanMac> mac15 = CreateConfiguredMac(phy9);
        mac15->SetAttribute("PingSlotPeriodicity", UintegerValue(6));

        Ptr<Packet> uplink = Create<Packet>(0);
        mac15->Send(uplink);

        LorawanMacHeader upMHdr;
        uplink->RemoveHeader(upMHdr);
        LoraFrameHeader upFHdr;
        upFHdr.SetAsUplink();
        uplink->RemoveHeader(upFHdr);

        Ptr<PingSlotInfoReq> psir = upFHdr.GetMacCommand<PingSlotInfoReq>();
        NS_TEST_ASSERT_MSG_NE(psir,
                              nullptr,
                              "PingSlotInfoReq should be present in the device's first uplink");
        NS_TEST_EXPECT_MSG_EQ(unsigned(psir->GetPeriodicity()),
                              6,
                              "PingSlotInfoReq should carry the device's configured "
                              "periodicity");

        // Flush the constructor-scheduled beacon-window event (see Part 1's comment) and the
        // Send()-scheduled TxFinished event before mac15/phy9 are destructed.
        Simulator::Destroy();
    }

    // Part 15: LorawanMacHelper configured with ED_B produces a ClassBEndDeviceLorawanMac
    //.
    ///////////////////////////////////////////////////////////////////////////////////////
    {
        LorawanMacHelper macHelper;
        macHelper.SetRegion(LorawanMacHelper::EU);
        macHelper.SetDeviceType(LorawanMacHelper::ED_B);
        /// @todo Install() should not require a node/device in input.
        Ptr<LorawanMac> mac = macHelper.Install(nullptr, nullptr);
        NS_TEST_ASSERT_MSG_NE(mac, nullptr, "Failed to install a MAC layer object.");
        NS_TEST_EXPECT_MSG_NE(DynamicCast<ClassBEndDeviceLorawanMac>(mac),
                              nullptr,
                              "LorawanMacHelper::Install() with ED_B should produce a "
                              "ClassBEndDeviceLorawanMac instance");

        // A fresh ClassBEndDeviceLorawanMac's constructor schedules a beacon-window-open event
        // (StartBeaconSearch()); flush it since nothing here calls Simulator::Run().
        Simulator::Destroy();
    }

    // Part 16: with an address generator set, a Class B device installed via the helper
    // receives a valid LoraDeviceAddress.
    ///////////////////////////////////////////////////////////////////////////////////////////
    {
        LorawanMacHelper macHelper;
        macHelper.SetRegion(LorawanMacHelper::EU);
        macHelper.SetDeviceType(LorawanMacHelper::ED_B);
        macHelper.SetAddressGenerator(CreateObject<LoraDeviceAddressGenerator>(0, 0));
        Ptr<ClassBEndDeviceLorawanMac> mac16 =
            DynamicCast<ClassBEndDeviceLorawanMac>(macHelper.Install(nullptr, nullptr));
        NS_TEST_ASSERT_MSG_NE(mac16, nullptr, "Failed to install a ClassBEndDeviceLorawanMac.");
        NS_TEST_EXPECT_MSG_EQ(mac16->GetDeviceAddress(),
                              LoraDeviceAddress(0, 0),
                              "Installed Class B device should have received the address "
                              "generator's first address");

        Simulator::Destroy();
    }

    // Part 17: region config (EU/SingleChannel/ALOHA) lands the expected RX2 parameters and
    // channel list on a Class B device installed via the helper; the fixed beacon/ping-slot
    // frequency and data rate constants are unaffected by region, since this module doesn't
    // implement BeaconFreqReq/PingSlotChannelReq.
    ///////////////////////////////////////////////////////////////////////////////////////////
    {
        LorawanMacHelper macHelper;
        macHelper.SetDeviceType(LorawanMacHelper::ED_B);

        NS_TEST_EXPECT_MSG_EQ(ClassBEndDeviceLorawanMac::BEACON_FREQUENCY_HZ,
                              869'525'000,
                              "Unexpected beacon/ping-slot frequency constant");
        NS_TEST_EXPECT_MSG_EQ(unsigned(ClassBEndDeviceLorawanMac::BEACON_DATA_RATE),
                              3,
                              "Unexpected beacon/ping-slot data rate constant");

        // EU: 16 available channel slots, 3 populated with the default channels
        macHelper.SetRegion(LorawanMacHelper::EU);
        Ptr<ClassBEndDeviceLorawanMac> euMac =
            DynamicCast<ClassBEndDeviceLorawanMac>(macHelper.Install(nullptr, nullptr));
        NS_TEST_ASSERT_MSG_NE(euMac, nullptr, "Failed to install a ClassBEndDeviceLorawanMac.");
        NS_TEST_EXPECT_MSG_EQ(euMac->GetSecondReceiveWindowFrequency(),
                              869'525'000,
                              "Unexpected EU RX2 frequency");
        NS_TEST_EXPECT_MSG_EQ(unsigned(euMac->GetSecondReceiveWindowDataRate()),
                              0,
                              "Unexpected EU RX2 data rate");
        auto euChannels = euMac->GetLogicalLoraChannelHelper()->GetRawChannelArray();
        NS_TEST_EXPECT_MSG_EQ(euChannels.size(), 16, "Unexpected EU channel slot count");
        int euPopulated = 0;
        for (const auto& c : euChannels)
        {
            euPopulated += (c != nullptr);
        }
        NS_TEST_EXPECT_MSG_EQ(euPopulated, 3, "Unexpected number of EU default channels");
        NS_TEST_ASSERT_MSG_NE(euChannels.at(0), nullptr, "Missing EU default channel 0");
        NS_TEST_EXPECT_MSG_EQ(euChannels.at(0)->GetFrequency(),
                              868'100'000,
                              "Unexpected EU default channel 0 frequency");
        Simulator::Destroy();

        // SingleChannel: 1 available channel slot, populated with the single default channel
        macHelper.SetRegion(LorawanMacHelper::SingleChannel);
        Ptr<ClassBEndDeviceLorawanMac> scMac =
            DynamicCast<ClassBEndDeviceLorawanMac>(macHelper.Install(nullptr, nullptr));
        NS_TEST_ASSERT_MSG_NE(scMac, nullptr, "Failed to install a ClassBEndDeviceLorawanMac.");
        NS_TEST_EXPECT_MSG_EQ(scMac->GetSecondReceiveWindowFrequency(),
                              869'525'000,
                              "Unexpected SingleChannel RX2 frequency");
        NS_TEST_EXPECT_MSG_EQ(unsigned(scMac->GetSecondReceiveWindowDataRate()),
                              0,
                              "Unexpected SingleChannel RX2 data rate");
        auto scChannels = scMac->GetLogicalLoraChannelHelper()->GetRawChannelArray();
        NS_TEST_EXPECT_MSG_EQ(scChannels.size(), 1, "Unexpected SingleChannel channel slot count");
        NS_TEST_ASSERT_MSG_NE(scChannels.at(0), nullptr, "Missing SingleChannel default channel");
        NS_TEST_EXPECT_MSG_EQ(scChannels.at(0)->GetFrequency(),
                              868'100'000,
                              "Unexpected SingleChannel default channel frequency");
        Simulator::Destroy();

        // ALOHA: 1 available channel slot, populated with the single default channel
        macHelper.SetRegion(LorawanMacHelper::ALOHA);
        Ptr<ClassBEndDeviceLorawanMac> alohaMac =
            DynamicCast<ClassBEndDeviceLorawanMac>(macHelper.Install(nullptr, nullptr));
        NS_TEST_ASSERT_MSG_NE(alohaMac, nullptr, "Failed to install a ClassBEndDeviceLorawanMac.");
        NS_TEST_EXPECT_MSG_EQ(alohaMac->GetSecondReceiveWindowFrequency(),
                              869'525'000,
                              "Unexpected ALOHA RX2 frequency");
        NS_TEST_EXPECT_MSG_EQ(unsigned(alohaMac->GetSecondReceiveWindowDataRate()),
                              0,
                              "Unexpected ALOHA RX2 data rate");
        auto alohaChannels = alohaMac->GetLogicalLoraChannelHelper()->GetRawChannelArray();
        NS_TEST_EXPECT_MSG_EQ(alohaChannels.size(), 1, "Unexpected ALOHA channel slot count");
        NS_TEST_ASSERT_MSG_NE(alohaChannels.at(0), nullptr, "Missing ALOHA default channel");
        NS_TEST_EXPECT_MSG_EQ(alohaChannels.at(0)->GetFrequency(),
                              868'100'000,
                              "Unexpected ALOHA default channel frequency");
        Simulator::Destroy();
    }

    // Part 18: LorawanMacHelper::EnableBeaconing() flips BeaconEnabled on every gateway in the
    // container, and beaconing actually starts, not just the flag.
    ///////////////////////////////////////////////////////////////////////////////////////////
    {
        Ptr<LoraChannel> channel10 = CreateChannel();
        MobilityHelper mobility2;
        mobility2.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        NodeContainer gateways2 = CreateGateways(2, mobility2, channel10);

        Ptr<GatewayLorawanMac> gwMac1 = GetMacLayerFromNode<GatewayLorawanMac>(gateways2.Get(0));
        Ptr<GatewayLorawanMac> gwMac2 = GetMacLayerFromNode<GatewayLorawanMac>(gateways2.Get(1));
        NS_TEST_EXPECT_MSG_EQ(gwMac1->GetBeaconEnabled(),
                              false,
                              "Beaconing should be disabled by default");
        NS_TEST_EXPECT_MSG_EQ(gwMac2->GetBeaconEnabled(),
                              false,
                              "Beaconing should be disabled by default");

        LorawanMacHelper macHelper2;
        macHelper2.EnableBeaconing(gateways2);

        NS_TEST_EXPECT_MSG_EQ(gwMac1->GetBeaconEnabled(),
                              true,
                              "EnableBeaconing() should enable beaconing on every gateway");
        NS_TEST_EXPECT_MSG_EQ(gwMac2->GetBeaconEnabled(),
                              true,
                              "EnableBeaconing() should enable beaconing on every gateway");

        m_edStartSendingTimes.clear();
        gwMac1->GetPhy()->TraceConnectWithoutContext(
            "StartSending",
            MakeCallback(&ClassBTest::OnEdStartSending, this));
        gwMac2->GetPhy()->TraceConnectWithoutContext(
            "StartSending",
            MakeCallback(&ClassBTest::OnEdStartSending, this));

        Simulator::Stop(Seconds(1));
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_EXPECT_MSG_EQ(m_edStartSendingTimes.size(),
                              2u,
                              "Both gateways should have transmitted a beacon shortly after "
                              "EnableBeaconing(), confirming it actually started beaconing and "
                              "not just the attribute flag");
    }
}

/**
 * @ingroup lorawan
 *
 * It tests that GatewayLorawanMac periodically transmits Class B beacons when enabled, at
 * the configured BeaconInterval, and not otherwise (Class B)
 */
class BeaconTransmissionTest : public TestCase
{
  public:
    BeaconTransmissionTest();           //!< Default constructor
    ~BeaconTransmissionTest() override; //!< Destructor

  private:
    void DoRun() override;

    /**
     * Trace sink for the gateway PHY's "StartSending" trace source. Records the simulation
     * time of every transmitted packet that carries a LoraBeaconHeader.
     *
     * @param packet The packet that started being sent.
     * @param nodeId The node ID of the sending PHY (unused).
     */
    void OnStartSending(Ptr<const Packet> packet, uint32_t nodeId);

    std::vector<Time> m_beaconTimes; //!< Times at which a beacon transmission was observed
};

// Add some help text to this case to describe what it is intended to test
BeaconTransmissionTest::BeaconTransmissionTest()
    : TestCase("Verify that GatewayLorawanMac transmits Class B beacons periodically when "
               "enabled, and not otherwise")
{
}

// Reminder that the test case should clean up after itself
BeaconTransmissionTest::~BeaconTransmissionTest()
{
}

void
BeaconTransmissionTest::OnStartSending(Ptr<const Packet> packet, uint32_t nodeId)
{
    Ptr<Packet> copy = packet->Copy();
    LoraBeaconHeader beaconHdr;
    if (copy->PeekHeader(beaconHdr) == 4)
    {
        m_beaconTimes.push_back(Simulator::Now());
    }
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
BeaconTransmissionTest::DoRun()
{
    NS_LOG_DEBUG("BeaconTransmissionTest");

    Ptr<LoraChannel> channel = CreateChannel();
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    NodeContainer gateways = CreateGateways(1, mobility, channel);

    Ptr<GatewayLorawanMac> gwMac = GetMacLayerFromNode<GatewayLorawanMac>(gateways.Get(0));
    gwMac->GetPhy()->TraceConnectWithoutContext(
        "StartSending",
        MakeCallback(&BeaconTransmissionTest::OnStartSending, this));

    NS_TEST_EXPECT_MSG_EQ(gwMac->GetBeaconEnabled(),
                          false,
                          "Beaconing should be disabled by default");

    gwMac->SetAttribute("BeaconInterval", TimeValue(Seconds(10)));

    // Beaconing stays disabled for the first 20s (regression check: no beacon should be sent);
    // it's then enabled, and should produce a beacon right away and every 10s after that.
    Simulator::Schedule(Seconds(20), &GatewayLorawanMac::SetBeaconEnabled, gwMac, true);

    Simulator::Stop(Seconds(55));
    Simulator::Run();
    Simulator::Destroy();

    for (const auto& t : m_beaconTimes)
    {
        NS_TEST_ASSERT_MSG_EQ((t >= Seconds(20)),
                              true,
                              "A beacon was sent before beaconing was enabled");
    }

    // Beacons expected at t=20,30,40,50 (4 of them within the [20s, 55s) simulation window)
    NS_TEST_EXPECT_MSG_EQ(m_beaconTimes.size(), 4, "Unexpected number of beacon transmissions");
    for (std::size_t i = 0; i < m_beaconTimes.size(); ++i)
    {
        Time expected = Seconds(20 + 10 * i);
        NS_TEST_EXPECT_MSG_EQ(m_beaconTimes.at(i),
                              expected,
                              "Beacon " << i << " sent at unexpected time");
    }
}

/**
 * @ingroup lorawan
 *
 * It tests that a gateway's periodic beacon transmission attempts are safely skipped (not
 * transmitted, but also not crashing anything) while duty cycle blocks the beacon channel, and
 * that the periodic schedule keeps going regardless (Class B)
 */
class BeaconDutyCycleGuardTest : public TestCase
{
  public:
    BeaconDutyCycleGuardTest();           //!< Default constructor
    ~BeaconDutyCycleGuardTest() override; //!< Destructor

  private:
    void DoRun() override;
};

// Add some help text to this case to describe what it is intended to test
BeaconDutyCycleGuardTest::BeaconDutyCycleGuardTest()
    : TestCase("Verify that beacon transmissions are safely skipped, not crashed, when duty "
               "cycle blocks the beacon channel")
{
}

// Reminder that the test case should clean up after itself
BeaconDutyCycleGuardTest::~BeaconDutyCycleGuardTest()
{
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
BeaconDutyCycleGuardTest::DoRun()
{
    NS_LOG_DEBUG("BeaconDutyCycleGuardTest");

    Ptr<LoraChannel> channel = CreateChannel();
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    NodeContainer gateways = CreateGateways(1, mobility, channel);

    Ptr<GatewayLorawanMac> gwMac = GetMacLayerFromNode<GatewayLorawanMac>(gateways.Get(0));

    // Artificially book the beacon channel busy for far longer than this test's duration, to
    // simulate heavy channel use blocking every beacon attempt via duty cycle.
    gwMac->GetLogicalLoraChannelHelper()->AddEvent(Seconds(1000),
                                                   GatewayLorawanMac::BEACON_FREQUENCY_HZ);

    gwMac->SetAttribute("BeaconInterval", TimeValue(Seconds(5)));
    gwMac->SetBeaconEnabled(true);

    // If SendBeacon() didn't guard against Send()'s duty-cycle abort correctly, this would
    // either crash (fatal assertion) or stop rescheduling itself; neither happens here, which
    // is itself the meaningful check.
    Simulator::Stop(Seconds(30));
    Simulator::Run();
    Simulator::Destroy();
}

/**
 * @ingroup lorawan
 *
 * It tests the functionalities of the MAC layer of LoRaWAN devices
 *
 * @todo Not implemented yet.
 */
class LorawanMacTest : public TestCase
{
  public:
    LorawanMacTest();           //!< Default constructor
    ~LorawanMacTest() override; //!< Destructor

  private:
    void DoRun() override;
};

// Add some help text to this case to describe what it is intended to test
LorawanMacTest::LorawanMacTest()
    : TestCase("Verify that the MAC layer of end devices behaves as expected")
{
}

// Reminder that the test case should clean up after itself
LorawanMacTest::~LorawanMacTest()
{
}

// This method is the pure virtual method from class TestCase that every
// TestCase must implement
void
LorawanMacTest::DoRun()
{
    NS_LOG_DEBUG("LorawanMacTest");
}

/**
 * @ingroup lorawan
 *
 * It tests the functionalities of LoRaWAN MAC commands received by devices.
 *
 * This means testing that (i) settings in the downlink MAC commands are correctly applied/rejected
 * by the device, and that (ii) the correct answer (if expected) is produced by the device.
 */
class MacCommandTest : public TestCase
{
  public:
    MacCommandTest();           //!< Default constructor
    ~MacCommandTest() override; //!< Destructor

  private:
    /**
     * Have this class' MAC layer receive a downlink packet carrying the input MAC command. After,
     * trigger a new empty uplink packet send that can then be used to examine the MAC command
     * answers in the header.
     *
     * @note The internal m_mac->Send() call schedules a TxFinished event on the real ns-3
     * Simulator, bound to a raw (non-Ptr<>) PHY pointer; since nothing here ever calls
     * Simulator::Run(), that event is never processed, and would stay queued -- referencing a
     * since-destructed PHY once Reset() replaces m_mac/its PHY -- for whichever later TestCase
     * happens to next call Simulator::Run() for real, corrupting or crashing it. Reset() calls
     * Simulator::Destroy() at its start specifically to discard this before it can do that.
     *
     * @tparam T  \explicit The type of MAC command to create.
     * @tparam Ts \deduced Types of the constructor arguments.
     * @param  [in] args MAC command constructor arguments.
     * @return The list of MAC commands produced by the device as an answer.
     */
    template <typename T, typename... Ts>
    std::vector<Ptr<MacCommand>> RunMacCommand(Ts&&... args);

    /**
     * This function resets the state of the MAC layer used for tests. Use it before each call of
     * RunMacCommand. Otherwise, on consecutive calls the MAC layer will not send due to duty-cycle
     * limitations.
     *
     * Also discards any Simulator event left over from the previous cycle's RunMacCommand() (see
     * the note on RunMacCommand() below) via Simulator::Destroy(), mirroring the same defensive
     * pattern AdrBackoffTest::Reset()/RetransmissionTest::Reset() already use.
     */
    void Reset();

    void DoRun() override;

    Ptr<ClassAEndDeviceLorawanMac> m_mac; //!< The end device's MAC layer used in tests.
};

MacCommandTest::MacCommandTest()
    : TestCase("Test functionality of MAC commands when received by a device")
{
}

MacCommandTest::~MacCommandTest()
{
    m_mac = nullptr;
}

template <typename T, typename... Ts>
std::vector<Ptr<MacCommand>>
MacCommandTest::RunMacCommand(Ts&&... args)
{
    Ptr<Packet> pkt;
    LoraFrameHeader fhdr;
    LorawanMacHeader mhdr;
    // Prepare DL packet with input command
    pkt = Create<Packet>(0);
    fhdr.SetAsDownlink();
    auto cmd = Create<T>(args...);
    fhdr.AddCommand(cmd);
    pkt->AddHeader(fhdr);
    mhdr.SetFType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
    pkt->AddHeader(mhdr);
    // Trigger MAC layer reception
    DynamicCast<LorawanMac>(m_mac)->Receive(pkt);
    // Trigger MAC layer send
    pkt = Create<Packet>(0);
    m_mac->Send(pkt);
    // Retrieve uplink MAC commands
    pkt->RemoveHeader(mhdr);
    fhdr.SetAsUplink();
    pkt->RemoveHeader(fhdr);
    return fhdr.GetCommands();
}

void
MacCommandTest::Reset()
{
    // Discard any Simulator event orphaned by the previous cycle's RunMacCommand() (see its
    // doc comment) before replacing m_mac/its PHY below.
    Simulator::Destroy();
    // Reset MAC state
    LorawanMacHelper macHelper;
    macHelper.SetRegion(LorawanMacHelper::EU);
    macHelper.SetDeviceType(LorawanMacHelper::ED_A);
    /// @todo Create should not require a node in input.
    m_mac = DynamicCast<ClassAEndDeviceLorawanMac>(macHelper.Install(nullptr, nullptr));
    NS_TEST_EXPECT_MSG_NE(m_mac, nullptr, "Failed to initialize MAC layer object.");
    auto phy = CreateObject<SimpleEndDeviceLoraPhy>();
    phy->SetChannel(CreateObject<LoraChannel>());
    phy->SetMobility(CreateObject<ConstantPositionMobilityModel>());
    m_mac->SetPhy(phy);
}

void
MacCommandTest::DoRun()
{
    NS_LOG_DEBUG("MacCommandTest");

    Reset();
    // LinkCheckAns: get connectivity metrics of last uplink LinkCheckReq command
    {
        uint8_t margin = 20; // best reception margin [dB] from demodulation floor
        uint8_t gwCnt = 3;   // number of gateways that received last uplink
        auto answers = RunMacCommand<LinkCheckAns>(margin, gwCnt);
        NS_TEST_EXPECT_MSG_EQ(unsigned(m_mac->GetLastKnownLinkMarginDb()),
                              unsigned(margin),
                              "m_lastKnownMarginDb differs from Margin field of LinkCheckAns");
        NS_TEST_EXPECT_MSG_EQ(unsigned(m_mac->GetLastKnownGatewayCount()),
                              unsigned(gwCnt),
                              "m_lastKnownGatewayCount differs GwCnt field of LinkCheckAns");
        NS_TEST_EXPECT_MSG_EQ(answers.size(),
                              0,
                              "Unexpected uplink MAC command answer(s) to LinkCheckAns");
    }

    Reset();
    // LinkAdrReq: change data rate, TX power, redundancy, or channel mask
    {
        uint8_t dataRate = 5;
        uint8_t txPower = 2;
        uint16_t chMask = 0b101;
        uint8_t chMaskCntl = 0;
        uint8_t nbTrans = 13;
        auto answers = RunMacCommand<LinkAdrReq>(dataRate, txPower, chMask, chMaskCntl, nbTrans);
        NS_TEST_EXPECT_MSG_EQ(unsigned(m_mac->GetDataRate()),
                              unsigned(dataRate),
                              "m_dataRate does not match DataRate field of LinkAdrReq");
        NS_TEST_EXPECT_MSG_EQ(m_mac->GetTransmissionPowerDbm(),
                              14 - txPower * 2,
                              "m_txPowerDbm does not match txPower field of LinkAdrReq");
        NS_TEST_EXPECT_MSG_EQ(unsigned(m_mac->GetMaxNumberOfTransmissions()),
                              unsigned(nbTrans),
                              "m_nbTrans does not match nbTrans field of LinkAdrReq");
        auto channels = m_mac->GetLogicalLoraChannelHelper()->GetRawChannelArray();
        for (size_t i = 0; i < channels.size(); i++)
        {
            const auto& c = channels.at(i + 16 * chMaskCntl);
            bool actual = (c) ? c->IsEnabledForUplink() : false;
            bool expected = (chMask & 0b1 << i);
            NS_TEST_EXPECT_MSG_EQ(actual, expected, "Channel " << i << " state != chMask");
        }
        NS_TEST_ASSERT_MSG_EQ(answers.size(), 1, "1 answer cmd was expected, found 0 or >1");
        auto laa = DynamicCast<LinkAdrAns>(answers.at(0));
        NS_TEST_ASSERT_MSG_NE(laa, nullptr, "LinkAdrAns was expected, cmd type cast failed");
        NS_TEST_EXPECT_MSG_EQ(laa->GetChannelMaskAck(), true, "ChannelMaskAck expected to be true");
        NS_TEST_EXPECT_MSG_EQ(laa->GetDataRateAck(), true, "DataRateAck expected to be true");
        NS_TEST_EXPECT_MSG_EQ(laa->GetPowerAck(), true, "PowerAck expected to be true");
    }

    Reset();
    // LinkAdrReq: ADR bit off, only change channel mask
    {
        uint8_t dataRate = 5;
        uint8_t txPower = 2;
        uint16_t chMask = 0b010;
        uint8_t chMaskCntl = 0;
        uint8_t nbTrans = 13;
        m_mac->SetUplinkAdrBit(false);
        auto answers = RunMacCommand<LinkAdrReq>(dataRate, txPower, chMask, chMaskCntl, nbTrans);
        NS_TEST_EXPECT_MSG_NE(unsigned(m_mac->GetDataRate()),
                              unsigned(dataRate),
                              "m_dataRate expected to differ from DataRate field of LinkAdrReq");
        NS_TEST_EXPECT_MSG_NE(m_mac->GetTransmissionPowerDbm(),
                              14 - txPower * 2,
                              "m_txPowerDbm expected to not match txPower field of LinkAdrReq");
        NS_TEST_EXPECT_MSG_NE(unsigned(m_mac->GetMaxNumberOfTransmissions()),
                              unsigned(nbTrans),
                              "m_nbTrans expected to differ from nbTrans field of LinkAdrReq");
        auto channels = m_mac->GetLogicalLoraChannelHelper()->GetRawChannelArray();
        for (size_t i = 0; i < channels.size(); i++)
        {
            const auto& c = channels.at(i + 16 * chMaskCntl);
            bool actual = (c) ? c->IsEnabledForUplink() : false;
            bool expected = (chMask & 0b1 << i);
            NS_TEST_EXPECT_MSG_EQ(actual, expected, "Channel " << i << " state != chMask");
        }
        NS_TEST_ASSERT_MSG_EQ(answers.size(), 1, "1 answer cmd was expected, found 0 or >1");
        auto laa = DynamicCast<LinkAdrAns>(answers.at(0));
        NS_TEST_ASSERT_MSG_NE(laa, nullptr, "LinkAdrAns was expected, cmd type cast failed");
        NS_TEST_EXPECT_MSG_EQ(laa->GetChannelMaskAck(), true, "ChannelMaskAck expected to be true");
        NS_TEST_EXPECT_MSG_EQ(laa->GetDataRateAck(), false, "DataRateAck expected to be false");
        NS_TEST_EXPECT_MSG_EQ(laa->GetPowerAck(), false, "PowerAck expected to be false");
    }

    Reset();
    // LinkAdrReq: invalid chMask, data rate and power
    { // WARNING: default values are manually set here
        uint8_t dataRate = 12;
        uint8_t txPower = 8;
        uint16_t chMask = 0b0;
        uint8_t chMaskCntl = 0;
        uint8_t nbTrans = 6;
        auto answers = RunMacCommand<LinkAdrReq>(dataRate, txPower, chMask, chMaskCntl, nbTrans);
        NS_TEST_EXPECT_MSG_EQ(unsigned(m_mac->GetDataRate()),
                              0,
                              "m_dataRate expected to be default value");
        NS_TEST_EXPECT_MSG_EQ(m_mac->GetTransmissionPowerDbm(),
                              14,
                              "m_txPowerDbm expected to be default value");
        NS_TEST_EXPECT_MSG_EQ(unsigned(m_mac->GetMaxNumberOfTransmissions()),
                              1,
                              "m_nbTrans expected to be default value");
        auto channels = m_mac->GetLogicalLoraChannelHelper()->GetRawChannelArray();
        for (size_t i = 0; i < channels.size(); i++)
        {
            const auto& c = channels.at(i + 16 * chMaskCntl);
            bool actual = (c) ? c->IsEnabledForUplink() : false;
            bool expected = (uint16_t(0b111) & 0b1 << i);
            NS_TEST_EXPECT_MSG_EQ(actual, expected, "Channel " << i << " state != default");
        }
        NS_TEST_ASSERT_MSG_EQ(answers.size(), 1, "1 answer cmd was expected, found 0 or >1");
        auto laa = DynamicCast<LinkAdrAns>(answers.at(0));
        NS_TEST_ASSERT_MSG_NE(laa, nullptr, "LinkAdrAns was expected, cmd type cast failed");
        NS_TEST_EXPECT_MSG_EQ(laa->GetChannelMaskAck(), false, "ChannelMaskAck != false");
        NS_TEST_EXPECT_MSG_EQ(laa->GetDataRateAck(), false, "DataRateAck expected to be false");
        NS_TEST_EXPECT_MSG_EQ(laa->GetPowerAck(), false, "PowerAck expected to be false");
    }

    Reset();
    // LinkAdrReq: invalid chMask, valid data rate and power
    { // WARNING: default values are manually set here
        uint8_t dataRate = 1;
        uint8_t txPower = 7;
        uint16_t chMask = 0b1000; // enable only non-exisitng channel
        uint8_t chMaskCntl = 0;
        uint8_t nbTrans = 3;
        auto answers = RunMacCommand<LinkAdrReq>(dataRate, txPower, chMask, chMaskCntl, nbTrans);
        NS_TEST_EXPECT_MSG_EQ(unsigned(m_mac->GetDataRate()),
                              0,
                              "m_dataRate expected to be default value");
        NS_TEST_EXPECT_MSG_EQ(m_mac->GetTransmissionPowerDbm(),
                              14,
                              "m_txPowerDbm expected to be default value");
        NS_TEST_EXPECT_MSG_EQ(unsigned(m_mac->GetMaxNumberOfTransmissions()),
                              1,
                              "m_nbTrans expected to be default value");
        auto channels = m_mac->GetLogicalLoraChannelHelper()->GetRawChannelArray();
        for (size_t i = 0; i < channels.size(); i++)
        {
            const auto& c = channels.at(i + 16 * chMaskCntl);
            bool actual = (c) ? c->IsEnabledForUplink() : false;
            bool expected = (uint16_t(0b111) & 0b1 << i);
            NS_TEST_EXPECT_MSG_EQ(actual, expected, "Channel " << i << " state != default");
        }
        NS_TEST_ASSERT_MSG_EQ(answers.size(), 1, "1 answer cmd was expected, found 0 or >1");
        auto laa = DynamicCast<LinkAdrAns>(answers.at(0));
        NS_TEST_ASSERT_MSG_NE(laa, nullptr, "LinkAdrAns was expected, cmd type cast failed");
        NS_TEST_EXPECT_MSG_EQ(laa->GetChannelMaskAck(), false, "ChannelMaskAck != false");
        NS_TEST_EXPECT_MSG_EQ(laa->GetDataRateAck(), true, "DataRateAck expected to be true");
        NS_TEST_EXPECT_MSG_EQ(laa->GetPowerAck(), true, "PowerAck expected to be true");
    }

    Reset();
    // LinkAdrReq: fringe parameter values
    { // WARNING: default values are manually set here
        uint8_t dataRate = 0xF;
        uint8_t txPower = 0xF;  // 0x0F ignores config
        uint16_t chMask = 0b0;  // should be ignored because chMaskCntl is 6
        uint8_t chMaskCntl = 6; // all channels on
        uint8_t nbTrans = 0;    // restore default 1
        // Set device params to values different from default
        m_mac->SetDataRate(3);
        m_mac->SetTransmissionPowerDbm(12);
        m_mac->SetMaxNumberOfTransmissions(15);
        auto channels = m_mac->GetLogicalLoraChannelHelper()->GetRawChannelArray();
        channels.at(0)->DisableForUplink();
        auto answers = RunMacCommand<LinkAdrReq>(dataRate, txPower, chMask, chMaskCntl, nbTrans);
        NS_TEST_EXPECT_MSG_EQ(unsigned(m_mac->GetDataRate()),
                              3,
                              "m_dataRate expected to be default value");
        NS_TEST_EXPECT_MSG_EQ(m_mac->GetTransmissionPowerDbm(),
                              12,
                              "m_txPowerDbm expected to be default value");
        NS_TEST_EXPECT_MSG_EQ(unsigned(m_mac->GetMaxNumberOfTransmissions()),
                              1,
                              "m_nbTrans expected to be default value");
        for (size_t i = 0; i < channels.size(); i++)
        {
            const auto& c = channels.at(i);
            bool actual = (c) ? c->IsEnabledForUplink() : false;
            bool expected = (uint16_t(0b111) & 0b1 << i);
            NS_TEST_EXPECT_MSG_EQ(actual, expected, "Channel " << i << " state != default");
        }
        NS_TEST_ASSERT_MSG_EQ(answers.size(), 1, "1 answer cmd was expected, found 0 or >1");
        auto laa = DynamicCast<LinkAdrAns>(answers.at(0));
        NS_TEST_ASSERT_MSG_NE(laa, nullptr, "LinkAdrAns was expected, cmd type cast failed");
        NS_TEST_EXPECT_MSG_EQ(laa->GetChannelMaskAck(), true, "ChannelMaskAck != true");
        NS_TEST_EXPECT_MSG_EQ(laa->GetDataRateAck(), true, "DataRateAck expected to be true");
        NS_TEST_EXPECT_MSG_EQ(laa->GetPowerAck(), true, "PowerAck expected to be true");
    }

    Reset();
    // DutyCycleReq: duty cycle to 100%
    {
        uint8_t maxDutyCycle = 0;
        auto answers = RunMacCommand<DutyCycleReq>(maxDutyCycle);
        NS_TEST_EXPECT_MSG_EQ(m_mac->GetAggregatedDutyCycle(),
                              1 / std::pow(2, maxDutyCycle),
                              "m_aggregatedDutyCycle != 1");
        NS_TEST_ASSERT_MSG_EQ(answers.size(), 1, "1 answer cmd was expected, found 0 or >1");
        auto dca = DynamicCast<DutyCycleAns>(answers.at(0));
        NS_TEST_EXPECT_MSG_NE(dca, nullptr, "DutyCycleAns was expected, cmd type cast failed");
    }

    Reset();
    // DutyCycleReq: duty cycle to 12.5%
    {
        uint8_t maxDutyCycle = 3;
        auto answers = RunMacCommand<DutyCycleReq>(maxDutyCycle);
        NS_TEST_EXPECT_MSG_EQ(m_mac->GetAggregatedDutyCycle(),
                              1 / std::pow(2, maxDutyCycle),
                              "m_aggregatedDutyCycle != 1");
        NS_TEST_ASSERT_MSG_EQ(answers.size(), 1, "1 answer cmd was expected, found 0 or >1");
        auto dca = DynamicCast<DutyCycleAns>(answers.at(0));
        NS_TEST_EXPECT_MSG_NE(dca, nullptr, "DutyCycleAns was expected, cmd type cast failed");
    }

    Reset();
    // RxParamSetupReq: set rx1Dr, rx2Dr, frequency
    {
        uint8_t rx1DrOffset = 5;
        uint8_t rx2DataRate = 5;
        double frequencyHz = 863500000;
        m_mac->SetDataRate(5);
        auto answers = RunMacCommand<RxParamSetupReq>(rx1DrOffset, rx2DataRate, frequencyHz);
        NS_TEST_EXPECT_MSG_EQ(unsigned(m_mac->GetFirstReceiveWindowDataRate()),
                              unsigned(5 - rx1DrOffset),
                              "Rx1DataRate does not match rx1DrOffset from RxParamSetupReq");
        NS_TEST_EXPECT_MSG_EQ(unsigned(m_mac->GetSecondReceiveWindowDataRate()),
                              unsigned(rx2DataRate),
                              "Rx2DataRate does not match rx2DataRate from RxParamSetupReq");
        NS_TEST_EXPECT_MSG_EQ(m_mac->GetSecondReceiveWindowFrequency(),
                              frequencyHz,
                              "Rx2 frequency does not match frequency from RxParamSetupReq");
        NS_TEST_ASSERT_MSG_EQ(answers.size(), 1, "1 answer cmd was expected, found 0 or >1");
        auto rpsa = DynamicCast<RxParamSetupAns>(answers.at(0));
        NS_TEST_ASSERT_MSG_NE(rpsa, nullptr, "RxParamSetupAns was expected, cmd type cast failed");
        NS_TEST_EXPECT_MSG_EQ(rpsa->GetRx1DrOffsetAck(), true, "Rx1DrOffsetAck != true");
        NS_TEST_EXPECT_MSG_EQ(rpsa->GetRx2DataRateAck(), true, "Rx2DataRateAck != true");
        NS_TEST_EXPECT_MSG_EQ(rpsa->GetChannelAck(), true, "ChannelAck expected to be true");
    }

    Reset();
    // RxParamSetupReq: invalid rx1Dr, rx2Dr, frequency
    { // WARNING: default values are manually set here
        uint8_t rx1DrOffset = 6;
        uint8_t rx2DataRate = 12;
        double frequencyHz = 871000000;
        m_mac->SetDataRate(5);
        auto answers = RunMacCommand<RxParamSetupReq>(rx1DrOffset, rx2DataRate, frequencyHz);
        NS_TEST_EXPECT_MSG_EQ(unsigned(m_mac->GetFirstReceiveWindowDataRate()),
                              5,
                              "Rx1DataRate expected to be default value");
        NS_TEST_EXPECT_MSG_EQ(unsigned(m_mac->GetSecondReceiveWindowDataRate()),
                              0,
                              "Rx2DataRate expected to be default value");
        NS_TEST_EXPECT_MSG_EQ(m_mac->GetSecondReceiveWindowFrequency(),
                              869525000,
                              "Rx2 frequency expected to be default value");
        NS_TEST_ASSERT_MSG_EQ(answers.size(), 1, "1 answer cmd was expected, found 0 or >1");
        auto rpsa = DynamicCast<RxParamSetupAns>(answers.at(0));
        NS_TEST_ASSERT_MSG_NE(rpsa, nullptr, "RxParamSetupAns was expected, cmd type cast failed");
        NS_TEST_EXPECT_MSG_EQ(rpsa->GetRx1DrOffsetAck(), false, "Rx1DrOffsetAck != false");
        NS_TEST_EXPECT_MSG_EQ(rpsa->GetRx2DataRateAck(), false, "Rx2DataRateAck != false");
        NS_TEST_EXPECT_MSG_EQ(rpsa->GetChannelAck(), false, "ChannelAck expected to be false");
    }

    Reset();
    // DevStatusReq: get default values
    { // WARNING: default values are manually set here
        auto answers = RunMacCommand<DevStatusReq>();
        NS_TEST_ASSERT_MSG_EQ(answers.size(), 1, "1 answer cmd was expected, found 0 or >1");
        auto dsa = DynamicCast<DevStatusAns>(answers.at(0));
        NS_TEST_ASSERT_MSG_NE(dsa, nullptr, "DevStatusAns was expected, cmd type cast failed");
        NS_TEST_EXPECT_MSG_EQ(unsigned(dsa->GetBattery()), 0, "Battery expected == 0 (ext power)");
        NS_TEST_EXPECT_MSG_EQ(unsigned(dsa->GetMargin()), 31, "Margin expected to be 31 (default)");
    }

    Reset();
    // NewChannelReq: add a new channel
    {
        uint8_t chIndex = 4;
        double frequencyHz = 865100000;
        uint8_t minDataRate = 1;
        uint8_t maxDataRate = 4;
        auto answers = RunMacCommand<NewChannelReq>(chIndex, frequencyHz, minDataRate, maxDataRate);
        NS_TEST_ASSERT_MSG_EQ(answers.size(), 1, "1 answer cmd was expected, found 0 or >1");
        auto c = m_mac->GetLogicalLoraChannelHelper()->GetRawChannelArray().at(chIndex);
        NS_TEST_ASSERT_MSG_NE(c, nullptr, "Channel at chIndex slot expected not to be nullptr");
        NS_TEST_EXPECT_MSG_EQ(c->GetFrequency(),
                              frequencyHz,
                              "Channel frequency expected to equal NewChannelReq frequency");
        NS_TEST_EXPECT_MSG_EQ(c->GetMinimumDataRate(),
                              unsigned(minDataRate),
                              "Channel minDataRate expected to equal NewChannelReq minDataRate");
        NS_TEST_EXPECT_MSG_EQ(c->GetMaximumDataRate(),
                              unsigned(maxDataRate),
                              "Channel maxDataRate expected to equal NewChannelReq maxDataRate");
        auto nca = DynamicCast<NewChannelAns>(answers.at(0));
        NS_TEST_ASSERT_MSG_NE(nca, nullptr, "NewChannelAns was expected, cmd type cast failed");
        NS_TEST_EXPECT_MSG_EQ(nca->GetDataRateRangeOk(), true, "DataRateRangeOk != true");
        NS_TEST_EXPECT_MSG_EQ(nca->GetChannelFrequencyOk(), true, "ChannelFrequencyOk != true");
    }

    Reset();
    // NewChannelReq: invalid new channel
    { // WARNING: default values are manually set here
        uint8_t chIndex = 1;
        double frequencyHz = 862000000;
        uint8_t minDataRate = 14;
        uint8_t maxDataRate = 13;
        auto answers = RunMacCommand<NewChannelReq>(chIndex, frequencyHz, minDataRate, maxDataRate);
        NS_TEST_ASSERT_MSG_EQ(answers.size(), 1, "1 answer cmd was expected, found 0 or >1");
        double defaultFrequenciesHz[3] = {868100000, 868300000, 868500000};
        auto channels = m_mac->GetLogicalLoraChannelHelper()->GetRawChannelArray();
        for (size_t i = 0; i < channels.size(); i++)
        {
            const auto& c = channels.at(i);
            if (i > 2)
            {
                NS_TEST_ASSERT_MSG_EQ(c, nullptr, "Channel " << i << "expected to be nullptr");
                continue;
            }
            NS_TEST_EXPECT_MSG_EQ(c->GetFrequency(),
                                  defaultFrequenciesHz[i],
                                  "Channel frequency expected to equal NewChannelReq frequency");
            NS_TEST_EXPECT_MSG_EQ(unsigned(c->GetMinimumDataRate()),
                                  0,
                                  "Channel " << i << " minDataRate expected to be default");
            NS_TEST_EXPECT_MSG_EQ(unsigned(c->GetMaximumDataRate()),
                                  5,
                                  "Channel " << i << " maxDataRate expected to be default");
            NS_TEST_EXPECT_MSG_EQ(c->IsEnabledForUplink(),
                                  true,
                                  "Channel " << i << " state expected to be active by default");
        }
        auto nca = DynamicCast<NewChannelAns>(answers.at(0));
        NS_TEST_ASSERT_MSG_NE(nca, nullptr, "NewChannelAns was expected, cmd type cast failed");
        NS_TEST_EXPECT_MSG_EQ(nca->GetDataRateRangeOk(), false, "DataRateRangeOk != false");
        NS_TEST_EXPECT_MSG_EQ(nca->GetChannelFrequencyOk(), false, "ChannelFrequencyOk != false");
    }

    // Discard the last cycle's orphaned Simulator event (see RunMacCommand()'s doc comment) --
    // Reset() only guards against the *previous* cycle's leak at the *start* of the *next* one,
    // so without this, this test's own final leak would still be left for whichever later
    // TestCase happens to next call Simulator::Run() for real.
    Simulator::Destroy();
}

/**
 * @ingroup lorawan
 *
 * It tests the correct execution of the ADR backoff procedure of LoRaWAN devices.
 * (See, LoRaWAN L2 1.0.4 Specifications (2020), Section 4.3.1.1)
 */
class AdrBackoffTest : public TestCase
{
  public:
    AdrBackoffTest();           //!< Default constructor
    ~AdrBackoffTest() override; //!< Destructor

  private:
    /**
     * Create and send an empty app payload unconfirmed frame through the MAC layer to increment
     * of the FCnt and ADRACKCnt and eventually activate the ADR backoff procedure configurations of
     * the MAC layer. The packet is sent after a delay (simulated time is fast-forwarded to the
     * event) such that the device does not incur any duty-cycle limitation. The sent packet FHDR is
     * returned as argument for validation purposes.
     *
     * @param after Delay to schedule the packet after to avoid duty-cycle limitations
     * @param fhdr [out] FHDR of the constructed frame passed to PHY by the MAC
     */
    void SendUplink(Time after, LoraFrameHeader& fhdr);

    /**
     * Create and schedule the PHY reception of a downlink transmission configured for the LoRaWAN
     * MAC first reception window. This is used to test resetting the ADR backoff procedure.
     *
     * @note This does not call Simulator::Run(), enabling preemptive scheduling, but must be
     * manually timed to happen during the first reception window
     *
     * It constrains the device to a single uplink channel to force-out the first reception window
     * frequency. The downlink spreading factor is taken from the current MAC configuration.
     *
     * @param after Delay to schedule the packet after (must target the first reception window)
     */
    void ScheduleRx1Downlink(Time after);

    /**
     * This function resets the simulation and device MAC layer, use before test sub-cases.
     */
    void Reset();

    void DoRun() override;

    Ptr<ClassAEndDeviceLorawanMac> m_mac; //!< The end device's MAC layer used in tests.
};

AdrBackoffTest::AdrBackoffTest()
    : TestCase("Test the ADR backoff procedure of the LoRaWAN MAC protocol")
{
}

AdrBackoffTest::~AdrBackoffTest()
{
    m_mac = nullptr;
}

void
AdrBackoffTest::SendUplink(Time after, LoraFrameHeader& fhdr)
{
    Ptr<Packet> pkt;
    LorawanMacHeader mhdr;
    // Send packet through the MAC layer
    pkt = Create<Packet>(0);
    Simulator::Schedule(after, &ClassAEndDeviceLorawanMac::Send, m_mac, pkt);
    Simulator::Run();
    // Retrieve uplink FHDR
    pkt->RemoveHeader(mhdr);
    fhdr.SetAsUplink();
    pkt->RemoveHeader(fhdr);
    NS_LOG_LOGIC("FHDR: " << fhdr);
}

void
AdrBackoffTest::ScheduleRx1Downlink(Time after)
{
    Ptr<Packet> pkt;
    LoraFrameHeader fhdr;
    LorawanMacHeader mhdr;
    // Prepare DL packet
    pkt = Create<Packet>(0);
    fhdr.SetAsDownlink();
    pkt->AddHeader(fhdr);
    mhdr.SetFType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
    pkt->AddHeader(mhdr);
    // Force the next RX1 window channel frequency
    auto chVec = m_mac->GetLogicalLoraChannelHelper()->GetRawChannelArray();
    chVec.at(1)->DisableForUplink();
    chVec.at(2)->DisableForUplink();
    // Schedule MAC layer reception through PHY
    auto phy = DynamicCast<SimpleEndDeviceLoraPhy>(m_mac->GetPhy());
    Simulator::Schedule(after,
                        &SimpleEndDeviceLoraPhy::StartReceive,
                        phy,
                        pkt,
                        chVec.at(0)->GetFrequency(),
                        IQPolarity::DOWN,
                        12,
                        -100,
                        MilliSeconds(10));
}

void
AdrBackoffTest::Reset()
{
    Simulator::Destroy();
    // Reset MAC state
    LorawanMacHelper macHelper;
    macHelper.SetRegion(LorawanMacHelper::EU);
    macHelper.SetDeviceType(LorawanMacHelper::ED_A);
    /// @todo Install should not require a node in input.
    m_mac = DynamicCast<ClassAEndDeviceLorawanMac>(macHelper.Install(nullptr, nullptr));
    NS_TEST_EXPECT_MSG_NE(m_mac, nullptr, "Failed to initialize MAC layer object.");
    auto phy = CreateObject<SimpleEndDeviceLoraPhy>();
    phy->SetChannel(CreateObject<LoraChannel>());
    phy->SetMobility(CreateObject<ConstantPositionMobilityModel>());
    m_mac->SetPhy(phy);
}

void
AdrBackoffTest::DoRun()
{
    NS_LOG_DEBUG("AdrBackoffTest");

    Reset();
    // Full ADR Backoff procedure
    {
        LoraFrameHeader fhdr;
        auto llch = m_mac->GetLogicalLoraChannelHelper();
        auto ADR_ACK_LIMIT = EndDeviceLorawanMac::ADR_ACK_LIMIT;
        auto ADR_ACK_DELAY = EndDeviceLorawanMac::ADR_ACK_DELAY;
        // Custom config to force full ADR backoff
        {
            // Tx parameters to furthest settings from default
            m_mac->SetDataRate(5);
            m_mac->SetTransmissionPowerDbm(0);
            m_mac->SetMaxNumberOfTransmissions(8);
            auto chVec = llch->GetRawChannelArray();
            chVec.at(0)->DisableForUplink();
            chVec.at(1)->DisableForUplink();
            chVec.at(2)->DisableForUplink();
            // Provide additional non-default channel for uplinks
            auto nonDefaultChannel = Create<LogicalLoraChannel>(869850000, 0, 5);
            llch->SetChannel(3, nonDefaultChannel);
        }
        // 7 total backoff steps: 1 tx power + 5 data rate + 1 nbtrans & channels
        for (uint32_t fCnt = 0; fCnt <= ADR_ACK_LIMIT + ADR_ACK_DELAY * 7U; ++fCnt)
        {
            SendUplink(Minutes(20), fhdr);
            NS_TEST_EXPECT_MSG_EQ(fhdr.GetFCnt(), fCnt, "Unexpected FCnt value in uplink FHDR");
            NS_TEST_EXPECT_MSG_EQ(fhdr.GetAdrAckReq(),
                                  fCnt >= ADR_ACK_LIMIT,
                                  "Unexpected ADRACKReq value in FHDR of uplink fCnt=" << fCnt);
            uint8_t step = (fCnt >= ADR_ACK_LIMIT) ? (fCnt - ADR_ACK_LIMIT) / ADR_ACK_DELAY : 0;
            NS_TEST_EXPECT_MSG_EQ(m_mac->GetTransmissionPowerDbm(),
                                  (step > 0) ? 14 : 0,
                                  "Unexpected tx power on uplink fCnt=" << fCnt);
            uint8_t expectedDr = (step == 0) ? 5 : (step < 7) ? 5 - (step - 1) : 0;
            NS_TEST_EXPECT_MSG_EQ(m_mac->GetDataRate(),
                                  expectedDr,
                                  "Unexpected data rate on uplink fCnt=" << fCnt);
            const auto chVec = llch->GetRawChannelArray();
            for (uint8_t i = 0; i < 3; ++i)
            {
                NS_TEST_EXPECT_MSG_EQ(chVec.at(i)->IsEnabledForUplink(),
                                      step >= 7,
                                      "Unexpected activation state of channel "
                                          << unsigned(i) << " on uplink fCnt=" << fCnt);
            }
            NS_TEST_EXPECT_MSG_EQ(
                chVec.at(3)->IsEnabledForUplink(),
                true,
                "Unexpected activation state of channel 3 on uplink fCnt=" << fCnt);
        }
    }

    Reset();
    // ADRACKReq back to false after downlink
    {
        LoraFrameHeader fhdr;
        auto ADR_ACK_LIMIT = EndDeviceLorawanMac::ADR_ACK_LIMIT;
        // Trigger ADRACKReq
        for (uint16_t fCnt = 0; fCnt <= ADR_ACK_LIMIT + 5; ++fCnt)
        {
            SendUplink(Minutes(20), fhdr);
            NS_TEST_EXPECT_MSG_EQ(fhdr.GetFCnt(), fCnt, "Unexpected FCnt value in uplink FHDR");
            NS_TEST_EXPECT_MSG_EQ(fhdr.GetAdrAckReq(),
                                  fCnt >= ADR_ACK_LIMIT,
                                  "Unexpected ADRACKReq value in FHDR of uplink fCnt=" << fCnt);
        }
        // Receive downlink for the next packet RX window
        ScheduleRx1Downlink(Minutes(20) + Seconds(2));
        // Trigger reception windows with new uplink
        SendUplink(Minutes(20), fhdr);
        NS_TEST_EXPECT_MSG_EQ(fhdr.GetFCnt(),
                              ADR_ACK_LIMIT + 5 + 1,
                              "Unexpected FCnt value in uplink FHDR");
        NS_TEST_EXPECT_MSG_EQ(
            fhdr.GetAdrAckReq(),
            true,
            "Unexpected ADRACKReq value in FHDR of uplink fCnt=" << fhdr.GetFCnt());
        // Next uplink should have ADRACKReq unset because a downlink was received
        SendUplink(Minutes(20), fhdr);
        NS_TEST_EXPECT_MSG_EQ(fhdr.GetFCnt(),
                              ADR_ACK_LIMIT + 5 + 2,
                              "Unexpected FCnt value in uplink FHDR");
        NS_TEST_EXPECT_MSG_EQ(
            fhdr.GetAdrAckReq(),
            false,
            "Unexpected ADRACKReq value in FHDR of uplink fCnt=" << fhdr.GetFCnt());
    }
}

/**
 * @ingroup lorawan
 *
 * It tests the correct execution of the retransmissions in LoRaWAN devices.
 * (See, LoRaWAN L2 1.0.4 Specifications (2020), Section 4.3.1.3)
 */
class RetransmissionTest : public TestCase
{
  public:
    RetransmissionTest();           //!< Default constructor
    ~RetransmissionTest() override; //!< Destructor

  private:
    /**
     * Create and send an empty app payload unconfirmed frame through the MAC layer NbTrans times.
     * The sent packet FHDR is returned as argument for validation purposes.
     *
     * @param fhdr [out] FHDR of the constructed frame passed to PHY by the MAC
     */
    void SendUplink(LoraFrameHeader& fhdr);

    /**
     * Create and schedule the PHY reception of a downlink transmission configured for the LoRaWAN
     * MAC first reception window. This is used to test stopping the retransmission process.
     *
     * @note This does not call Simulator::Run(), enabling preemptive scheduling, but must be
     * manually timed to happen during the first reception window
     *
     * It constrains the device to a single uplink channel to force-out the first reception window
     * frequency. The downlink spreading factor is taken from the current MAC configuration.
     *
     * @param after Delay to schedule the packet after (must target the first reception window)
     * @param ack Whether to set the ACK flag in the frame header
     */
    void ScheduleRx1Downlink(Time after, bool ack = false);

    /**
     * Callback for tracing MAC layer SentNewPacket.
     *
     * @param packet The packet sent.
     */
    void MacSentNewPacket(Ptr<const Packet> packet);

    /**
     * Callback for tracing the outcome of MAC layer's confirmed packet retransmission and
     * acknowledgement.
     *
     * @note This callback only traces confirmed packets, unused otherwise.
     *
     * @param txCount Number of transmissions attempted during the process.
     * @param ack Whether the retransmission process led to acknowledgement.
     * @param firstAttempt Timestamp of the initial transmission attempt.
     * @param packet The packet being retransmitted.
     */
    void MacConfirmedTransmissionOutcome(uint8_t txCount,
                                         bool ack,
                                         Time firstAttempt,
                                         Ptr<Packet> packet);

    /**
     * Callback for tracing PHY layer StartSending.
     *
     * @param packet The packet being sent.
     * @param node The sender node id if any, 0 otherwise.
     */
    void PhyStartSending(Ptr<const Packet> packet, uint32_t node);

    /**
     * Callback for tracing PHY layer ReceivedPacket.
     *
     * @param packet The packet being received.
     * @param node The sender node id if any, 0 otherwise.
     */
    void PhyReceivedPacket(Ptr<const Packet> packet, uint32_t node);

    /**
     * This function resets the simulation and device MAC layer, use before test sub-cases.
     */
    void Reset();

    void DoRun() override;

    Ptr<ClassAEndDeviceLorawanMac> m_mac; //!< The end device's MAC layer used in tests.
    Ptr<Packet> m_packet;                 //!< Target packet for tracing

    int m_macSentNewPacketCalls = 0;  //!< Counter for MacSentNewPacket calls
    int m_macConfirmedTxOutcome = 0;  //!< Counter for MacConfirmedTransmissionOutcome calls
    int m_phyStartSendingCalls = 0;   //!< Counter for PhyStartSending calls
    int m_phyReceivedPacketCalls = 0; //!< Counter for PhyReceivedPacket calls

    uint8_t m_numTransmissions = 0;   //!< Number of confirmed packet transmissions
    bool m_successfullyAcked = false; //!< Acknowledgement of confirmed packet
};

RetransmissionTest::RetransmissionTest()
    : TestCase("Test the retransmission process of the LoRaWAN MAC protocol")
{
}

RetransmissionTest::~RetransmissionTest()
{
    m_mac = nullptr;
}

void
RetransmissionTest::SendUplink(LoraFrameHeader& fhdr)
{
    Ptr<Packet> pkt;
    LorawanMacHeader mhdr;
    // Send packet through the MAC layer
    pkt = Create<Packet>(0);
    m_packet = pkt;
    Simulator::ScheduleNow(&ClassAEndDeviceLorawanMac::Send, m_mac, pkt);
    Simulator::Run();
    // Retrieve uplink FHDR
    pkt->RemoveHeader(mhdr);
    fhdr.SetAsUplink();
    pkt->RemoveHeader(fhdr);
    NS_LOG_LOGIC("Frame Header: " << fhdr);
}

void
RetransmissionTest::ScheduleRx1Downlink(Time after, bool ack)
{
    Ptr<Packet> pkt;
    LoraFrameHeader fhdr;
    LorawanMacHeader mhdr;
    // Prepare DL packet
    pkt = Create<Packet>(0);
    fhdr.SetAsDownlink();
    fhdr.SetAck(ack);
    pkt->AddHeader(fhdr);
    mhdr.SetFType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
    pkt->AddHeader(mhdr);
    // Force the next RX1 window channel frequency
    auto chVec = m_mac->GetLogicalLoraChannelHelper()->GetRawChannelArray();
    chVec.at(1)->DisableForUplink();
    chVec.at(2)->DisableForUplink();
    // Schedule MAC layer reception through PHY
    auto phy = DynamicCast<SimpleEndDeviceLoraPhy>(m_mac->GetPhy());
    Simulator::Schedule(after,
                        &SimpleEndDeviceLoraPhy::StartReceive,
                        phy,
                        pkt,
                        chVec.at(0)->GetFrequency(),
                        IQPolarity::DOWN,
                        m_mac->GetSfFromDataRate(m_mac->GetDataRate()),
                        -100,
                        MilliSeconds(10));
}

void
RetransmissionTest::MacSentNewPacket(Ptr<const Packet> packet)
{
    m_macSentNewPacketCalls++;
}

void
RetransmissionTest::MacConfirmedTransmissionOutcome(uint8_t txCount,
                                                    bool ack,
                                                    Time firstAttempt,
                                                    Ptr<Packet> packet)
{
    m_macConfirmedTxOutcome++;
    if (m_packet == packet)
    {
        m_numTransmissions = txCount;
        m_successfullyAcked = ack;
    }
}

void
RetransmissionTest::PhyStartSending(Ptr<const Packet> packet, uint32_t node)
{
    m_phyStartSendingCalls++;
}

void
RetransmissionTest::PhyReceivedPacket(Ptr<const Packet> packet, uint32_t node)
{
    m_phyReceivedPacketCalls++;
}

void
RetransmissionTest::Reset()
{
    m_macSentNewPacketCalls = 0;
    m_macConfirmedTxOutcome = 0;
    m_phyStartSendingCalls = 0;
    m_phyReceivedPacketCalls = 0;
    m_numTransmissions = 0;
    m_successfullyAcked = false;
    Simulator::Destroy();
    // Reset MAC state
    LorawanMacHelper macHelper;
    macHelper.SetRegion(LorawanMacHelper::EU);
    macHelper.SetDeviceType(LorawanMacHelper::ED_A);
    m_mac = DynamicCast<ClassAEndDeviceLorawanMac>(macHelper.Install(nullptr, nullptr));
    m_mac->SetDataRate(5);
    m_mac->TraceConnectWithoutContext("SentNewPacket",
                                      MakeCallback(&RetransmissionTest::MacSentNewPacket, this));
    m_mac->TraceConnectWithoutContext(
        "ConfirmedTransmissionOutcome",
        MakeCallback(&RetransmissionTest::MacConfirmedTransmissionOutcome, this));
    NS_TEST_EXPECT_MSG_NE(m_mac, nullptr, "Failed to initialize MAC layer object.");
    auto phy = CreateObject<SimpleEndDeviceLoraPhy>();
    phy->SetChannel(CreateObject<LoraChannel>());
    phy->SetMobility(CreateObject<ConstantPositionMobilityModel>());
    phy->TraceConnectWithoutContext("StartSending",
                                    MakeCallback(&RetransmissionTest::PhyStartSending, this));
    phy->TraceConnectWithoutContext("ReceivedPacket",
                                    MakeCallback(&RetransmissionTest::PhyReceivedPacket, this));
    m_mac->SetPhy(phy);
    m_mac->Initialize();
}

void
RetransmissionTest::DoRun()
{
    NS_LOG_DEBUG("RetransmissionTest");

    Reset();
    // Unconfirmed send yields the correct number of retransmissions (base case)
    { // WARNING: default values are manually set here
        m_mac->SetFType(LorawanMacHeader::UNCONFIRMED_DATA_UP);
        LoraFrameHeader fhdr;
        SendUplink(fhdr);
        NS_TEST_EXPECT_MSG_EQ(fhdr.GetFCnt(), 0, "Unexpected FCnt value in uplink FHDR");
        NS_TEST_ASSERT_MSG_EQ(m_mac->GetUplinkFrameCounter(),
                              1,
                              "Unexpected MAC frame counter value");
        NS_TEST_ASSERT_MSG_EQ(m_phyStartSendingCalls,
                              1,
                              "Unexpected number of PHY layer StartSending calls");
        NS_TEST_ASSERT_MSG_EQ(m_phyReceivedPacketCalls,
                              0,
                              "Unexpected number of PHY layer ReceivedPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macSentNewPacketCalls,
                              1,
                              "Unexpected number of MAC layer SendNewPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macConfirmedTxOutcome,
                              0,
                              "Unexpected number of MAC layer ConfirmedTransmissionOutcome calls");
    }

    Reset();
    // Unconfirmed send yields the correct number of retransmissions
    {
        uint8_t nbTrans = 4;
        m_mac->SetMaxNumberOfTransmissions(nbTrans);
        m_mac->SetFType(LorawanMacHeader::UNCONFIRMED_DATA_UP);
        LoraFrameHeader fhdr;
        SendUplink(fhdr);
        NS_TEST_EXPECT_MSG_EQ(fhdr.GetFCnt(), 0, "Unexpected FCnt value in uplink FHDR");
        NS_TEST_ASSERT_MSG_EQ(m_mac->GetUplinkFrameCounter(),
                              1,
                              "Unexpected FCnt value in MAC layer");
        NS_TEST_ASSERT_MSG_EQ(m_phyStartSendingCalls,
                              nbTrans,
                              "Unexpected number of PHY layer StartSending calls");
        NS_TEST_ASSERT_MSG_EQ(m_phyReceivedPacketCalls,
                              0,
                              "Unexpected number of PHY layer ReceivedPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macSentNewPacketCalls,
                              1,
                              "Unexpected number of MAC layer SendNewPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macConfirmedTxOutcome,
                              0,
                              "Unexpected number of MAC layer ConfirmedTransmissionOutcome calls");
    }

    Reset();
    // Unconfirmed send yields the correct number of retransmissions (limit case)
    {
        uint8_t nbTrans = 15;
        m_mac->SetMaxNumberOfTransmissions(nbTrans);
        m_mac->SetFType(LorawanMacHeader::UNCONFIRMED_DATA_UP);
        LoraFrameHeader fhdr;
        SendUplink(fhdr);
        NS_TEST_EXPECT_MSG_EQ(fhdr.GetFCnt(), 0, "Unexpected FCnt value in uplink FHDR");
        NS_TEST_ASSERT_MSG_EQ(m_mac->GetUplinkFrameCounter(),
                              1,
                              "Unexpected MAC frame counter value");
        NS_TEST_ASSERT_MSG_EQ(m_phyStartSendingCalls,
                              nbTrans,
                              "Unexpected number of physical layer transmissions");
        NS_TEST_ASSERT_MSG_EQ(m_phyReceivedPacketCalls,
                              0,
                              "Unexpected number of PHY layer ReceivedPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macSentNewPacketCalls,
                              1,
                              "Unexpected number of MAC layer SendNewPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macConfirmedTxOutcome,
                              0,
                              "Unexpected number of MAC layer ConfirmedTransmissionOutcome calls");
    }

    Reset();
    // Unconfirmed send interrupted in-between retransmissions
    {
        uint8_t nbTrans = 9;
        m_mac->SetMaxNumberOfTransmissions(nbTrans);
        m_mac->SetFType(LorawanMacHeader::UNCONFIRMED_DATA_UP);
        Simulator::Schedule(Seconds(2.5),
                            &ClassAEndDeviceLorawanMac::Send,
                            m_mac,
                            Create<Packet>(0));
        LoraFrameHeader fhdr;
        SendUplink(fhdr);
        NS_TEST_EXPECT_MSG_EQ(fhdr.GetFCnt(), 0, "Unexpected FCnt value in uplink FHDR");
        NS_TEST_ASSERT_MSG_EQ(m_mac->GetUplinkFrameCounter(),
                              2,
                              "Unexpected FCnt value in MAC layer");
        NS_TEST_ASSERT_MSG_EQ(m_phyStartSendingCalls,
                              1 + nbTrans,
                              "Unexpected number of PHY layer StartSending calls");
        NS_TEST_ASSERT_MSG_EQ(m_phyReceivedPacketCalls,
                              0,
                              "Unexpected number of PHY layer ReceivedPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macSentNewPacketCalls,
                              2,
                              "Unexpected number of MAC layer SendNewPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macConfirmedTxOutcome,
                              0,
                              "Unexpected number of MAC layer ConfirmedTransmissionOutcome calls");
    }

    Reset();
    // Unconfirmed send retransmissions interrupted while MAC layer busy
    {
        uint8_t nbTrans = 8;
        m_mac->SetMaxNumberOfTransmissions(nbTrans);
        m_mac->SetFType(LorawanMacHeader::UNCONFIRMED_DATA_UP);
        Simulator::Schedule(Seconds(7), &ClassAEndDeviceLorawanMac::Send, m_mac, Create<Packet>(0));
        LoraFrameHeader fhdr;
        SendUplink(fhdr);
        NS_TEST_EXPECT_MSG_EQ(fhdr.GetFCnt(), 0, "Unexpected FCnt value in uplink FHDR");
        NS_TEST_ASSERT_MSG_EQ(m_mac->GetUplinkFrameCounter(),
                              2,
                              "Unexpected FCnt value in MAC layer");
        NS_TEST_ASSERT_MSG_EQ(m_phyStartSendingCalls,
                              2 + nbTrans,
                              "Unexpected number of PHY layer StartSending calls");
        NS_TEST_ASSERT_MSG_EQ(m_phyReceivedPacketCalls,
                              0,
                              "Unexpected number of PHY layer ReceivedPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macSentNewPacketCalls,
                              2,
                              "Unexpected number of MAC layer SendNewPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macConfirmedTxOutcome,
                              0,
                              "Unexpected number of MAC layer ConfirmedTransmissionOutcome calls");
    }

    Reset();
    // Unconfirmed send retransmissions interrupted after downlink
    {
        uint8_t nbTrans = 3;
        m_mac->SetMaxNumberOfTransmissions(nbTrans);
        m_mac->SetFType(LorawanMacHeader::UNCONFIRMED_DATA_UP);
        ScheduleRx1Downlink(MilliSeconds(45) + Seconds(1));
        LoraFrameHeader fhdr;
        SendUplink(fhdr);
        NS_TEST_EXPECT_MSG_EQ(fhdr.GetFCnt(), 0, "Unexpected FCnt value in uplink FHDR");
        NS_TEST_ASSERT_MSG_EQ(m_mac->GetUplinkFrameCounter(),
                              1,
                              "Unexpected FCnt value in MAC layer");
        NS_TEST_ASSERT_MSG_EQ(m_phyStartSendingCalls,
                              1,
                              "Unexpected number of PHY layer StartSending calls");
        NS_TEST_ASSERT_MSG_EQ(m_phyReceivedPacketCalls,
                              1,
                              "Unexpected number of PHY layer ReceivedPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macSentNewPacketCalls,
                              1,
                              "Unexpected number of MAC layer SendNewPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macConfirmedTxOutcome,
                              0,
                              "Unexpected number of MAC layer ConfirmedTransmissionOutcome calls");
    }

    Reset();
    // Unconfirmed send retransmissions interrupted after downlink (different params)
    {
        uint8_t nbTrans = 13;
        m_mac->SetMaxNumberOfTransmissions(nbTrans);
        m_mac->SetFType(LorawanMacHeader::UNCONFIRMED_DATA_UP);
        ScheduleRx1Downlink(Seconds(27.665));
        LoraFrameHeader fhdr;
        SendUplink(fhdr);
        NS_TEST_EXPECT_MSG_EQ(fhdr.GetFCnt(), 0, "Unexpected FCnt value in uplink FHDR");
        NS_TEST_ASSERT_MSG_EQ(m_mac->GetUplinkFrameCounter(),
                              1,
                              "Unexpected FCnt value in MAC layer");
        NS_TEST_ASSERT_MSG_EQ(m_phyStartSendingCalls,
                              7,
                              "Unexpected number of PHY layer StartSending calls");
        NS_TEST_ASSERT_MSG_EQ(m_phyReceivedPacketCalls,
                              1,
                              "Unexpected number of PHY layer ReceivedPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macSentNewPacketCalls,
                              1,
                              "Unexpected number of MAC layer SendNewPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macConfirmedTxOutcome,
                              0,
                              "Unexpected number of MAC layer ConfirmedTransmissionOutcome calls");
    }

    Reset();
    // Confirmed yields the correct number of unacknowledged retransmissions
    {
        uint8_t nbTrans = 7;
        m_mac->SetMaxNumberOfTransmissions(nbTrans);
        m_mac->SetFType(LorawanMacHeader::CONFIRMED_DATA_UP);
        LoraFrameHeader fhdr;
        SendUplink(fhdr);
        NS_TEST_EXPECT_MSG_EQ(fhdr.GetFCnt(), 0, "Unexpected FCnt value in uplink FHDR");
        NS_TEST_ASSERT_MSG_EQ(m_mac->GetUplinkFrameCounter(),
                              1,
                              "Unexpected FCnt value in MAC layer");
        NS_TEST_ASSERT_MSG_EQ(m_phyStartSendingCalls,
                              nbTrans,
                              "Unexpected number of PHY layer StartSending calls");
        NS_TEST_ASSERT_MSG_EQ(m_phyReceivedPacketCalls,
                              0,
                              "Unexpected number of PHY layer ReceivedPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macSentNewPacketCalls,
                              1,
                              "Unexpected number of MAC layer SendNewPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macConfirmedTxOutcome,
                              1,
                              "Unexpected number of MAC layer ConfirmedTransmissionOutcome calls");
        NS_TEST_ASSERT_MSG_EQ(m_numTransmissions,
                              nbTrans,
                              "Unexpected number of transmissions for confirmed packet");
        NS_TEST_ASSERT_MSG_EQ(m_successfullyAcked,
                              false,
                              "Unexpected acknowledgment state for confirmed packet");
    }

    Reset();
    // Confirmed send retransmissions interrupted in-between retransmissions
    {
        uint8_t nbTrans = 6;
        m_mac->SetMaxNumberOfTransmissions(nbTrans);
        m_mac->SetFType(LorawanMacHeader::CONFIRMED_DATA_UP);
        Simulator::Schedule(Seconds(8.5),
                            &ClassAEndDeviceLorawanMac::Send,
                            m_mac,
                            Create<Packet>(0));
        LoraFrameHeader fhdr;
        SendUplink(fhdr);
        NS_TEST_EXPECT_MSG_EQ(fhdr.GetFCnt(), 0, "Unexpected FCnt value in uplink FHDR");
        NS_TEST_ASSERT_MSG_EQ(m_mac->GetUplinkFrameCounter(),
                              2,
                              "Unexpected FCnt value in MAC layer");
        NS_TEST_ASSERT_MSG_EQ(m_phyStartSendingCalls,
                              2 + nbTrans,
                              "Unexpected number of PHY layer StartSending calls");
        NS_TEST_ASSERT_MSG_EQ(m_phyReceivedPacketCalls,
                              0,
                              "Unexpected number of PHY layer ReceivedPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macSentNewPacketCalls,
                              2,
                              "Unexpected number of MAC layer SendNewPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macConfirmedTxOutcome,
                              2,
                              "Unexpected number of MAC layer ConfirmedTransmissionOutcome calls");
        NS_TEST_ASSERT_MSG_EQ(m_numTransmissions,
                              2,
                              "Unexpected number of transmissions for confirmed packet");
        NS_TEST_ASSERT_MSG_EQ(m_successfullyAcked,
                              false,
                              "Unexpected acknowledgment state for confirmed packet");
    }

    Reset();
    // Confirmed send retransmissions interrupted interrupted while MAC layer busy
    {
        uint8_t nbTrans = 9;
        m_mac->SetMaxNumberOfTransmissions(nbTrans);
        m_mac->SetFType(LorawanMacHeader::CONFIRMED_DATA_UP);
        Simulator::Schedule(Seconds(10),
                            &ClassAEndDeviceLorawanMac::Send,
                            m_mac,
                            Create<Packet>(0));
        LoraFrameHeader fhdr;
        SendUplink(fhdr);
        NS_TEST_EXPECT_MSG_EQ(fhdr.GetFCnt(), 0, "Unexpected FCnt value in uplink FHDR");
        NS_TEST_ASSERT_MSG_EQ(m_mac->GetUplinkFrameCounter(),
                              2,
                              "Unexpected FCnt value in MAC layer");
        NS_TEST_ASSERT_MSG_EQ(m_phyStartSendingCalls,
                              3 + nbTrans,
                              "Unexpected number of PHY layer StartSending calls");
        NS_TEST_ASSERT_MSG_EQ(m_phyReceivedPacketCalls,
                              0,
                              "Unexpected number of PHY layer ReceivedPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macSentNewPacketCalls,
                              2,
                              "Unexpected number of MAC layer SendNewPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macConfirmedTxOutcome,
                              2,
                              "Unexpected number of MAC layer ConfirmedTransmissionOutcome calls");
        NS_TEST_ASSERT_MSG_EQ(m_numTransmissions,
                              3,
                              "Unexpected number of transmissions for confirmed packet");
        NS_TEST_ASSERT_MSG_EQ(m_successfullyAcked,
                              false,
                              "Unexpected acknowledgment state for confirmed packet");
    }

    Reset();
    // Confirmed send retransmissions not interrupted after downlink without ACK
    {
        uint8_t nbTrans = 10;
        m_mac->SetMaxNumberOfTransmissions(nbTrans);
        m_mac->SetFType(LorawanMacHeader::CONFIRMED_DATA_UP);
        ScheduleRx1Downlink(Seconds(31.31));
        LoraFrameHeader fhdr;
        SendUplink(fhdr);
        NS_TEST_EXPECT_MSG_EQ(fhdr.GetFCnt(), 0, "Unexpected FCnt value in uplink FHDR");
        NS_TEST_ASSERT_MSG_EQ(m_mac->GetUplinkFrameCounter(),
                              1,
                              "Unexpected FCnt value in MAC layer");
        NS_TEST_ASSERT_MSG_EQ(m_phyStartSendingCalls,
                              nbTrans,
                              "Unexpected number of PHY layer StartSending calls");
        NS_TEST_ASSERT_MSG_EQ(m_phyReceivedPacketCalls,
                              1,
                              "Unexpected number of PHY layer ReceivedPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macSentNewPacketCalls,
                              1,
                              "Unexpected number of MAC layer SendNewPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macConfirmedTxOutcome,
                              1,
                              "Unexpected number of MAC layer ConfirmedTransmissionOutcome calls");
        NS_TEST_ASSERT_MSG_EQ(m_numTransmissions,
                              nbTrans,
                              "Unexpected number of transmissions for confirmed packet");
        NS_TEST_ASSERT_MSG_EQ(m_successfullyAcked,
                              false,
                              "Unexpected acknowledgment state for confirmed packet");
    }

    Reset();
    // Confirmed send retransmissions interrupted after downlink with ACK
    {
        uint8_t nbTrans = 14;
        m_mac->SetMaxNumberOfTransmissions(nbTrans);
        m_mac->SetFType(LorawanMacHeader::CONFIRMED_DATA_UP);
        ScheduleRx1Downlink(Seconds(41.28), true);
        LoraFrameHeader fhdr;
        SendUplink(fhdr);
        NS_TEST_EXPECT_MSG_EQ(fhdr.GetFCnt(), 0, "Unexpected FCnt value in uplink FHDR");
        NS_TEST_ASSERT_MSG_EQ(m_mac->GetUplinkFrameCounter(),
                              1,
                              "Unexpected FCnt value in MAC layer");
        NS_TEST_ASSERT_MSG_EQ(m_phyStartSendingCalls,
                              10,
                              "Unexpected number of PHY layer StartSending calls");
        NS_TEST_ASSERT_MSG_EQ(m_phyReceivedPacketCalls,
                              1,
                              "Unexpected number of PHY layer ReceivedPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macSentNewPacketCalls,
                              1,
                              "Unexpected number of MAC layer SendNewPacket calls");
        NS_TEST_ASSERT_MSG_EQ(m_macConfirmedTxOutcome,
                              1,
                              "Unexpected number of MAC layer ConfirmedTransmissionOutcome calls");
        NS_TEST_ASSERT_MSG_EQ(m_numTransmissions,
                              10,
                              "Unexpected number of transmissions for confirmed packet");
        NS_TEST_ASSERT_MSG_EQ(m_successfullyAcked,
                              true,
                              "Unexpected acknowledgment state for confirmed packet");
    }
}

/**
 * @ingroup lorawan
 *
 * The TestSuite class names the TestSuite, identifies what type of TestSuite, and enables the
 * TestCases to be run. Typically, only the constructor for this class must be defined
 */
class LorawanTestSuite : public TestSuite
{
  public:
    LorawanTestSuite(); //!< Default constructor
};

LorawanTestSuite::LorawanTestSuite()
    : TestSuite("lorawan", Type::UNIT)
{
    // LogComponentEnable("LorawanTestSuite", LOG_LEVEL_DEBUG);
    // LogComponentEnable("LorawanMac", LOG_LEVEL_DEBUG);
    // LogComponentEnable("EndDeviceLorawanMac", LOG_LEVEL_DEBUG);
    // LogComponentEnable("ClassAEndDeviceLorawanMac", LOG_LEVEL_DEBUG);
    // LogComponentEnable("SimpleEndDeviceLoraPhy", LOG_LEVEL_DEBUG);
    // LogComponentEnable("EndDeviceLoraPhy", LOG_LEVEL_DEBUG);
    // LogComponentEnable("LoraPhy", LOG_LEVEL_DEBUG);
    // LogComponentEnable("LoraChannel", LOG_LEVEL_DEBUG);
    // LogComponentEnable("LoraFrameHeader", LOG_LEVEL_DEBUG);
    // LogComponentEnableAll(LOG_PREFIX_FUNC);
    // LogComponentEnableAll(LOG_PREFIX_NODE);
    // LogComponentEnableAll(LOG_PREFIX_TIME);

    AddTestCase(new InterferenceTest, Duration::QUICK);
    AddTestCase(new AddressTest, Duration::QUICK);
    AddTestCase(new HeaderTest, Duration::QUICK);
    AddTestCase(new BeaconHeaderTest, Duration::QUICK);
    AddTestCase(new PingSlotInfoCommandTest, Duration::QUICK);
    AddTestCase(new FrameHeaderDirectionRequiredForCidTableTest, Duration::QUICK);
    AddTestCase(new PingSlotOffsetTest, Duration::QUICK);
    AddTestCase(new ReceivePathTest, Duration::QUICK);
    AddTestCase(new LogicalLoraChannelTest, Duration::QUICK);
    AddTestCase(new TimeOnAirTest, Duration::QUICK);
    AddTestCase(new PhyConnectivityTest, Duration::QUICK);
    AddTestCase(new ContinuousReceiveTest, Duration::QUICK);
    AddTestCase(new BeaconTransmissionTest, Duration::QUICK);
    AddTestCase(new BeaconDutyCycleGuardTest, Duration::QUICK);
    AddTestCase(new ClassBTest, Duration::QUICK);
    AddTestCase(new ClassCTest, Duration::QUICK);
    AddTestCase(new MacCommandTest, Duration::QUICK);
    AddTestCase(new AdrBackoffTest, Duration::QUICK);
    AddTestCase(new RetransmissionTest, Duration::QUICK);
}

// Do not forget to allocate an instance of this TestSuite
static LorawanTestSuite lorawanTestSuite;
