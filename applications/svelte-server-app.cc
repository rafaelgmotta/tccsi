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

#include "svelte-server-app.h"
#include "svelte-client-app.h"

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT                             \
  std::clog << "[" << GetAppName ()                       \
            << " server teid " << GetTeidHex () << "] ";

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SvelteServerApp");
NS_OBJECT_ENSURE_REGISTERED (SvelteServerApp);

SvelteServerApp::SvelteServerApp ()
  : m_appStats (CreateObject<AppStatsCalculator> ()),
  m_socket (0),
  m_clientApp (0)
{
  NS_LOG_FUNCTION (this);
}

SvelteServerApp::~SvelteServerApp ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
SvelteServerApp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SvelteServerApp")
    .SetParent<Application> ()
    .AddConstructor<SvelteServerApp> ()
    .AddAttribute ("ClientAddress",
                   "The client socket address.",
                   AddressValue (),
                   MakeAddressAccessor (&SvelteServerApp::m_clientAddress),
                   MakeAddressChecker ())
    .AddAttribute ("LocalPort",
                   "Local port.",
                   UintegerValue (10000),
                   MakeUintegerAccessor (&SvelteServerApp::m_localPort),
                   MakeUintegerChecker<uint16_t> ())
  ;
  return tid;
}

std::string
SvelteServerApp::GetAppName (void) const
{
  // No log to avoid infinite recursion.
  return m_clientApp ? m_clientApp->GetAppName () : "";
}

bool
SvelteServerApp::IsActive (void) const
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT_MSG (m_clientApp, "Client application undefined.");
  return m_clientApp->IsActive ();
}

bool
SvelteServerApp::IsForceStop (void) const
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT_MSG (m_clientApp, "Client application undefined.");
  return m_clientApp->IsForceStop ();
}

std::string
SvelteServerApp::GetTeidHex (void) const
{
  // No log to avoid infinite recursion.
  return m_clientApp ? m_clientApp->GetTeidHex () : "0x0";
}

Ptr<SvelteClientApp>
SvelteServerApp::GetClientApp (void) const
{
  NS_LOG_FUNCTION (this);

  return m_clientApp;
}

Ptr<const AppStatsCalculator>
SvelteServerApp::GetAppStats (void) const
{
  NS_LOG_FUNCTION (this);

  return m_appStats;
}

void
SvelteServerApp::SetClient (Ptr<SvelteClientApp> clientApp,
                            Address clientAddress)
{
  NS_LOG_FUNCTION (this << clientApp << clientAddress);

  m_clientApp = clientApp;
  m_clientAddress = clientAddress;
}

void
SvelteServerApp::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_appStats = 0;
  m_socket = 0;
  m_clientApp = 0;
  Application::DoDispose ();
}

void
SvelteServerApp::NotifyStart ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("Starting server application.");

  // Reset internal statistics.
  ResetAppStats ();
}

void
SvelteServerApp::NotifyForceStop ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("Forcing the server application to stop.");
}

uint32_t
SvelteServerApp::NotifyTx (uint32_t txBytes)
{
  NS_LOG_FUNCTION (this << txBytes);

  NS_ASSERT_MSG (m_clientApp, "Client application undefined.");
  return m_clientApp->m_appStats->NotifyTx (txBytes);
}

void
SvelteServerApp::NotifyRx (uint32_t rxBytes, Time timestamp)
{
  NS_LOG_FUNCTION (this << rxBytes << timestamp);

  m_appStats->NotifyRx (rxBytes, timestamp);
}

void
SvelteServerApp::ResetAppStats ()
{
  NS_LOG_FUNCTION (this);

  m_appStats->ResetCounters ();
}

} // namespace ns3
