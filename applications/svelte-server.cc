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

#include "svelte-server.h"
#include "svelte-client.h"

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT                             \
  std::clog << "[" << GetAppName ()                       \
            << " server teid " << GetTeidHex () << "] ";

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SvelteServer");
NS_OBJECT_ENSURE_REGISTERED (SvelteServer);

SvelteServer::SvelteServer ()
  : m_appStats (CreateObject<AppStatsCalculator> ()),
  m_socket (0),
  m_clientApp (0)
{
  NS_LOG_FUNCTION (this);
}

SvelteServer::~SvelteServer ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
SvelteServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SvelteServer")
    .SetParent<Application> ()
    .AddConstructor<SvelteServer> ()
    .AddAttribute ("ClientAddress", "The client socket address.",
                   AddressValue (),
                   MakeAddressAccessor (&SvelteServer::m_clientAddress),
                   MakeAddressChecker ())
    .AddAttribute ("LocalPort", "Local port.",
                   UintegerValue (10000),
                   MakeUintegerAccessor (&SvelteServer::m_localPort),
                   MakeUintegerChecker<uint16_t> ())
  ;
  return tid;
}

std::string
SvelteServer::GetAppName (void) const
{
  // No log to avoid infinite recursion.
  return m_clientApp ? m_clientApp->GetAppName () : "";
}

bool
SvelteServer::IsActive (void) const
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT_MSG (m_clientApp, "Client application undefined.");
  return m_clientApp->IsActive ();
}

bool
SvelteServer::IsForceStop (void) const
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT_MSG (m_clientApp, "Client application undefined.");
  return m_clientApp->IsForceStop ();
}

std::string
SvelteServer::GetTeidHex (void) const
{
  // No log to avoid infinite recursion.
  return m_clientApp ? m_clientApp->GetTeidHex () : "0x0";
}

Ptr<SvelteClient>
SvelteServer::GetClientApp (void) const
{
  NS_LOG_FUNCTION (this);

  return m_clientApp;
}

Ptr<const AppStatsCalculator>
SvelteServer::GetAppStats (void) const
{
  NS_LOG_FUNCTION (this);

  return m_appStats;
}

void
SvelteServer::SetClient (Ptr<SvelteClient> clientApp, Address clientAddress)
{
  NS_LOG_FUNCTION (this << clientApp << clientAddress);

  m_clientApp = clientApp;
  m_clientAddress = clientAddress;
}

void
SvelteServer::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_appStats = 0;
  m_socket = 0;
  m_clientApp = 0;
  Application::DoDispose ();
}

void
SvelteServer::NotifyStart ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("Starting server application.");

  // Reset internal statistics.
  ResetAppStats ();
}

void
SvelteServer::NotifyForceStop ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("Forcing the server application to stop.");
}

uint32_t
SvelteServer::NotifyTx (uint32_t txBytes)
{
  NS_LOG_FUNCTION (this << txBytes);

  NS_ASSERT_MSG (m_clientApp, "Client application undefined.");
  return m_clientApp->m_appStats->NotifyTx (txBytes);
}

void
SvelteServer::NotifyRx (uint32_t rxBytes, Time timestamp)
{
  NS_LOG_FUNCTION (this << rxBytes << timestamp);

  m_appStats->NotifyRx (rxBytes, timestamp);
}

void
SvelteServer::ResetAppStats ()
{
  NS_LOG_FUNCTION (this);

  m_appStats->ResetCounters ();
}

} // namespace ns3
