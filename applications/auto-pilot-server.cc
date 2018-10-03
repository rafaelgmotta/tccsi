/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017 University of Campinas (Unicamp)
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Luciano Chaves <luciano@lrc.ic.unicamp.br>
 */

#include <ns3/seq-ts-header.h>
#include "auto-pilot-server.h"

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT \
  std::clog << "[Pilot server teid " << GetTeidHex () << "] ";

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("AutoPilotServer");
NS_OBJECT_ENSURE_REGISTERED (AutoPilotServer);

TypeId
AutoPilotServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::AutoPilotServer")
    .SetParent<SvelteServerApp> ()
    .AddConstructor<AutoPilotServer> ()
  ;
  return tid;
}

AutoPilotServer::AutoPilotServer ()
  : m_sendEvent (EventId ())
{
  NS_LOG_FUNCTION (this);

  // The server sends a 1KB packet with uniformly distributed average time
  // between packets ranging from 0.999 to 1.001 sec.
  m_pktSize = 1024;
  m_intervalRng = CreateObject<UniformRandomVariable> ();
  m_intervalRng->SetAttribute ("Min", DoubleValue (0.999));
  m_intervalRng->SetAttribute ("Max", DoubleValue (1.001));
}

AutoPilotServer::~AutoPilotServer ()
{
  NS_LOG_FUNCTION (this);
}

void
AutoPilotServer::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_sendEvent.Cancel ();
  SvelteServerApp::DoDispose ();
}

void
AutoPilotServer::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_INFO ("Opening the UDP socket.");
  TypeId udpFactory = TypeId::LookupByName ("ns3::UdpSocketFactory");
  m_socket = Socket::CreateSocket (GetNode (), udpFactory);
  m_socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), m_localPort));
  m_socket->Connect (InetSocketAddress::ConvertFrom (m_clientAddress));
  m_socket->SetRecvCallback (
    MakeCallback (&AutoPilotServer::ReadPacket, this));
}

void
AutoPilotServer::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0)
    {
      m_socket->Close ();
      m_socket->Dispose ();
      m_socket = 0;
    }
}

void
AutoPilotServer::NotifyStart ()
{
  NS_LOG_FUNCTION (this);

  // Chain up to reset statistics.
  SvelteServerApp::NotifyStart ();

  // Start traffic.
  m_sendEvent.Cancel ();
  m_sendEvent = Simulator::Schedule (Seconds (m_intervalRng->GetValue ()),
                                     &AutoPilotServer::SendPacket, this);
}

void
AutoPilotServer::NotifyForceStop ()
{
  NS_LOG_FUNCTION (this);

  // Chain up just for log.
  SvelteServerApp::NotifyForceStop ();

  // Stop traffic.
  m_sendEvent.Cancel ();
}

void
AutoPilotServer::SendPacket ()
{
  NS_LOG_FUNCTION (this);

  Ptr<Packet> packet = Create<Packet> (m_pktSize);

  SeqTsHeader seqTs;
  seqTs.SetSeq (NotifyTx (packet->GetSize () + seqTs.GetSerializedSize ()));
  packet->AddHeader (seqTs);

  int bytes = m_socket->Send (packet);
  if (bytes == static_cast<int> (packet->GetSize ()))
    {
      NS_LOG_DEBUG ("Server TX " << bytes << " bytes with " <<
                    "sequence number " << seqTs.GetSeq ());
    }
  else
    {
      NS_LOG_ERROR ("Server TX error.");
    }

  // Schedule next packet transmission.
  m_sendEvent = Simulator::Schedule (Seconds (m_intervalRng->GetValue ()),
                                     &AutoPilotServer::SendPacket, this);
}

void
AutoPilotServer::ReadPacket (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  // Receive the datagram from the socket.
  Ptr<Packet> packet = socket->Recv ();

  SeqTsHeader seqTs;
  packet->PeekHeader (seqTs);
  NotifyRx (packet->GetSize (), seqTs.GetTs ());
  NS_LOG_DEBUG ("Server RX " << packet->GetSize () << " bytes with " <<
                "sequence number " << seqTs.GetSeq ());
}

} // Namespace ns3
