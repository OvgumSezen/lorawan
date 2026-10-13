# Class B and Class C support

The upstream module only simulates Class A end devices. I needed Class B and Class C devices
for my thesis, so I added both. This note covers what was added, the main design choices, and
what is still missing. The user-facing documentation is in `doc/lorawan.rst`.

## Class C

A Class C device keeps its receiver open whenever it is not transmitting, so the network
server can reach it at almost any time. The price is energy: the radio is almost never asleep.

**PHY.** `EndDeviceLoraPhy` now has a real continuous receive mode (`ReceiveContinuous()` and
`StopReceiveContinuous()`). Another option was to chain short receive windows back to back at
the MAC layer, but that creates a new event every few symbols for every idle device, which does
not scale to long runs. The continuous mode is a single PHY state. It also takes care of the
`RXCONTINUOUS` TODOs that were already in the PHY header. Nothing that Class A or the gateway
uses was changed.

**MAC.** `ClassCEndDeviceLorawanMac` sits next to `ClassAEndDeviceLorawanMac`. It does not
inherit from it. After the usual RX1/RX2 windows it goes back to listening on the RX2 channel,
and it pauses listening only to transmit. The RX2 frequency and data rate fields moved up to
`EndDeviceLorawanMac`, so the helper can configure both classes in the same way.

**Network server.** Three changes:

1. Device registration (`EndDeviceStatus`, `NetworkStatus::AddNode`, `NetworkServer::AddNode`)
   now takes the generic `EndDeviceLorawanMac` instead of the Class A type.
2. If no gateway is free to send a reply in RX1 or RX2, the server used to drop the reply. For
   a Class C device it now keeps retrying, since the device is still listening.
3. A new `NetworkStatus::SendDownlinkToDevice()` sends a downlink that is not a reply to an
   uplink. A Class C device gets it as soon as a gateway is free. A Class A device gets it at
   its next receive window, which is how real Class A behaves.

## Class B

Class B sits between A and C. Gateways broadcast a beacon every 128 s, which gives devices a
shared time reference. A synchronised device then opens short "ping slots" at times the server
can also compute, so downlinks arrive with bounded delay without the device listening all the
time.

**Beacons.** `LoraBeaconHeader` is the beacon frame. Gateways send it on 869.525 MHz at SF9
when `BeaconEnabled` is set on `GatewayLorawanMac`. Beaconing is off by default, so existing
scenarios behave exactly as before. Beacons are ordinary transmissions: they use the gateway's
duty cycle, and devices can miss them because of interference, path loss or their own
activity. In all three supported regions, 869.525 MHz was already the RX2 frequency, so no new
sub-band was needed.

**Synchronisation.** A `ClassBEndDeviceLorawanMac` starts looking for a beacon as soon as it
is deployed. It becomes synchronised when it receives one, and loses synchronisation after
missing two in a row. While unsynchronised it does not open ping slots, but it keeps listening
for the next beacon.

**Ping slots.** The periodicity attribute (0–7, default 4) sets the number of slots per beacon
period to `2^(7 - periodicity)`. The default gives 8 slots, one every 16 s. Each device's
offset within the period is pseudo-random and is computed the same way by the device and the
server. The device reports its periodicity to the server with `PingSlotInfoReq`.

**Collisions with the device's own traffic.** If a beacon or ping-slot window comes up while
the device is in the middle of its own uplink and RX1/RX2 sequence, the window is skipped and
a trace source counts the skip. If the application wants to send while a window is open, the
uplink waits a little. Both rules live in the MAC, so the PHY did not change for Class B.

**Network server.** `SendDownlinkToDevice()` works out the device's next ping slot and
schedules the downlink for that moment. If no gateway is free, it tries again at the slot after
that, rather than retrying every second as for Class C.

## Where I depart from the specification

- The ping-slot offset uses ns-3's `Hash32` over the device address and the beacon time
  instead of the AES-128 construction in the spec. Device and server still compute the same
  value, and it is still spread evenly over the period, so network behaviour and energy use do
  not change.
- Slot timing is computed directly as a time value, not in the spec's "4096 units per beacon
  period".
- Clock drift is not modelled, because ns-3 gives every node the exact time. A device only
  loses synchronisation by missing beacons.

## Configuration

`LorawanMacHelper` has two new device types, `ED_B` and `ED_C`, next to `ED_A`. A device keeps
its class for the whole simulation. Like Class A, both new classes support the EU868,
SingleChannel and ALOHA regions.

`examples/class-c-network-example.cc` builds a mixed network of Class A, B and C devices.
It pushes downlinks to them and prints delivery delay and energy for each class. With the
default settings (a ten-minute run), a Class C device used about 3.5 J against about 0.14 J for Class A. I had
expected Class B to always land between the two, but at the default periodicity it sometimes
used less energy than Class A, because ping slots are very cheap. The example therefore only
checks that Class C uses more energy than Class B.

## Testing

New test cases in `test/lorawan-test-suite.cc` cover the continuous receive mode, the Class C
and Class B MACs, the beacon header, the `PingSlotInfoReq`/`Ans` commands, ping-slot offsets,
beacon transmission, and a gateway that cannot send a beacon because of its duty cycle. The
network server and network status suites have new cases for registering Class B/C devices and
for `SendDownlinkToDevice()`. The example also runs as part of `test/examples-to-run.py`.

## Upstream fixes made along the way

- `NetworkStatus::GetEndDeviceStatus()` did not set the uplink/downlink direction on the frame
  header, which crashed `LoraFrameHeader::Deserialize()`. Two similar missing calls were fixed
  too, as was a wrong command type in `DlChannelAns`.
- `EndDeviceStatus::GetPowerGatewayMap()` crashed for devices that had never sent an uplink.
- `logical-lora-channel.cc` was missing `#include "ns3/log.h"`.
- Some tests left events in the simulator, which then leaked into later test cases.

## Not implemented

- No join procedure. Because of this, a new Class C device only starts listening after its
  first own uplink and RX1/RX2 sequence. Class B does not have this problem because it
  synchronises on beacons.
- `SendDownlinkToDevice()` has no retry limit for Class C. A downlink to a device that never
  becomes reachable is retried every second until the simulation ends.
- `PingSlotChannelReq`, `BeaconFreqReq` and `DeviceModeInd` are missing, so ping-slot
  frequency and data rate stay at their defaults, and a device cannot change class during a
  run.
- The missed-beacon limit is fixed at 2 and cannot be configured.
- Like the existing Class A code, the server looks up some device settings (here the ping-slot
  periodicity) straight from the device's MAC object instead of learning them over the air.
