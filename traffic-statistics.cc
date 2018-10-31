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
#include <string>
#include "traffic-statistics.h"
#include "applications/svelte-client.h"

using namespace std;

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TrafficStatistics");
NS_OBJECT_ENSURE_REGISTERED (TrafficStatistics);

TrafficStatistics::TrafficStatistics ()
{
  NS_LOG_FUNCTION (this);

  // Clear adm and drp stats.
  memset (&m_admStats, 0, sizeof (AdmStats));
  memset (&m_drpStats, 0, sizeof (DropStats));

  // Connect this stats calculator to required trace sources.
  Config::Connect (
    "/NodeList/*/ApplicationList/*/$ns3::CustomController/Request",
    MakeCallback (&TrafficStatistics::NotifyRequest, this));
  Config::Connect (
    "/NodeList/*/ApplicationList/*/$ns3::CustomController/Release",
    MakeCallback (&TrafficStatistics::NotifyRelease, this));
  Config::Connect (
    "/NodeList/*/$ns3::OFSwitch13Device/OverloadDrop",
    MakeCallback (&TrafficStatistics::OverloadDropPacket, this));
  Config::Connect (
    "/NodeList/*/$ns3::OFSwitch13Device/MeterDrop",
    MakeCallback (&TrafficStatistics::MeterDropPacket, this));
  Config::Connect (
    "/NodeList/*/$ns3::OFSwitch13Device/PortList/*/PortQueue/Drop",
    MakeCallback (&TrafficStatistics::QueueDropPacket, this));
  Config::Connect (
    "/NodeList/*/ApplicationList/*/$ns3::SvelteClient/AppStop",
    MakeCallback (&TrafficStatistics::DumpTraffic, this));
  Config::Connect (
    "/NodeList/*/ApplicationList/*/$ns3::SvelteClient/AppError",
    MakeCallback (&TrafficStatistics::DumpTraffic, this));
}

TrafficStatistics::~TrafficStatistics ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
TrafficStatistics::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TrafficStatistics")
    .SetParent<Object> ()
    .AddConstructor<TrafficStatistics> ()
    .AddAttribute ("AdmStatsFilename",
                   "Filename for bearer admission and counter statistics.",
                   StringValue ("admission-counters"),
                   MakeStringAccessor (&TrafficStatistics::m_admFilename),
                   MakeStringChecker ())
    .AddAttribute ("AppStatsFilename",
                   "Filename for L7 traffic application QoS statistics.",
                   StringValue ("traffic-qos-l7-app"),
                   MakeStringAccessor (&TrafficStatistics::m_appFilename),
                   MakeStringChecker ())
    .AddAttribute ("DrpStatsFilename",
                   "Filename packet drop statistics.",
                   StringValue ("packet-drops"),
                   MakeStringAccessor (&TrafficStatistics::m_drpFilename),
                   MakeStringChecker ())
  ;
  return tid;
}

std::string
TrafficStatistics::DirectionStr (Direction dir)
{
  switch (dir)
    {
    case TrafficStatistics::DLINK:
      return "Dlink";
    case TrafficStatistics::ULINK:
      return "Ulink";
    default:
      return "-";
    }
}

void
TrafficStatistics::DoDispose ()
{
  NS_LOG_FUNCTION (this);

  m_admWrapper = 0;
  m_appWrapper = 0;
  m_drpWrapper = 0;
  Object::DoDispose ();
}

void
TrafficStatistics::NotifyConstructionCompleted (void)
{
  NS_LOG_FUNCTION (this);

  StringValue stringValue;
  GlobalValue::GetValueByName ("OutputPrefix", stringValue);
  std::string prefix = stringValue.Get ();
  SetAttribute ("AdmStatsFilename", StringValue (prefix + m_admFilename));
  SetAttribute ("AppStatsFilename", StringValue (prefix + m_appFilename));
  SetAttribute ("DrpStatsFilename", StringValue (prefix + m_drpFilename));

  // Create the output file for admission stats.
  m_admWrapper = Create<OutputStreamWrapper> (m_admFilename + ".log", std::ios::out);

  // Print the header in output file.
  *m_admWrapper->GetStream ()
    << boolalpha << right << fixed << setprecision (3)
    << " " << setw (8) << "Time:s"
    << " " << setw (8) << "IReque"
    << " " << setw (8) << "IAccep"
    << " " << setw (8) << "IBlock"
    << " " << setw (8) << "IRelea"
    << " " << setw (8) << "#Actv"
    << " " << setw (8) << "TReque"
    << " " << setw (8) << "TAccep"
    << " " << setw (8) << "TBlock"
    << " " << setw (8) << "TRelea"
    << std::endl;

  // Create the output file for application stats.
  m_appWrapper = Create<OutputStreamWrapper> (m_appFilename + ".log", std::ios::out);

  // Print the header in output file.
  *m_appWrapper->GetStream ()
    << boolalpha << right << fixed << setprecision (3)
    << " " << setw (8)  << "Time:s"
    << " " << setw (11) << "Teid"
    << " " << setw (8)  << "AppName"
    << " " << setw (6)  << "Ul/Dl";
  AppStatsCalculator::PrintHeader (*m_appWrapper->GetStream ());
  *m_appWrapper->GetStream () << std::endl;

  // Create the output file for drop stats.
  m_drpWrapper = Create<OutputStreamWrapper> (m_drpFilename + ".log", std::ios::out);

  // Print the header in output file.
  *m_drpWrapper->GetStream ()
    << boolalpha << right << fixed << setprecision (3)
    << " " << setw (8)  << "Time:s"
    << " " << setw (8)  << "ILoad"
    << " " << setw (8)  << "IMeter"
    << " " << setw (8)  << "IQueue"
    << " " << setw (8)  << "TLoad"
    << " " << setw (8)  << "TMeter"
    << " " << setw (8)  << "TQueue"
    << std::endl;

  Simulator::Schedule (Seconds (1), &TrafficStatistics::DumpAdmission, this);
  Simulator::Schedule (Seconds (1), &TrafficStatistics::DumpDrop, this);

  Object::NotifyConstructionCompleted ();
}

void
TrafficStatistics::DumpAdmission ()
{
  NS_LOG_FUNCTION (this);

  *m_admWrapper->GetStream ()
    << " " << setw (8) << Simulator::Now ().GetSeconds ()
    << " " << setw (8) << m_admStats.tempRequests
    << " " << setw (8) << m_admStats.tempAccepted
    << " " << setw (8) << m_admStats.tempBlocked
    << " " << setw (8) << m_admStats.tempReleases
    << " " << setw (8) << m_admStats.activeBearers
    << " " << setw (8) << m_admStats.totalRequests
    << " " << setw (8) << m_admStats.totalAccepted
    << " " << setw (8) << m_admStats.totalBlocked
    << " " << setw (8) << m_admStats.totalReleases
    << std::endl;

  m_admStats.tempReleases = 0;
  m_admStats.tempRequests = 0;
  m_admStats.tempAccepted = 0;
  m_admStats.tempBlocked = 0;

  Simulator::Schedule (Seconds (1), &TrafficStatistics::DumpAdmission, this);
}

void
TrafficStatistics::DumpDrop ()
{
  NS_LOG_FUNCTION (this);

  *m_drpWrapper->GetStream ()
    << " " << setw (8) << Simulator::Now ().GetSeconds ()
    << " " << setw (8) << m_drpStats.tempLoad
    << " " << setw (8) << m_drpStats.tempMeter
    << " " << setw (8) << m_drpStats.tempQueue
    << " " << setw (8) << m_drpStats.totalLoad
    << " " << setw (8) << m_drpStats.totalMeter
    << " " << setw (8) << m_drpStats.totalQueue
    << std::endl;

  m_drpStats.tempLoad = 0;
  m_drpStats.tempMeter = 0;
  m_drpStats.tempQueue = 0;

  Simulator::Schedule (Seconds (1), &TrafficStatistics::DumpDrop, this);
}

void
TrafficStatistics::DumpTraffic (
  std::string context, Ptr<SvelteClient> app)
{
  NS_LOG_FUNCTION (this << context << app->GetTeidHex ());

  if (app->GetAppName () != "LivVideo")
    {
      // Dump uplink statistics.
      *m_appWrapper->GetStream ()
        << " " << setw (8)  << Simulator::Now ().GetSeconds ()
        << " " << setw (11) << app->GetTeidHex ()
        << " " << setw (8)  << app->GetAppName ()
        << " " << setw (6)  << DirectionStr (Direction::ULINK)
        << *app->GetServerAppStats ()
        << std::endl;
    }

  // Dump downlink statistics.
  *m_appWrapper->GetStream ()
    << " " << setw (8)  << Simulator::Now ().GetSeconds ()
    << " " << setw (11) << app->GetTeidHex ()
    << " " << setw (8)  << app->GetAppName ()
    << " " << setw (6)  << DirectionStr (Direction::DLINK)
    << *app->GetAppStats ()
    << std::endl;
}

void
TrafficStatistics::NotifyRequest (
  std::string context, uint32_t teid, bool accepted)
{
  NS_LOG_FUNCTION (this << context << teid << accepted);

  m_admStats.tempRequests++;
  m_admStats.totalRequests++;
  if (accepted)
    {
      m_admStats.tempAccepted++;
      m_admStats.totalAccepted++;
      m_admStats.activeBearers++;
    }
  else
    {
      m_admStats.tempBlocked++;
      m_admStats.totalBlocked++;
    }
}

void
TrafficStatistics::NotifyRelease (
  std::string context, uint32_t teid)
{
  NS_LOG_FUNCTION (this << context << teid);

  m_admStats.tempReleases++;
  m_admStats.totalReleases++;
  m_admStats.activeBearers--;
}

void
TrafficStatistics::OverloadDropPacket (
  std::string context, Ptr<const Packet> packet)
{
  NS_LOG_FUNCTION (this << context << packet);

  m_drpStats.tempLoad++;
  m_drpStats.totalLoad++;
}

void
TrafficStatistics::MeterDropPacket (
  std::string context, Ptr<const Packet> packet, uint32_t meterId)
{
  NS_LOG_FUNCTION (this << context << packet << meterId);

  m_drpStats.tempMeter++;
  m_drpStats.totalMeter++;
}

void
TrafficStatistics::QueueDropPacket (
  std::string context, Ptr<const Packet> packet)
{
  NS_LOG_FUNCTION (this << context << packet);

  m_drpStats.tempQueue++;
  m_drpStats.totalQueue++;
}

} // Namespace ns3
