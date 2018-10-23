/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 University of Campinas (Unicamp)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Luciano Chaves <luciano@lrc.ic.unicamp.br>
 */

#include <iomanip>
#include <iostream>
#include "traffic-stats-calculator.h"
#include "applications/svelte-client.h"

using namespace std;

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TrafficStatsCalculator");
NS_OBJECT_ENSURE_REGISTERED (TrafficStatsCalculator);

TrafficStatsCalculator::TrafficStatsCalculator ()
{
  NS_LOG_FUNCTION (this);

  // Connect this stats calculator to required trace sources.

  // Config::Connect (
  //   "/NodeList/*/$ns3::OFSwitch13Device/LoadDrop",//FIXME overload
  //   MakeCallback (&TrafficStatsCalculator::OverloadDropPacket, this));
  // Config::Connect (
  //   "/NodeList/*/$ns3::OFSwitch13Device/MeterDrop",
  //   MakeCallback (&TrafficStatsCalculator::MeterDropPacket, this));
  // Config::Connect (
  //   "/NodeList/*/$ns3::OFSwitch13Device/PortList/*/PortQueue/Drop",
  //   MakeCallback (&TrafficStatsCalculator::QueueDropPacket, this));
  // Config::Connect (
  //   "/NodeList/*/ApplicationList/*/$ns3::SvelteClient/AppStart",
  //   MakeCallback (&TrafficStatsCalculator::ResetCounters, this));

  Config::Connect (
    "/NodeList/*/ApplicationList/*/$ns3::SvelteClient/AppStop",
    MakeCallback (&TrafficStatsCalculator::DumpStatistics, this));
  Config::Connect (
    "/NodeList/*/ApplicationList/*/$ns3::SvelteClient/AppError",
    MakeCallback (&TrafficStatsCalculator::DumpStatistics, this));
}

TrafficStatsCalculator::~TrafficStatsCalculator ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
TrafficStatsCalculator::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TrafficStatsCalculator")
    .SetParent<Object> ()
    .AddConstructor<TrafficStatsCalculator> ()
    .AddAttribute ("AppStatsFilename",
                   "Filename for L7 traffic application QoS statistics.",
                   StringValue ("traffic-qos-l7-app"),
                   MakeStringAccessor (&TrafficStatsCalculator::m_appFilename),
                   MakeStringChecker ())

  ;
  return tid;
}

std::string
TrafficStatsCalculator::DirectionStr (Direction dir)
{
  switch (dir)
    {
    case TrafficStatsCalculator::DLINK:
      return "Dlink";
    case TrafficStatsCalculator::ULINK:
      return "Ulink";
    default:
      return "-";
    }
}
/*
TrafficStatsCalculator::Direction
TrafficStatsCalculator::GetDirection (EpcGtpuTag &gtpuTag)
{
  return gtpuTag.IsDownlink () ? Direction::DLINK : Direction::ULINK;
}
*/
void
TrafficStatsCalculator::DoDispose ()
{
  NS_LOG_FUNCTION (this);

  m_appWrapper = 0;
  Object::DoDispose ();
}

void
TrafficStatsCalculator::NotifyConstructionCompleted (void)
{
  NS_LOG_FUNCTION (this);

  // Create the output file for application stats.
  m_appWrapper = Create<OutputStreamWrapper> (
      m_appFilename + ".log", std::ios::out);

  // Print the header in output file.
  *m_appWrapper->GetStream ()
    << boolalpha << right << fixed << setprecision (3)
    << " " << setw (8) << "Time:s"
    << " " << setw (11) << "Teid"
    << " " << setw (8) << "AppName"
    << " " << setw (6) << "Ul/Dl";

  AppStatsCalculator::PrintHeader (*m_appWrapper->GetStream ());
  *m_appWrapper->GetStream () << std::endl;

  Object::NotifyConstructionCompleted ();
}

void
TrafficStatsCalculator::DumpStatistics (std::string context,
                                        Ptr<SvelteClient> app)
{
  NS_LOG_FUNCTION (this << context << app->GetTeidHex ());

  uint32_t teid = app->GetTeid ();
  char teidStr[11];
  sprintf (teidStr,"0x%08x",teid);

  if (app->GetAppName () != "LiveVid")
    {
      // Dump uplink statistics.
      *m_appWrapper->GetStream ()
        << " " << setw (8) << Simulator::Now ().GetSeconds ()
        << " " << setw (11) << std::string (teidStr)
        << " " << setw (8) << app->GetAppName ()
        << " " << setw (6) << DirectionStr (Direction::ULINK)
        << *app->GetServerAppStats ()
        << std::endl;
    }

  // Dump downlink statistics.

  *m_appWrapper->GetStream ()
    << " " << setw (8) << Simulator::Now ().GetSeconds ()
    << " " << setw (11) <<  std::string (teidStr)
    << " " << setw (8) << app->GetAppName ()
    << " " << setw (6) << DirectionStr (Direction::DLINK)
    << *app->GetAppStats ()
    << std::endl;

}
/*
void
TrafficStatsCalculator::ResetCounters (std::string context,
                                       Ptr<SvelteClient> app)
{
  NS_LOG_FUNCTION (this << context << app);

}

void
TrafficStatsCalculator::OverloadDropPacket (std::string context,
                                            Ptr<const Packet> packet)
{
  NS_LOG_FUNCTION (this << context << packet);

  EpcGtpuTag gtpuTag;
  Ptr<EpcStatsCalculator> epcStats;
  if (packet->PeekPacketTag (gtpuTag))
    {
      epcStats = GetEpcStats (gtpuTag.GetTeid (), GetDirection (gtpuTag));
      epcStats->NotifyDrop (packet->GetSize (), EpcStatsCalculator::PLOAD);
    }
  else
    {
      //
      // This only happens when a packet is dropped at the P-GW, before
      // entering the logical port that is responsible for attaching the
      // EpcGtpuTag and notifying that the packet is entering the EPC. To keep
      // consistent log results, we are doing this manually here.
      //
      EthernetHeader ethHeader;
      Ipv4Header ipv4Header;

      Ptr<Packet> packetCopy = packet->Copy ();
      packetCopy->RemoveHeader (ethHeader);
      packetCopy->PeekHeader (ipv4Header);

      Ptr<UeInfo> ueInfo = UeInfo::GetPointer (ipv4Header.GetDestination ());
      uint32_t teid = ueInfo->Classify (packetCopy);

      epcStats = GetEpcStats (teid, Direction::DLINK);
      epcStats->NotifyTx (packetCopy->GetSize ());
      epcStats->NotifyDrop (packetCopy->GetSize (), EpcStatsCalculator::PLOAD);
    }
}

void
TrafficStatsCalculator::MeterDropPacket (
  std::string context, Ptr<const Packet> packet, uint32_t meterId)
{
  NS_LOG_FUNCTION (this << context << packet << meterId);

  uint32_t teid;
  EpcGtpuTag gtpuTag;
  Ptr<EpcStatsCalculator> epcStats;
  if (packet->PeekPacketTag (gtpuTag))
    {
      teid = gtpuTag.GetTeid ();
      epcStats = GetEpcStats (teid, GetDirection (gtpuTag));

      // Notify the droped packet, based on meter type (traffic or slicing).
      if (teid == meterId)
        {
          epcStats->NotifyDrop (packet->GetSize (), EpcStatsCalculator::METER);
        }
      else
        {
          epcStats->NotifyDrop (packet->GetSize (), EpcStatsCalculator::SLICE);
        }
    }
  else
    {
      //
      // This only happens when a packet is dropped at the P-GW, before
      // entering the logical port that is responsible for attaching the
      // EpcGtpuTag and notifying that the packet is entering the EPC.
      // To keep consistent log results, we are doing this manually here.
      //
      teid = meterId;
      epcStats = GetEpcStats (teid, Direction::DLINK);
      epcStats->NotifyTx (packet->GetSize ());

      // Notify the droped packet (it must be a traffic meter because we only
      // have slicing meters on ring switches, not on the P-GW).
      epcStats->NotifyDrop (packet->GetSize (), EpcStatsCalculator::METER);
    }
}

void
TrafficStatsCalculator::QueueDropPacket (std::string context,
                                         Ptr<const Packet> packet)
{
  NS_LOG_FUNCTION (this << context << packet);

  EpcGtpuTag gtpuTag;
  Ptr<EpcStatsCalculator> epcStats;
  if (packet->PeekPacketTag (gtpuTag))
    {
      epcStats = GetEpcStats (gtpuTag.GetTeid (), GetDirection (gtpuTag));
      epcStats->NotifyDrop (packet->GetSize (), EpcStatsCalculator::QUEUE);
    }
}

*/

} // Namespace ns3
