/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 University of Campinas (Unicamp)
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

#include "svelte-client.h"
#include "svelte-server.h"

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT                             \
  std::clog << "[" << GetAppName ()                       \
            << " client teid " << GetTeidHex () << "] ";

namespace ns3 {

std::string
GetUint32Hex (uint32_t value)
{
  char valueStr [11];
  sprintf (valueStr, "0x%08x", value);
  return std::string (valueStr);
}

NS_LOG_COMPONENT_DEFINE ("SvelteClient");
NS_OBJECT_ENSURE_REGISTERED (SvelteClient);

SvelteClient::SvelteClient ()
  : m_appStats (CreateObject<AppStatsCalculator> ()),
  m_socket (0),
  m_serverApp (0),
  m_active (false),
  m_forceStop (EventId ()),
  m_forceStopFlag (false),
  m_bearerId (1),   // This is the default BID.
  m_teid (0)
{
  NS_LOG_FUNCTION (this);
}

SvelteClient::~SvelteClient ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
SvelteClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SvelteClient")
    .SetParent<Application> ()
    .AddConstructor<SvelteClient> ()
    .AddAttribute ("AppName", "The application name.",
                   StringValue ("NoName"),
                   MakeStringAccessor (&SvelteClient::m_name),
                   MakeStringChecker ())
    .AddAttribute ("MaxOnTime", "A hard duration time threshold.",
                   TimeValue (Time ()),
                   MakeTimeAccessor (&SvelteClient::m_maxOnTime),
                   MakeTimeChecker ())
    .AddAttribute ("TrafficLength",
                   "A random variable used to pick the traffic length [s].",
                   StringValue ("ns3::ConstantRandomVariable[Constant=30.0]"),
                   MakePointerAccessor (&SvelteClient::m_lengthRng),
                   MakePointerChecker <RandomVariableStream> ())

    .AddAttribute ("ServerAddress", "The server socket address.",
                   AddressValue (),
                   MakeAddressAccessor (&SvelteClient::m_serverAddress),
                   MakeAddressChecker ())
    .AddAttribute ("LocalPort", "Local port.",
                   UintegerValue (10000),
                   MakeUintegerAccessor (&SvelteClient::m_localPort),
                   MakeUintegerChecker<uint16_t> ())

    .AddTraceSource ("AppStart", "SvelteClient start trace source.",
                     MakeTraceSourceAccessor (&SvelteClient::m_appStartTrace),
                     "ns3::SvelteClient::EpcAppTracedCallback")
    .AddTraceSource ("AppStop", "SvelteClient stop trace source.",
                     MakeTraceSourceAccessor (&SvelteClient::m_appStopTrace),
                     "ns3::SvelteClient::EpcAppTracedCallback")
    .AddTraceSource ("AppError", "SvelteClient error trace source.",
                     MakeTraceSourceAccessor (&SvelteClient::m_appErrorTrace),
                     "ns3::SvelteClient::EpcAppTracedCallback")
  ;
  return tid;
}

std::string
SvelteClient::GetAppName (void) const
{
  // No log to avoid infinite recursion.
  return m_name;
}

std::string
SvelteClient::GetNameTeid (void) const
{
  // No log to avoid infinite recursion.
  std::ostringstream value;
  value << GetAppName () << " over bearer teid " << GetTeidHex ();
  return value.str ();
}

bool
SvelteClient::IsActive (void) const
{
  NS_LOG_FUNCTION (this);

  return m_active;
}

Time
SvelteClient::GetMaxOnTime (void) const
{
  NS_LOG_FUNCTION (this);

  return m_maxOnTime;
}

bool
SvelteClient::IsForceStop (void) const
{
  NS_LOG_FUNCTION (this);

  return m_forceStopFlag;
}

EpsBearer
SvelteClient::GetEpsBearer (void) const
{
  NS_LOG_FUNCTION (this);

  return m_bearer;
}

uint8_t
SvelteClient::GetEpsBearerId (void) const
{
  NS_LOG_FUNCTION (this);

  return m_bearerId;
}

uint32_t
SvelteClient::GetTeid (void) const
{
  NS_LOG_FUNCTION (this);

  return m_teid;
}

std::string
SvelteClient::GetTeidHex (void) const
{
  // No log to avoid infinite recursion.
  return GetUint32Hex (m_teid);
}

Ptr<SvelteServer>
SvelteClient::GetServerApp (void) const
{
  NS_LOG_FUNCTION (this);

  return m_serverApp;
}

Ptr<const AppStatsCalculator>
SvelteClient::GetAppStats (void) const
{
  NS_LOG_FUNCTION (this);

  return m_appStats;
}

Ptr<const AppStatsCalculator>
SvelteClient::GetServerAppStats (void) const
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT_MSG (m_serverApp, "Server application undefined.");
  return m_serverApp->GetAppStats ();
}

void
SvelteClient::SetEpsBearer (EpsBearer value)
{
  NS_LOG_FUNCTION (this);

  m_bearer = value;
}

void
SvelteClient::SetEpsBearerId (uint8_t value)
{
  NS_LOG_FUNCTION (this << value);

  m_bearerId = value;
}

void
SvelteClient::SetTeid (uint32_t value)
{
  NS_LOG_FUNCTION (this << value);

  m_teid = value;
}

void
SvelteClient::SetServer (Ptr<SvelteServer> serverApp, Address serverAddress)
{
  NS_LOG_FUNCTION (this << serverApp << serverAddress);

  m_serverApp = serverApp;
  m_serverAddress = serverAddress;
}

void
SvelteClient::Start ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("Starting client application.");

  // Set the active flag.
  NS_ASSERT_MSG (!IsActive (), "Can't start an already active application.");
  m_active = true;

  // Reset internal statistics.
  ResetAppStats ();

  // Schedule the force stop event.
  m_forceStopFlag = false;
  if (!m_maxOnTime.IsZero ())
    {
      m_forceStop =
        Simulator::Schedule (m_maxOnTime, &SvelteClient::ForceStop, this);
    }

  // Notify the server and fire start trace source.
  NS_ASSERT_MSG (m_serverApp, "Server application undefined.");
  m_serverApp->NotifyStart ();
  m_appStartTrace (this);
}

void
SvelteClient::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_appStats = 0;
  m_lengthRng = 0;
  m_socket = 0;
  m_serverApp = 0;
  m_forceStop.Cancel ();
  Application::DoDispose ();
}

void
SvelteClient::ForceStop ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("Forcing the client application to stop.");

  // Set the force stop flag.
  NS_ASSERT_MSG (IsActive (), "Can't stop an inactive application.");
  m_forceStopFlag = true;
  m_forceStop.Cancel ();

  // Notify the server.
  NS_ASSERT_MSG (m_serverApp, "Server application undefined.");
  m_serverApp->NotifyForceStop ();
}

Time
SvelteClient::GetTrafficLength ()
{
  NS_LOG_FUNCTION (this);

  return Seconds (std::abs (m_lengthRng->GetValue ()));
}

void
SvelteClient::NotifyStop (bool withError)
{
  NS_LOG_FUNCTION (this << withError);
  NS_LOG_INFO ("Client application stopped.");

  // Set the active flag.
  NS_ASSERT_MSG (IsActive (), "Can't stop an inactive application.");
  m_active = false;
  m_forceStop.Cancel ();

  // Fire the stop trace source.
  if (withError)
    {
      NS_LOG_ERROR ("Client application stopped with error.");
      m_appErrorTrace (this);
    }
  else
    {
      m_appStopTrace (this);
    }
}

uint32_t
SvelteClient::NotifyTx (uint32_t txBytes)
{
  NS_LOG_FUNCTION (this << txBytes);

  NS_ASSERT_MSG (m_serverApp, "Server application undefined.");
  return m_serverApp->m_appStats->NotifyTx (txBytes);
}

void
SvelteClient::NotifyRx (uint32_t rxBytes, Time timestamp)
{
  NS_LOG_FUNCTION (this << rxBytes << timestamp);

  m_appStats->NotifyRx (rxBytes, timestamp);
}

void
SvelteClient::ResetAppStats ()
{
  NS_LOG_FUNCTION (this);

  m_appStats->ResetCounters ();
}

} // namespace ns3
