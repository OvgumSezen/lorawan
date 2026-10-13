/*
 * Copyright (c) 2026 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Davide Magrin <magrinda@dei.unipd.it>
 *         Martina Capuzzo <capuzzom@dei.unipd.it>
 */

#include "lora-beacon-header.h"

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("LoraBeaconHeader");

LoraBeaconHeader::LoraBeaconHeader()
    : m_time(0)
{
}

LoraBeaconHeader::~LoraBeaconHeader()
{
}

TypeId
LoraBeaconHeader::GetTypeId()
{
    static TypeId tid =
        TypeId("LoraBeaconHeader").SetParent<Header>().AddConstructor<LoraBeaconHeader>();
    return tid;
}

TypeId
LoraBeaconHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
LoraBeaconHeader::GetSerializedSize() const
{
    NS_LOG_FUNCTION_NOARGS();

    return 4; // The Time field, 32 bits
}

void
LoraBeaconHeader::Serialize(Buffer::Iterator start) const
{
    NS_LOG_FUNCTION_NOARGS();

    start.WriteHtonU32(m_time);
}

uint32_t
LoraBeaconHeader::Deserialize(Buffer::Iterator start)
{
    NS_LOG_FUNCTION_NOARGS();

    m_time = start.ReadNtohU32();

    return 4; // the number of bytes consumed
}

void
LoraBeaconHeader::Print(std::ostream& os) const
{
    os << "Time=" << m_time;
}

void
LoraBeaconHeader::SetTime(uint32_t timeSecs)
{
    NS_LOG_FUNCTION(this << timeSecs);

    m_time = timeSecs;
}

uint32_t
LoraBeaconHeader::GetTime() const
{
    NS_LOG_FUNCTION_NOARGS();

    return m_time;
}

} // namespace lorawan
} // namespace ns3
