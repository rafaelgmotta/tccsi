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

#include "application-helper.h"

namespace ns3 {

ApplicationHelper::ApplicationHelper ()
{
  m_clientFactory.SetTypeId (SvelteClient::GetTypeId ());
  m_serverFactory.SetTypeId (SvelteServer::GetTypeId ());
}

ApplicationHelper::ApplicationHelper (TypeId clientType, TypeId serverType)
{
  m_clientFactory.SetTypeId (clientType);
  m_serverFactory.SetTypeId (serverType);
}

void
ApplicationHelper::SetClientAttribute (std::string name,
                                       const AttributeValue &value)
{
  m_clientFactory.Set (name, value);
}

void
ApplicationHelper::SetServerAttribute (std::string name,
                                       const AttributeValue &value)
{
  m_serverFactory.Set (name, value);
}

Ptr<SvelteClient>
ApplicationHelper::Install (Ptr<Node> clientNode, Ptr<Node> serverNode,
                            Ipv4Address clientAddr, Ipv4Address serverAddr,
                            uint16_t port, Ipv4Header::DscpType dscp)
{
  Ptr<SvelteClient> clientApp;
  clientApp = m_clientFactory.Create ()->GetObject<SvelteClient> ();
  NS_ASSERT_MSG (clientApp, "Invalid client type id.");

  Ptr<SvelteServer> serverApp;
  serverApp = m_serverFactory.Create ()->GetObject<SvelteServer> ();
  NS_ASSERT_MSG (serverApp, "Invalid server type id.");

  InetSocketAddress serverInetAddr (serverAddr, port);
  // serverInetAddr.SetTos (Dscp2Tos (dscp));
  clientApp->SetAttribute ("LocalPort", UintegerValue (port));
  clientApp->SetServer (serverApp, serverInetAddr);
  clientNode->AddApplication (clientApp);

  InetSocketAddress clientInetAddr (clientAddr, port);
  // clientInetAddr.SetTos (Dscp2Tos (dscp));
  serverApp->SetAttribute ("LocalPort", UintegerValue (port));
  serverApp->SetClient (clientApp, clientInetAddr);
  serverNode->AddApplication (serverApp);

  return clientApp;
}

} // namespace ns3
