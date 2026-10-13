/*
 * Copyright (c) 2026 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 *         Martina Capuzzo <capuzzom@dei.unipd.it>
 */

#ifndef LORA_BEACON_HEADER_H
#define LORA_BEACON_HEADER_H

#include "ns3/header.h"

namespace ns3
{
namespace lorawan
{

/**
 * @ingroup lorawan
 *
 * This class represents the payload of a Class B beacon frame.
 *
 * Real LoRaWAN beacons are not LoRaWAN MAC frames: they carry no MHDR/FHDR and are
 * transmitted with a fixed, implicit-header PHY configuration on a dedicated beacon
 * channel. This class models a simplified beacon payload that only carries the one field
 * an end device actually needs in order to make use of a beacon: the network time at
 * which it was sent. Other real-world beacon fields (GwSpecific info, the redundant CRC
 * blocks) are not modeled, since nothing in this module consumes them.
 */
class LoraBeaconHeader : public Header
{
  public:
    LoraBeaconHeader();           //!< Default constructor
    ~LoraBeaconHeader() override; //!< Destructor

    /**
     *  Register this type.
     *  @return The object TypeId.
     */
    static TypeId GetTypeId();

    // Methods inherited from Header

    TypeId GetInstanceTypeId() const override;

    /**
     * Return the size required for serialization of this header.
     *
     * @return The serialized size in bytes.
     */
    uint32_t GetSerializedSize() const override;

    /**
     * Serialize the header.
     *
     * @param start A pointer to the buffer that will be filled with the serialization.
     */
    void Serialize(Buffer::Iterator start) const override;

    /**
     * Deserialize the contents of the buffer into a LoraBeaconHeader object.
     *
     * @param start A pointer to the buffer we need to deserialize.
     * @return The number of consumed bytes.
     */
    uint32_t Deserialize(Buffer::Iterator start) override;

    /**
     * Print the header in a human-readable format.
     *
     * @param os The std::ostream on which to print the header.
     */
    void Print(std::ostream& os) const override;

    /**
     * Set the Time field.
     *
     * @param timeSecs The network time [s] this beacon is sent at, i.e., the boundary of the
     * beacon period it opens.
     */
    void SetTime(uint32_t timeSecs);

    /**
     * Get the Time field.
     *
     * @return The network time [s] this beacon was sent at.
     */
    uint32_t GetTime() const;

  private:
    uint32_t m_time; //!< The Time field: network time [s] this beacon was sent at
};

} // namespace lorawan
} // namespace ns3

#endif /* LORA_BEACON_HEADER_H */
