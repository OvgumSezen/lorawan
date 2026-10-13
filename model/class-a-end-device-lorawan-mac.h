/*
 * Copyright (c) 2017 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 *         Martina Capuzzo <capuzzom@dei.unipd.it>
 *
 * Modified by: Peggy Anderson <peggy.anderson@usask.ca>
 */

#ifndef CLASS_A_END_DEVICE_LORAWAN_MAC_H
#define CLASS_A_END_DEVICE_LORAWAN_MAC_H

#include "end-device-lorawan-mac.h"

namespace ns3
{
namespace lorawan
{

/**
 * @ingroup lorawan
 *
 * Class representing the MAC layer of a Class A LoRaWAN device.
 */
class ClassAEndDeviceLorawanMac : public EndDeviceLorawanMac
{
  public:
    /**
     *  Register this type.
     *  @return The object TypeId.
     */
    static TypeId GetTypeId();

    ClassAEndDeviceLorawanMac();           //!< Default constructor
    ~ClassAEndDeviceLorawanMac() override; //!< Destructor

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

    /**
     * Perform operations needed to open the first receive window.
     */
    void OpenFirstReceiveWindow();

    /**
     * Perform operations needed to open the second receive window.
     */
    void OpenSecondReceiveWindow();

    /**
     * Perform operations needed to close the first receive window.
     */
    void CloseFirstReceiveWindow();

    /**
     * Perform operations needed to close the second receive window.
     */
    void CloseSecondReceiveWindow();

    /**
     * Decide whether we can retransmit based on reception outcome.
     *
     * @param outcome Outcome of the reception.
     */
    void ManageRetransmissions(RxOutcome outcome);

    void Receive(Ptr<const Packet> packet) override;
    void FailedReception(Ptr<const Packet> packet) override;
    void OnRxParamSetupReq(uint8_t rx1DrOffset, uint8_t rx2DataRate, double frequencyHz) override;

    EventId m_secondReceiveWindow; //!< The event of the second receive window opening, used
                                   //!< to cancel the second window in case the first one is
                                   //!< successful.

    // Reception window parameters

    Time m_receiveDelay1; //!< The interval between when a packet is done sending and when the first
                          //!< receive window is opened.
    uint32_t m_firstReceiveWindowFrequencyHz; //!< The frequency [Hz] to listen on for the first
                                              //!< receive window. This value is set dynamically to
                                              //!< the last uplink transmission frequency.

    Time m_receiveDelay2; //!< The interval between when a packet is done sending and when the
                          //!< second receive window is opened.

    // Rescheduling purposes

    bool m_busy; //!< Whether the MAC layer is currently busy with in the LoRaWAN Class A process of
                 //!< transmitting an uplink packet and then opening two reception windows.
};

} /* namespace lorawan */
} /* namespace ns3 */

#endif /* CLASS_A_END_DEVICE_LORAWAN_MAC_H */
