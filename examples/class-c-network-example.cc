/*
 * Copyright (c) 2026 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 */

/*
 * This example builds a network mixing Class A, Class B, and Class C end devices, all sending
 * periodic uplinks and all receiving periodic server-initiated downlinks (reactively for Class A,
 * via NetworkStatus::SendDownlinkToDevice() for Class B/C -- delivered at the device's next ping
 * slot for Class B, immediately for Class C's continuous listening), in order to observe and
 * compare downlink delivery latency/success rate and end-device energy consumption across all
 * three classes under identical offered traffic.
 */

#include "ns3/basic-energy-source-helper.h"
#include "ns3/core-module.h"
#include "ns3/lorawan-module.h"
#include "ns3/mobility-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/simple-ref-count.h"

#include <array>
#include <deque>
#include <numeric>

using namespace ns3;
using namespace lorawan;
using namespace energy;

NS_LOG_COMPONENT_DEFINE("ClassCNetworkExample");

/**
 * The three LoRaWAN device classes this example mixes together, used throughout to key
 * per-class statistics (delivery latency, energy draw).
 */
enum class DeviceClass
{
    A, //!< Class A: reactive-only downlink reception
    B, //!< Class B: downlink reception at scheduled ping slots
    C  //!< Class C: continuous downlink reception
};

/**
 * @param deviceClass The device class.
 * @return A single letter identifying deviceClass ('A', 'B', or 'C'), for print output.
 */
char
DeviceClassLetter(DeviceClass deviceClass)
{
    switch (deviceClass)
    {
    case DeviceClass::A:
        return 'A';
    case DeviceClass::B:
        return 'B';
    case DeviceClass::C:
        return 'C';
    }
    NS_ABORT_MSG("Unreachable: unknown DeviceClass value");
}

/**
 * Determine an end device MAC's DeviceClass by its concrete type.
 *
 * @param edMac The end device's MAC layer.
 * @return The device's class.
 */
DeviceClass
GetDeviceClass(Ptr<EndDeviceLorawanMac> edMac)
{
    if (DynamicCast<ClassCEndDeviceLorawanMac>(edMac))
    {
        return DeviceClass::C;
    }
    if (DynamicCast<ClassBEndDeviceLorawanMac>(edMac))
    {
        return DeviceClass::B;
    }
    return DeviceClass::A;
}

/**
 * Tracks server-initiated ("spontaneous") downlinks pushed to a single end device, matching each
 * arrival at the device back to the push that produced it (FIFO), to compute delivery latency and
 * keep an exact attempted/delivered/pending count.
 */
class DownlinkTracker : public SimpleRefCount<DownlinkTracker>
{
  public:
    /**
     * Constructor.
     *
     * @param deviceClass The class of the tracked device.
     */
    explicit DownlinkTracker(DeviceClass deviceClass)
        : m_deviceClass(deviceClass)
    {
    }

    /**
     * Record that a downlink was just pushed to the tracked device.
     */
    void NotePush()
    {
        m_pendingPushTimes.push_back(Simulator::Now());
    }

    /**
     * Callback registered as the tracked device MAC's ReceiveCallback.
     *
     * @param packet The received packet.
     */
    void OnReceived(Ptr<Packet> packet)
    {
        if (m_pendingPushTimes.empty())
        {
            // Not a downlink we pushed (shouldn't happen in this example: Class A/B devices only
            // ever receive what this example pushes to them, and Class C's bootstrap uplinks are
            // unconfirmed, so they don't produce a reply either).
            return;
        }
        Time pushTime = m_pendingPushTimes.front();
        m_pendingPushTimes.pop_front();
        m_latencies.push_back(Simulator::Now() - pushTime);
    }

    /**
     * @return The class of the tracked device.
     */
    DeviceClass GetDeviceClass() const
    {
        return m_deviceClass;
    }

    /**
     * @return The total number of downlinks pushed to the tracked device.
     */
    uint32_t GetAttempts() const
    {
        return m_latencies.size() + m_pendingPushTimes.size();
    }

    /**
     * @return The number of pushed downlinks still awaiting delivery.
     */
    uint32_t GetPending() const
    {
        return m_pendingPushTimes.size();
    }

    /**
     * @return The delivery latency of each pushed downlink that has been delivered so far.
     */
    const std::vector<Time>& GetLatencies() const
    {
        return m_latencies;
    }

  private:
    DeviceClass m_deviceClass;           //!< The class of the tracked device
    std::deque<Time> m_pendingPushTimes; //!< Push times of downlinks not yet delivered (FIFO)
    std::vector<Time> m_latencies;       //!< Delivery latency of each delivered downlink
};

/**
 * Push one downlink to every tracked device, then reschedule itself.
 *
 * @param status The NetworkStatus to push the downlinks through.
 * @param addresses The address of each tracked device.
 * @param trackers The DownlinkTracker associated with each tracked device (same order/size as
 * addresses).
 * @param period The interval at which to repeat.
 */
void
PushDownlinks(Ptr<NetworkStatus> status,
              std::vector<LoraDeviceAddress> addresses,
              std::vector<Ptr<DownlinkTracker>> trackers,
              Time period)
{
    for (std::size_t i = 0; i < addresses.size(); ++i)
    {
        status->SendDownlinkToDevice(addresses.at(i), Create<Packet>(5));
        trackers.at(i)->NotePush();
    }
    Simulator::Schedule(period, &PushDownlinks, status, addresses, trackers, period);
}

int
main(int argc, char* argv[])
{
    uint32_t nDevices = 6;
    // Default to an even three-way split (was 0.5/0/rest before Class B support was added): the
    // point of this example is now precisely the three-way comparison, so that should be visible
    // without needing to pass any extra flags.
    double fractionClassB = 1.0 / 3.0;
    double fractionClassC = 1.0 / 3.0;
    uint32_t nGateways = 1;
    Time simulationTime = Minutes(10);
    Time appPeriod = Seconds(60);
    // Kept longer than appPeriod by default: EndDeviceStatus holds a single reply slot (not a
    // queue), so pushing faster than a Class A device's own uplink cadence would silently
    // overwrite an undelivered reply with the next one before the device ever picks it up
    // reactively -- starving Class A rather than meaningfully comparing it against Class C.
    Time downlinkPeriod = Seconds(90);
    bool verbose = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nDevices", "Number of end devices to create", nDevices);
    cmd.AddValue("fractionClassB",
                 "Fraction (0-1) of end devices configured as Class B",
                 fractionClassB);
    cmd.AddValue("fractionClassC",
                 "Fraction (0-1) of end devices configured as Class C",
                 fractionClassC);
    cmd.AddValue("nGateways", "Number of gateways to create", nGateways);
    cmd.AddValue("simulationTime", "Total duration of the simulation", simulationTime);
    cmd.AddValue("appPeriod", "Period of each end device's periodic uplink application", appPeriod);
    cmd.AddValue("downlinkPeriod",
                 "Period at which the server pushes a downlink to every device",
                 downlinkPeriod);
    cmd.AddValue("verbose", "Whether to print detailed logs", verbose);
    cmd.Parse(argc, argv);

    NS_ABORT_MSG_IF(fractionClassB < 0.0 || fractionClassC < 0.0 ||
                        fractionClassB + fractionClassC > 1.0,
                    "fractionClassB and fractionClassC must each be >= 0 and sum to <= 1 (the "
                    "remainder is Class A)");

    if (verbose)
    {
        LogComponentEnable("ClassCNetworkExample", LOG_LEVEL_ALL);
        LogComponentEnableAll(LOG_PREFIX_FUNC);
        LogComponentEnableAll(LOG_PREFIX_NODE);
        LogComponentEnableAll(LOG_PREFIX_TIME);
    }

    // Create a simple wireless channel
    ///////////////////////////////////

    Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel>();
    loss->SetPathLossExponent(3.76);
    loss->SetReference(1, 7.7);

    Ptr<PropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel>();

    Ptr<LoraChannel> channel = CreateObject<LoraChannel>(loss, delay);

    // Helpers
    //////////

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator",
                                  "rho",
                                  DoubleValue(1000),
                                  "X",
                                  DoubleValue(0.0),
                                  "Y",
                                  DoubleValue(0.0));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    LoraPhyHelper phyHelper = LoraPhyHelper();
    phyHelper.SetChannel(channel);

    LoraHelper helper = LoraHelper();

    // Create end devices: split nDevices between Class A, Class B, and Class C
    /////////////////////////////////////////////////////////////////////////

    uint32_t nClassC = static_cast<uint32_t>(fractionClassC * nDevices);
    uint32_t nClassB = static_cast<uint32_t>(fractionClassB * nDevices);
    uint32_t nClassA = nDevices - nClassC - nClassB;

    NodeContainer classADevices;
    classADevices.Create(nClassA);
    mobility.Install(classADevices);

    NodeContainer classBDevices;
    classBDevices.Create(nClassB);
    mobility.Install(classBDevices);

    NodeContainer classCDevices;
    classCDevices.Create(nClassC);
    mobility.Install(classCDevices);

    Ptr<LoraDeviceAddressGenerator> addrGen = CreateObject<LoraDeviceAddressGenerator>();

    LorawanMacHelper macHelper = LorawanMacHelper();
    macHelper.SetAddressGenerator(addrGen);
    macHelper.SetRegion(LorawanMacHelper::EU);

    phyHelper.SetDeviceType(LoraPhyHelper::ED);
    macHelper.SetDeviceType(LorawanMacHelper::ED_A);
    NetDeviceContainer classADeviceNetDevices = helper.Install(phyHelper, macHelper, classADevices);

    macHelper.SetDeviceType(LorawanMacHelper::ED_B);
    NetDeviceContainer classBDeviceNetDevices = helper.Install(phyHelper, macHelper, classBDevices);

    macHelper.SetDeviceType(LorawanMacHelper::ED_C);
    NetDeviceContainer classCDeviceNetDevices = helper.Install(phyHelper, macHelper, classCDevices);

    NodeContainer endDevices;
    endDevices.Add(classADevices);
    endDevices.Add(classBDevices);
    endDevices.Add(classCDevices);

    // Give every device periodic uplink traffic. For Class C this doubles as the "bootstrap"
    // uplink that lets it fall through to continuous listening (see the Known
    // limitations in doc/lorawan.rst); Class B needs no such bootstrap for its own beacon/ping-slot schedule (that
    // starts unconditionally at device construction), but, like every other class here, still
    // needs to have sent at least one uplink before the server can push it anything -- that is
    // how NetworkStatus::GetBestGatewayForDevice() learns which gateway(s) can reach it at all.
    PeriodicSenderHelper periodicSenderHelper;
    periodicSenderHelper.SetPeriod(appPeriod);
    periodicSenderHelper.Install(endDevices);

    // Create gateways
    //////////////////

    NodeContainer gateways;
    gateways.Create(nGateways);
    mobility.Install(gateways);

    phyHelper.SetDeviceType(LoraPhyHelper::GW);
    macHelper.SetDeviceType(LorawanMacHelper::GW);
    helper.Install(phyHelper, macHelper, gateways);

    // Class B devices need a beacon to synchronize to; no point broadcasting one if none are
    // present.
    if (nClassB > 0)
    {
        macHelper.EnableBeaconing(gateways);
    }

    // Raise data rates from the SF12 (DR0) default: at SF12, a device's duty-cycle "off time"
    // after a single transmission can easily exceed this example's whole simulation time,
    // preventing it from ever sending a second uplink. A fixed rate is used for every device,
    // regardless of class -- rather than LorawanMacHelper::SetSpreadingFactorsUp()'s per-device,
    // distance-based assignment (which also can't be used on Class B/C devices to begin with: it
    // DynamicCasts to ClassAEndDeviceLorawanMac internally, same Class-A-only limitation noted in
    // test/utilities.cc). This keeps the three-way class comparison controlled: distance-driven
    // SF heterogeneity turned out to be a large enough source of per-device RX-window-duration
    // variance (RX1 always mirrors the device's own uplink SF) to occasionally invert the
    // Class A vs. Class B energy comparison below under some random topologies/seeds -- a
    // SF-driven artifact, not a real class-driven effect, and not what this example is meant to
    // isolate. (Class B's ping slots are unaffected either way -- they always use the fixed
    // beacon channel data rate, regardless of the device's own uplink data rate.)
    for (auto it = endDevices.Begin(); it != endDevices.End(); ++it)
    {
        Ptr<EndDeviceLorawanMac> mac = DynamicCast<EndDeviceLorawanMac>(
            DynamicCast<LoraNetDevice>((*it)->GetDevice(0))->GetMac());
        mac->SetDataRate(5); // SF7
    }

    // Create the network server
    /////////////////////////////

    Ptr<Node> networkServerNode = CreateObject<Node>();

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    P2PGwRegistration_t gwRegistration;
    for (auto gw = gateways.Begin(); gw != gateways.End(); ++gw)
    {
        auto container = p2p.Install(networkServerNode, *gw);
        auto serverP2PNetDev = DynamicCast<PointToPointNetDevice>(container.Get(0));
        gwRegistration.emplace_back(serverP2PNetDev, *gw);
    }

    NetworkServerHelper networkServerHelper;
    networkServerHelper.SetGatewaysP2P(gwRegistration);
    networkServerHelper.SetEndDevices(endDevices);
    ApplicationContainer networkServerApps = networkServerHelper.Install(networkServerNode);

    ForwarderHelper forwarderHelper;
    forwarderHelper.Install(gateways);

    // Install the energy model on every end device (as lorawan-energy-model-example.cc does)
    ////////////////////////////////////////////////////////////////////////////////////////

    NetDeviceContainer endDeviceNetDevices;
    endDeviceNetDevices.Add(classADeviceNetDevices);
    endDeviceNetDevices.Add(classBDeviceNetDevices);
    endDeviceNetDevices.Add(classCDeviceNetDevices);

    BasicEnergySourceHelper basicSourceHelper;
    basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(10000));
    basicSourceHelper.Set("BasicEnergySupplyVoltageV", DoubleValue(3.3));
    EnergySourceContainer energySources = basicSourceHelper.Install(endDevices);

    LoraRadioEnergyModelHelper radioEnergyHelper;
    radioEnergyHelper.Set("SleepCurrentA", DoubleValue(0.1e-6));   // 0.1uA
    radioEnergyHelper.Set("StandbyCurrentA", DoubleValue(1.4e-3)); // 1.4mA
    radioEnergyHelper.Set("RxCurrentA", DoubleValue(10.8e-3));     // 10.8mA
    radioEnergyHelper.SetTxCurrentModel("ns3::LinearLoraTxCurrentModel",
                                        "Eta",
                                        DoubleValue(0.452750),
                                        "Voltage",
                                        DoubleValue(3.3),
                                        "BaseCurrent",
                                        DoubleValue(0.014646));
    radioEnergyHelper.Install(endDeviceNetDevices, energySources);

    // Track and periodically push server-initiated downlinks
    ///////////////////////////////////////////////////////////

    std::vector<LoraDeviceAddress> trackedAddresses;
    std::vector<Ptr<DownlinkTracker>> trackers;
    for (auto it = endDevices.Begin(); it != endDevices.End(); ++it)
    {
        Ptr<EndDeviceLorawanMac> edMac = DynamicCast<EndDeviceLorawanMac>(
            DynamicCast<LoraNetDevice>((*it)->GetDevice(0))->GetMac());

        auto tracker = Create<DownlinkTracker>(GetDeviceClass(edMac));
        edMac->SetReceiveCallback(MakeCallback(&DownlinkTracker::OnReceived, tracker));

        trackedAddresses.push_back(edMac->GetDeviceAddress());
        trackers.push_back(tracker);
    }

    Ptr<NetworkStatus> networkStatus =
        DynamicCast<NetworkServer>(networkServerApps.Get(0))->GetNetworkStatus();

    // Give devices one appPeriod to complete their bootstrap uplink/ack cycle (see the Known
    // limitations note above) before the first push.
    Simulator::Schedule(appPeriod,
                        &PushDownlinks,
                        networkStatus,
                        trackedAddresses,
                        trackers,
                        downlinkPeriod);

    // Start simulation
    ////////////////////

    Simulator::Stop(simulationTime);
    Simulator::Run();

    // Report downlink delivery statistics
    ///////////////////////////////////////

    // Per-class aggregated statistics, indexed by static_cast<size_t>(DeviceClass).
    struct ClassStats
    {
        uint32_t attempts = 0;          //!< Total downlinks pushed to devices of this class
        uint32_t pending = 0;           //!< Of those, still awaiting delivery
        std::vector<double> latenciesS; //!< Delivery latency [s] of each delivered downlink
        std::vector<double> consumedJ;  //!< Energy [J] consumed by each device of this class
    };

    std::array<ClassStats, 3> stats;

    for (const auto& tracker : trackers)
    {
        ClassStats& s = stats.at(static_cast<std::size_t>(tracker->GetDeviceClass()));
        s.attempts += tracker->GetAttempts();
        s.pending += tracker->GetPending();
        for (const auto& latency : tracker->GetLatencies())
        {
            s.latenciesS.push_back(latency.GetSeconds());
        }
    }

    // Sanity check: every pushed downlink is accounted for as either delivered or still pending
    // (DownlinkTracker guarantees this by construction; the check below guards against a change
    // elsewhere in this file breaking that invariant, e.g. double-counting a device).
    for (const auto& s : stats)
    {
        NS_ASSERT_MSG(s.latenciesS.size() + s.pending == s.attempts,
                      "Downlink delivery accounting is inconsistent");
    }

    auto average = [](const std::vector<double>& v) -> double {
        return v.empty() ? 0.0 : std::accumulate(v.begin(), v.end(), 0.0) / v.size();
    };

    std::cout << "Downlink delivery statistics" << std::endl;
    for (std::size_t i = 0; i < stats.size(); ++i)
    {
        std::cout << "  Class " << DeviceClassLetter(static_cast<DeviceClass>(i)) << ": "
                  << stats[i].latenciesS.size() << "/" << stats[i].attempts << " delivered ("
                  << stats[i].pending << " pending), average latency "
                  << average(stats[i].latenciesS) << " s" << std::endl;
    }

    // Report energy consumption statistics
    /////////////////////////////////////////

    for (std::size_t i = 0; i < energySources.GetN(); ++i)
    {
        Ptr<EnergySource> source = energySources.Get(i);
        double consumedJ = source->GetInitialEnergy() - source->GetRemainingEnergy();
        stats.at(static_cast<std::size_t>(trackers.at(i)->GetDeviceClass()))
            .consumedJ.push_back(consumedJ);
    }

    // Average current draw [A] = average consumed energy [J] / (supply voltage [V] * simulation
    // time [s]); all devices share the same voltage and simulation duration, so this is directly
    // proportional to (and a more directly interpretable form of) the consumed energy above.
    double supplyVoltageV = 3.3;
    auto averageCurrentMa = [&](const std::vector<double>& consumedJ) -> double {
        return average(consumedJ) / (supplyVoltageV * simulationTime.GetSeconds()) * 1000;
    };

    std::cout << "Energy consumption statistics" << std::endl;
    for (std::size_t i = 0; i < stats.size(); ++i)
    {
        std::cout << "  Class " << DeviceClassLetter(static_cast<DeviceClass>(i)) << ": average "
                  << average(stats[i].consumedJ) << " J consumed, "
                  << averageCurrentMa(stats[i].consumedJ) << " mA average current draw"
                  << std::endl;
    }

    const ClassStats& classAStats = stats.at(static_cast<std::size_t>(DeviceClass::A));
    const ClassStats& classBStats = stats.at(static_cast<std::size_t>(DeviceClass::B));
    const ClassStats& classCStats = stats.at(static_cast<std::size_t>(DeviceClass::C));

    // Sanity check: continuous RX2 listening should make Class C devices draw noticeably more
    // current on average than Class A devices under the same traffic pattern.
    if (!classAStats.consumedJ.empty() && !classCStats.consumedJ.empty())
    {
        NS_ASSERT_MSG(average(classCStats.consumedJ) > average(classAStats.consumedJ),
                      "Expected Class C devices to consume more energy than Class A devices, "
                      "due to continuously listening on RX2");
    }

    // Sanity check: Class B's ping slots are far cheaper than Class C's always-on receiver, so
    // this margin is large and robust (unlike the Class A comparison below, it never came close
    // to flipping across the many random topologies/seeds this was checked against).
    if (!classBStats.consumedJ.empty() && !classCStats.consumedJ.empty())
    {
        NS_ASSERT_MSG(average(classCStats.consumedJ) > average(classBStats.consumedJ),
                      "Expected Class C devices to consume more energy than Class B devices, "
                      "due to continuously listening on RX2 rather than briefly at ping slots");
    }

    // NOTE on what's deliberately *not* asserted here: Class B's own RX1/RX2 ack cycle (shared
    // with Class A) already dominates its energy draw; its ping slots are individually brief
    // enough that their added cost is a comparatively small increment on top of that shared
    // baseline. Because of that, "Class B draws more current than Class A" -- while true on
    // average and visible above in most runs -- is not a robust per-run invariant: sweeping
    // --RngRun and --nDevices repeatedly produced runs (with everything else held fixed,
    // including a uniform data rate across classes) where the two came out essentially tied, or
    // even briefly inverted, purely from which random topology/uplink-phase each individual
    // device happened to draw. Asserting a strict ordering here would make this example flaky
    // for no real benefit; the two averages are still printed above for direct comparison.

    Simulator::Destroy();

    return 0;
}
