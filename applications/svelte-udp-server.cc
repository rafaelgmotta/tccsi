/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2018 University of Campinas (Unicamp)
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
#include "svelte-udp-server.h"

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT \
  std::clog << "[" << GetAppName ()                       \
            << " server teid " << GetTeidHex () << "] ";

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SvelteUdpServer");
NS_OBJECT_ENSURE_REGISTERED (SvelteUdpServer);

TypeId
SvelteUdpServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SvelteUdpServer")
    .SetParent<SvelteServer> ()
    .AddConstructor<SvelteUdpServer> ()

    // These attributes must be configured for the desired traffic pattern.
    .AddAttribute ("PktInterval",
                   "A random variable used to pick the packet "
                   "inter-arrival time [s].",
                   StringValue ("ns3::ConstantRandomVariable[Constant=1]"),
                   MakePointerAccessor (&SvelteUdpServer::m_pktInterRng),
                   MakePointerChecker <RandomVariableStream> ())
    .AddAttribute ("PktSize",
                   "A random variable used to pick the packet size [bytes].",
                   StringValue ("ns3::ConstantRandomVariable[Constant=100]"),
                   MakePointerAccessor (&SvelteUdpServer::m_pktSizeRng),
                   MakePointerChecker <RandomVariableStream> ())
  ;
  return tid;
}

SvelteUdpServer::SvelteUdpServer ()
  : m_sendEvent (EventId ())
{
  NS_LOG_FUNCTION (this);
}

SvelteUdpServer::~SvelteUdpServer ()
{
  NS_LOG_FUNCTION (this);
}

void
SvelteUdpServer::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_sendEvent.Cancel ();
  SvelteServer::DoDispose ();
}

void
SvelteUdpServer::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_INFO ("Opening the UDP socket.");
  TypeId udpFactory = TypeId::LookupByName ("ns3::UdpSocketFactory");
  m_socket = Socket::CreateSocket (GetNode (), udpFactory);
  m_socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), m_localPort));
  m_socket->Connect (InetSocketAddress::ConvertFrom (m_clientAddress));
  m_socket->SetRecvCallback (
    MakeCallback (&SvelteUdpServer::ReadPacket, this));
}

void
SvelteUdpServer::StopApplication ()
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
SvelteUdpServer::NotifyStart ()
{
  NS_LOG_FUNCTION (this);

  // Chain up to reset statistics.
  SvelteServer::NotifyStart ();

  // Start traffic.
  m_sendEvent.Cancel ();
  m_sendEvent = Simulator::Schedule (Seconds (m_pktInterRng->GetValue ()),
                                     &SvelteUdpServer::SendPacket, this);
}

void
SvelteUdpServer::NotifyForceStop ()
{
  NS_LOG_FUNCTION (this);

  // Chain up just for log.
  SvelteServer::NotifyForceStop ();

  // Stop traffic.
  m_sendEvent.Cancel ();
}

void
SvelteUdpServer::SendPacket ()
{
  NS_LOG_FUNCTION (this);

  Ptr<Packet> packet = Create<Packet> (m_pktSizeRng->GetValue ());

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
  m_sendEvent = Simulator::Schedule (Seconds (m_pktInterRng->GetValue ()),
                                     &SvelteUdpServer::SendPacket, this);
}

void
SvelteUdpServer::ReadPacket (Ptr<Socket> socket)
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
