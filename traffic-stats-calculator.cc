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
#include "traffic-stats-calculator.h"
#include "applications/svelte-client.h"

using namespace std;

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TrafficStatsCalculator");
NS_OBJECT_ENSURE_REGISTERED (TrafficStatsCalculator);

TrafficStatsCalculator::TrafficStatsCalculator ()
{
  NS_LOG_FUNCTION (this);

  // Clear adm and drp stats.
  memset (&m_admStats, 0, sizeof (AdmStats));
  memset (&m_drpStats, 0, sizeof (DropStats));

  // Connect this stats calculator to required trace sources.
  Config::Connect (
    "/NodeList/*/ApplicationList/*/$ns3::CustomController/Request",
    MakeCallback (&TrafficStatsCalculator::NotifyRequest, this));
  Config::Connect (
    "/NodeList/*/ApplicationList/*/$ns3::CustomController/Release",
    MakeCallback (&TrafficStatsCalculator::NotifyRelease, this));
  Config::Connect (
    "/NodeList/*/$ns3::OFSwitch13Device/OverloadDrop",
    MakeCallback (&TrafficStatsCalculator::OverloadDropPacket, this));
  Config::Connect (
    "/NodeList/*/$ns3::OFSwitch13Device/MeterDrop",
    MakeCallback (&TrafficStatsCalculator::MeterDropPacket, this));
  Config::Connect (
    "/NodeList/*/$ns3::OFSwitch13Device/PortList/*/PortQueue/Drop",
    MakeCallback (&TrafficStatsCalculator::QueueDropPacket, this));
  Config::Connect (
    "/NodeList/*/ApplicationList/*/$ns3::SvelteClient/AppStop",
    MakeCallback (&TrafficStatsCalculator::DumpTraffic, this));
  Config::Connect (
    "/NodeList/*/ApplicationList/*/$ns3::SvelteClient/AppError",
    MakeCallback (&TrafficStatsCalculator::DumpTraffic, this));
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
    .AddAttribute ("AdmStatsFilename",
                   "Filename for bearer admission and counter statistics.",
                   StringValue ("admission-counters"),
                   MakeStringAccessor (&TrafficStatsCalculator::m_admFilename),
                   MakeStringChecker ())
    .AddAttribute ("AppStatsFilename",
                   "Filename for L7 traffic application QoS statistics.",
                   StringValue ("traffic-qos-l7-app"),
                   MakeStringAccessor (&TrafficStatsCalculator::m_appFilename),
                   MakeStringChecker ())
    .AddAttribute ("DrpStatsFilename",
                   "Filename packet drop statistics.",
                   StringValue ("packet-drops"),
                   MakeStringAccessor (&TrafficStatsCalculator::m_drpFilename),
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

void
TrafficStatsCalculator::DoDispose ()
{
  NS_LOG_FUNCTION (this);

  m_admWrapper = 0;
  m_appWrapper = 0;
  m_drpWrapper = 0;
  Object::DoDispose ();
}

void
TrafficStatsCalculator::NotifyConstructionCompleted (void)
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
    << " " << setw (6) << "Relea"
    << " " << setw (6) << "Reque"
    << " " << setw (6) << "Accep"
    << " " << setw (6) << "Block"
    << " " << setw (6) << "#Actv"
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
    << " " << setw (6)  << "Load"
    << " " << setw (6)  << "Meter"
    << " " << setw (6)  << "Queue"
    << std::endl;

  Simulator::Schedule (Seconds (1), &TrafficStatsCalculator::DumpAdmission, this);
  Simulator::Schedule (Seconds (1), &TrafficStatsCalculator::DumpDrop, this);

  Object::NotifyConstructionCompleted ();
}

void
TrafficStatsCalculator::DumpAdmission ()
{
  NS_LOG_FUNCTION (this);

  *m_admWrapper->GetStream ()
    << " " << setw (8) << Simulator::Now ().GetSeconds ()
    << " " << setw (6) << m_admStats.releases
    << " " << setw (6) << m_admStats.requests
    << " " << setw (6) << m_admStats.accepted
    << " " << setw (6) << m_admStats.blocked
    << " " << setw (6) << m_admStats.activeBearers
    << std::endl;

  m_admStats.releases = 0;
  m_admStats.requests = 0;
  m_admStats.accepted = 0;
  m_admStats.blocked = 0;

  Simulator::Schedule (Seconds (1), &TrafficStatsCalculator::DumpAdmission, this);
}

void
TrafficStatsCalculator::DumpDrop ()
{
  NS_LOG_FUNCTION (this);

  *m_drpWrapper->GetStream ()
    << " " << setw (8) << Simulator::Now ().GetSeconds ()
    << " " << setw (6) << m_drpStats.load
    << " " << setw (6) << m_drpStats.meter
    << " " << setw (6) << m_drpStats.queue
    << std::endl;

  m_drpStats.load = 0;
  m_drpStats.meter = 0;
  m_drpStats.queue = 0;

  Simulator::Schedule (Seconds (1), &TrafficStatsCalculator::DumpDrop, this);
}

void
TrafficStatsCalculator::DumpTraffic (
  std::string context, Ptr<SvelteClient> app)
{
  NS_LOG_FUNCTION (this << context << app->GetTeidHex ());

  uint32_t teid = app->GetTeid ();
  char teidStr[11];
  sprintf (teidStr,"0x%08x",teid);

  if (app->GetAppName () != "LivVideo")
    {
      // Dump uplink statistics.
      *m_appWrapper->GetStream ()
        << " " << setw (8)  << Simulator::Now ().GetSeconds ()
        << " " << setw (11) << std::string (teidStr)
        << " " << setw (8)  << app->GetAppName ()
        << " " << setw (6)  << DirectionStr (Direction::ULINK)
        << *app->GetServerAppStats ()
        << std::endl;
    }

  // Dump downlink statistics.
  *m_appWrapper->GetStream ()
    << " " << setw (8)  << Simulator::Now ().GetSeconds ()
    << " " << setw (11) << std::string (teidStr)
    << " " << setw (8)  << app->GetAppName ()
    << " " << setw (6)  << DirectionStr (Direction::DLINK)
    << *app->GetAppStats ()
    << std::endl;
}

void
TrafficStatsCalculator::NotifyRequest (
  std::string context, uint32_t teid, bool accepted)
{
  m_admStats.requests++;
  if (accepted)
    {
      m_admStats.accepted++;
      m_admStats.activeBearers++;
    }
  else
    {
      m_admStats.blocked++;
    }
}

void
TrafficStatsCalculator::NotifyRelease (
  std::string context, uint32_t teid)
{
  m_admStats.releases++;
  m_admStats.activeBearers--;
}

void
TrafficStatsCalculator::OverloadDropPacket (
  std::string context, Ptr<const Packet> packet)
{
  NS_LOG_FUNCTION (this << context << packet);

  m_drpStats.load++;
}

void
TrafficStatsCalculator::MeterDropPacket (
  std::string context, Ptr<const Packet> packet, uint32_t meterId)
{
  NS_LOG_FUNCTION (this << context << packet << meterId);

  m_drpStats.meter++; 
}

void
TrafficStatsCalculator::QueueDropPacket (
  std::string context, Ptr<const Packet> packet)
{
  NS_LOG_FUNCTION (this << context << packet);

  m_drpStats.queue++; 
}

} // Namespace ns3
