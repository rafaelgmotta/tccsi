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
#include "svelte-udp-client.h"

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT \
  std::clog << "[" << GetAppName ()                       \
            << " client teid " << GetTeidHex () << "] ";

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SvelteUdpClient");
NS_OBJECT_ENSURE_REGISTERED (SvelteUdpClient);

TypeId
SvelteUdpClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SvelteUdpClient")
    .SetParent<SvelteClient> ()
    .AddConstructor<SvelteUdpClient> ()

    // These attributes must be configured for the desired traffic pattern.
    .AddAttribute ("PktInterval",
                   "A random variable used to pick the packet "
                   "inter-arrival time [s].",
                   StringValue ("ns3::ConstantRandomVariable[Constant=1]"),
                   MakePointerAccessor (&SvelteUdpClient::m_pktInterRng),
                   MakePointerChecker <RandomVariableStream> ())
    .AddAttribute ("PktSize",
                   "A random variable used to pick the packet size [bytes].",
                   StringValue ("ns3::ConstantRandomVariable[Constant=100]"),
                   MakePointerAccessor (&SvelteUdpClient::m_pktSizeRng),
                   MakePointerChecker <RandomVariableStream> ())
    .AddAttribute ("TrafficLength",
                   "A random variable used to pick the traffic length [s].",
                   StringValue ("ns3::ConstantRandomVariable[Constant=30.0]"),
                   MakePointerAccessor (&SvelteUdpClient::m_lengthRng),
                   MakePointerChecker <RandomVariableStream> ())
  ;
  return tid;
}

SvelteUdpClient::SvelteUdpClient ()
  : m_sendEvent (EventId ()),
  m_stopEvent (EventId ())
{
  NS_LOG_FUNCTION (this);
}

SvelteUdpClient::~SvelteUdpClient ()
{
  NS_LOG_FUNCTION (this);
}

void
SvelteUdpClient::Start ()
{
  NS_LOG_FUNCTION (this);

  // Schedule the ForceStop method to stop traffic generation on both sides
  // based on traffic length.
  Time sTime = Seconds (std::abs (m_lengthRng->GetValue ()));
  m_stopEvent = Simulator::Schedule (sTime, &SvelteUdpClient::ForceStop, this);
  NS_LOG_INFO ("Set traffic length to " << sTime.GetSeconds () << "s.");

  // Chain up to reset statistics, notify server, and fire start trace source.
  SvelteClient::Start ();

  // Start traffic.
  m_sendEvent.Cancel ();
  m_sendEvent = Simulator::Schedule (Seconds (m_pktInterRng->GetValue ()),
                                     &SvelteUdpClient::SendPacket, this);
}

void
SvelteUdpClient::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_lengthRng = 0;
  m_stopEvent.Cancel ();
  m_sendEvent.Cancel ();
  SvelteClient::DoDispose ();
}

void
SvelteUdpClient::ForceStop ()
{
  NS_LOG_FUNCTION (this);

  // Cancel (possible) pending stop event and stop the traffic.
  m_stopEvent.Cancel ();
  m_sendEvent.Cancel ();

  // Chain up to notify server.
  SvelteClient::ForceStop ();

  // Notify the stopped application one second later.
  Simulator::Schedule (Seconds (1), &SvelteUdpClient::NotifyStop, this, false);
}

void
SvelteUdpClient::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_INFO ("Opening the UDP socket.");
  TypeId udpFactory = TypeId::LookupByName ("ns3::UdpSocketFactory");
  m_socket = Socket::CreateSocket (GetNode (), udpFactory);
  m_socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), m_localPort));
  m_socket->Connect (InetSocketAddress::ConvertFrom (m_serverAddress));
  m_socket->SetRecvCallback (
    MakeCallback (&SvelteUdpClient::ReadPacket, this));
}

void
SvelteUdpClient::StopApplication ()
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
SvelteUdpClient::SendPacket ()
{
  NS_LOG_FUNCTION (this);

  Ptr<Packet> packet = Create<Packet> (m_pktSizeRng->GetValue ());

  SeqTsHeader seqTs;
  seqTs.SetSeq (NotifyTx (packet->GetSize () + seqTs.GetSerializedSize ()));
  packet->AddHeader (seqTs);

  int bytes = m_socket->Send (packet);
  if (bytes == static_cast<int> (packet->GetSize ()))
    {
      NS_LOG_DEBUG ("Client TX " << bytes << " bytes with " <<
                    "sequence number " << seqTs.GetSeq ());
    }
  else
    {
      NS_LOG_ERROR ("Client TX error.");
    }

  // Schedule next packet transmission.
  m_sendEvent = Simulator::Schedule (Seconds (m_pktInterRng->GetValue ()),
                                     &SvelteUdpClient::SendPacket, this);
}

void
SvelteUdpClient::ReadPacket (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  // Receive the datagram from the socket.
  Ptr<Packet> packet = socket->Recv ();

  SeqTsHeader seqTs;
  packet->PeekHeader (seqTs);
  NotifyRx (packet->GetSize (), seqTs.GetTs ());
  NS_LOG_DEBUG ("Client RX " << packet->GetSize () << " bytes with " <<
                "sequence number " << seqTs.GetSeq ());
}

} // Namespace ns3
