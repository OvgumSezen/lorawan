/*
 * Copyright (c) 2026 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 *         Martina Capuzzo <capuzzom@dei.unipd.it>
 */

#ifndef CLASS_C_END_DEVICE_LORAWAN_MAC_H
#define CLASS_C_END_DEVICE_LORAWAN_MAC_H

#include "end-device-lorawan-mac.h"

namespace ns3
{
namespace lorawan
{

/**
 * @ingroup lorawan
 *
 * Class representing the MAC layer of a Class C LoRaWAN device.
 *
 * Like Class A devices, Class C devices open a first and second receive window (RX1, RX2) after
 * each uplink transmission to allow the network server to reply. Unlike Class A, a Class C device
 * keeps listening on the RX2 parameters continuously whenever it is otherwise idle (i.e., outside
 * of an uplink's RX1 window and outside of an ongoing transmission), instead of going to sleep
 * once the RX1/RX2 sequence concludes. This allows the network server to deliver a downlink to the
 * device at any time, not just right after one of its own uplinks.
 */
class ClassCEndDeviceLorawanMac : public EndDeviceLorawanMac
{
  public:
    /**
     *  Register this type.
     *  @return The object TypeId.
     */
    static TypeId GetTypeId();

    ClassCEndDeviceLorawanMac();           //!< Default constructor
    ~ClassCEndDeviceLorawanMac() override; //!< Destructor

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
    void Receive(Ptr<const Packet> packet) override;
    void FailedReception(Ptr<const Packet> packet) override;
    void OnRxParamSetupReq(uint8_t rx1DrOffset, uint8_t rx2DataRate, double frequencyHz) override;

    /**
     * Perform operations needed to open the first receive window.
     */
    void OpenFirstReceiveWindow();

    /**
     * Perform operations needed to close the first receive window.
     */
    void CloseFirstReceiveWindow();

    /**
     * Perform operations needed to open the second receive window.
     */
    void OpenSecondReceiveWindow();

    /**
     * Perform operations needed to close the second receive window.
     *
     * Unlike Class A, absence of a reply at this point does not put the device to sleep: it falls
     * through to EnterContinuousListening() instead.
     */
    void CloseSecondReceiveWindow();

    /**
     * Decide whether we can retransmit based on reception outcome.
     *
     * @param outcome Outcome of the reception.
     */
    void ManageRetransmissions(RxOutcome outcome);

    /**
     * Start (or resume) continuous listening on the RX2 parameters. This is the idle steady
     * state of a Class C device: outside of the TX/RX1/RX2 sequence following one of this
     * device's own uplinks, the PHY is always either transmitting or continuously listening.
     */
    void EnterContinuousListening();

    EventId m_secondReceiveWindow; //!< The event of the second receive window opening, used
                                   //!< to cancel the second window in case the first one is
                                   //!< successful.

    // Reception window parameters (RX1DROffset and RX2 parameters are inherited from
    // EndDeviceLorawanMac, shared with ClassAEndDeviceLorawanMac)

    Time m_receiveDelay1; //!< The interval between when a packet is done sending and when the first
                          //!< receive window is opened.
    uint32_t m_firstReceiveWindowFrequencyHz; //!< The frequency [Hz] to listen on for the first
                                              //!< receive window. This value is set dynamically to
                                              //!< the last uplink transmission frequency.

    Time m_receiveDelay2; //!< The interval between when a packet is done sending and when the
                          //!< second receive window is opened.

    // Rescheduling purposes

    bool m_busy; //!< Whether the MAC layer is currently busy with in the LoRaWAN Class C process of
                 //!< transmitting an uplink packet and then opening two reception windows.

    bool m_listeningContinuously; //!< Whether the PHY is currently listening on the RX2 parameters
                                  //!< in continuous (RXCONTINUOUS) mode, i.e., outside of the
                                  //!< TX/RX1/RX2 sequence following one of this device's own
                                  //!< uplinks. This is this class's idle steady state.
};

} /* namespace lorawan */
} /* namespace ns3 */

#endif /* CLASS_C_END_DEVICE_LORAWAN_MAC_H */
