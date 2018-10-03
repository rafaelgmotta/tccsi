/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013 Federal University of Uberlandia
 *               2014 University of Campinas (Unicamp)
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
 * Author: Saulo da Mata <damata.saulo@gmail.com>
 *         Luciano Chaves <luciano@lrc.ic.unicamp.br>
 */

#include <ns3/seq-ts-header.h>
#include "voip-client.h"

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT \
  std::clog << "[Voip client teid " << GetTeidHex () << "] ";

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("VoipClient");
NS_OBJECT_ENSURE_REGISTERED (VoipClient);

TypeId
VoipClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::VoipClient")
    .SetParent<SvelteClientApp> ()
    .AddConstructor<VoipClient> ()
    //
    // For traffic length, we are considering an estimative from Vodafone that
    // the average call length is 1 min and 40 sec. We are including a normal
    // standard deviation of 10 sec. See http://tinyurl.com/pzmyys2 and
    // http://www.theregister.co.uk/2013/01/30/mobile_phone_calls_shorter for
    // more information on this topic.
    //
    .AddAttribute ("TrafficLength",
                   "A random variable used to pick the traffic length [s].",
                   StringValue (
                     "ns3::NormalRandomVariable[Mean=100.0|Variance=100.0]"),
                   MakePointerAccessor (&VoipClient::m_lengthRng),
                   MakePointerChecker <RandomVariableStream> ())
  ;
  return tid;
}

VoipClient::VoipClient ()
  : m_sendEvent (EventId ()),
  m_stopEvent (EventId ())
{
  NS_LOG_FUNCTION (this);

  // VoIP traffic simulating the G.729 codec (~8.0 kbps for payload).
  // The client sends a 20B packet every 0.02 sec.
  m_pktSize = 20;
  m_interval = Seconds (0.02);
}

VoipClient::~VoipClient ()
{
  NS_LOG_FUNCTION (this);
}

void
VoipClient::Start ()
{
  NS_LOG_FUNCTION (this);

  // Schedule the ForceStop method to stop traffic generation on both sides
  // based on call length.
  Time sTime = Seconds (std::abs (m_lengthRng->GetValue ()));
  m_stopEvent = Simulator::Schedule (sTime, &VoipClient::ForceStop, this);
  NS_LOG_INFO ("Set traffic length to " << sTime.GetSeconds () << "s.");

  // Chain up to reset statistics, notify server, and fire start trace source.
  SvelteClientApp::Start ();

  // Start traffic.
  m_sendEvent.Cancel ();
  m_sendEvent = Simulator::Schedule (
      m_interval, &VoipClient::SendPacket, this);
}

void
VoipClient::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_lengthRng = 0;
  m_stopEvent.Cancel ();
  m_sendEvent.Cancel ();
  SvelteClientApp::DoDispose ();
}

void
VoipClient::NotifyConstructionCompleted (void)
{
  NS_LOG_FUNCTION (this);

  SetAttribute ("AppName", StringValue ("Voip"));

  SvelteClientApp::NotifyConstructionCompleted ();
}

void
VoipClient::ForceStop ()
{
  NS_LOG_FUNCTION (this);

  // Cancel (possible) pending stop event and stop the traffic.
  m_stopEvent.Cancel ();
  m_sendEvent.Cancel ();

  // Chain up to notify server.
  SvelteClientApp::ForceStop ();

  // Notify the stopped application one second later.
  Simulator::Schedule (Seconds (1), &VoipClient::NotifyStop, this, false);
}

void
VoipClient::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_INFO ("Opening the UDP socket.");
  TypeId udpFactory = TypeId::LookupByName ("ns3::UdpSocketFactory");
  m_socket = Socket::CreateSocket (GetNode (), udpFactory);
  m_socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), m_localPort));
  m_socket->Connect (InetSocketAddress::ConvertFrom (m_serverAddress));
  m_socket->SetRecvCallback (MakeCallback (&VoipClient::ReadPacket, this));
}

void
VoipClient::StopApplication ()
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
VoipClient::SendPacket ()
{
  NS_LOG_FUNCTION (this);

  Ptr<Packet> packet = Create<Packet> (m_pktSize);

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
  m_sendEvent = Simulator::Schedule (
      m_interval, &VoipClient::SendPacket, this);
}

void
VoipClient::ReadPacket (Ptr<Socket> socket)
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
