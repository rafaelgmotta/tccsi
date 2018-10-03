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

#include "svelte-client-app.h"
#include "svelte-server-app.h"

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT                             \
  std::clog << "[" << GetAppName ()                       \
            << " client teid " << GetTeidHex () << "] ";

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SvelteClientApp");
NS_OBJECT_ENSURE_REGISTERED (SvelteClientApp);

SvelteClientApp::SvelteClientApp ()
  : m_appStats (CreateObject<AppStatsCalculator> ()),
  m_socket (0),
  m_serverApp (0),
  m_active (false),
  m_forceStop (EventId ()),
  m_forceStopFlag (false),
  m_tft (0),
  m_teid (0)
{
  NS_LOG_FUNCTION (this);
}

SvelteClientApp::~SvelteClientApp ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
SvelteClientApp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SvelteClientApp")
    .SetParent<Application> ()
    .AddConstructor<SvelteClientApp> ()
    .AddAttribute ("MaxOnTime",
                   "A hard duration time threshold.",
                   TimeValue (Time ()),
                   MakeTimeAccessor (&SvelteClientApp::m_maxOnTime),
                   MakeTimeChecker ())
    .AddAttribute ("AppName",
                   "The application name.",
                   StringValue ("NoName"),
                   MakeStringAccessor (&SvelteClientApp::m_name),
                   MakeStringChecker ())

    .AddAttribute ("ServerAddress",
                   "The server socket address.",
                   AddressValue (),
                   MakeAddressAccessor (&SvelteClientApp::m_serverAddress),
                   MakeAddressChecker ())
    .AddAttribute ("LocalPort",
                   "Local port.",
                   UintegerValue (10000),
                   MakeUintegerAccessor (&SvelteClientApp::m_localPort),
                   MakeUintegerChecker<uint16_t> ())

    .AddTraceSource ("AppStart",
                     "SvelteClientApp start trace source.",
                     MakeTraceSourceAccessor (
                       &SvelteClientApp::m_appStartTrace),
                     "ns3::SvelteClientApp::EpcAppTracedCallback")
    .AddTraceSource ("AppStop",
                     "SvelteClientApp stop trace source.",
                     MakeTraceSourceAccessor (
                       &SvelteClientApp::m_appStopTrace),
                     "ns3::SvelteClientApp::EpcAppTracedCallback")
    .AddTraceSource ("AppError",
                     "SvelteClientApp error trace source.",
                     MakeTraceSourceAccessor (
                       &SvelteClientApp::m_appErrorTrace),
                     "ns3::SvelteClientApp::EpcAppTracedCallback")
  ;
  return tid;
}

std::string
SvelteClientApp::GetAppName (void) const
{
  // No log to avoid infinite recursion.
  return m_name;
}

std::string
SvelteClientApp::GetNameTeid (void) const
{
  // No log to avoid infinite recursion.
  std::ostringstream value;
  value << GetAppName () << " over bearer teid " << GetTeidHex ();
  return value.str ();
}

bool
SvelteClientApp::IsActive (void) const
{
  NS_LOG_FUNCTION (this);

  return m_active;
}

Time
SvelteClientApp::GetMaxOnTime (void) const
{
  NS_LOG_FUNCTION (this);

  return m_maxOnTime;
}

bool
SvelteClientApp::IsForceStop (void) const
{
  NS_LOG_FUNCTION (this);

  return m_forceStopFlag;
}

Ptr<EpcTft>
SvelteClientApp::GetTft (void) const
{
  NS_LOG_FUNCTION (this);

  return m_tft;
}

EpsBearer
SvelteClientApp::GetEpsBearer (void) const
{
  NS_LOG_FUNCTION (this);

  return m_bearer;
}

uint32_t
SvelteClientApp::GetTeid (void) const
{
  NS_LOG_FUNCTION (this);

  return m_teid;
}

std::string
SvelteClientApp::GetTeidHex (void) const
{
  // No log to avoid infinite recursion.
  char valueStr[11];
  sprintf(valueStr, "0x%08x", m_teid);
  return std::string(valueStr);
}

Ptr<SvelteServerApp>
SvelteClientApp::GetServerApp (void) const
{
  NS_LOG_FUNCTION (this);

  return m_serverApp;
}

Ptr<const AppStatsCalculator>
SvelteClientApp::GetAppStats (void) const
{
  NS_LOG_FUNCTION (this);

  return m_appStats;
}

Ptr<const AppStatsCalculator>
SvelteClientApp::GetServerAppStats (void) const
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT_MSG (m_serverApp, "Server application undefined.");
  return m_serverApp->GetAppStats ();
}

void
SvelteClientApp::SetTft (Ptr<EpcTft> value)
{
  NS_LOG_FUNCTION (this << value);

  m_tft = value;
}

void
SvelteClientApp::SetEpsBearer (EpsBearer value)
{
  NS_LOG_FUNCTION (this);

  m_bearer = value;
}

void
SvelteClientApp::SetTeid (uint32_t value)
{
  NS_LOG_FUNCTION (this << value);

  m_teid = value;
}

void
SvelteClientApp::SetServer (Ptr<SvelteServerApp> serverApp,
                            Address serverAddress)
{
  NS_LOG_FUNCTION (this << serverApp << serverAddress);

  m_serverApp = serverApp;
  m_serverAddress = serverAddress;
}

void
SvelteClientApp::Start ()
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
        Simulator::Schedule (m_maxOnTime, &SvelteClientApp::ForceStop, this);
    }

  // Notify the server and fire start trace source.
  NS_ASSERT_MSG (m_serverApp, "Server application undefined.");
  m_serverApp->NotifyStart ();
  m_appStartTrace (this);
}

void
SvelteClientApp::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_appStats = 0;
  m_tft = 0;
  m_socket = 0;
  m_serverApp = 0;
  m_forceStop.Cancel ();
  Application::DoDispose ();
}

void
SvelteClientApp::ForceStop ()
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

void
SvelteClientApp::NotifyStop (bool withError)
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
SvelteClientApp::NotifyTx (uint32_t txBytes)
{
  NS_LOG_FUNCTION (this << txBytes);

  NS_ASSERT_MSG (m_serverApp, "Server application undefined.");
  return m_serverApp->m_appStats->NotifyTx (txBytes);
}

void
SvelteClientApp::NotifyRx (uint32_t rxBytes, Time timestamp)
{
  NS_LOG_FUNCTION (this << rxBytes << timestamp);

  m_appStats->NotifyRx (rxBytes, timestamp);
}

void
SvelteClientApp::ResetAppStats ()
{
  NS_LOG_FUNCTION (this);

  m_appStats->ResetCounters ();
}

} // namespace ns3
