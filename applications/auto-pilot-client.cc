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
#include "auto-pilot-client.h"

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT \
  std::clog << "[Pilot client teid " << GetTeidHex () << "] ";

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("AutoPilotClient");
NS_OBJECT_ENSURE_REGISTERED (AutoPilotClient);

TypeId
AutoPilotClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::AutoPilotClient")
    .SetParent<SvelteClientApp> ()
    .AddConstructor<AutoPilotClient> ()
    //
    // For traffic length, we are using a synthetic average length of 90
    // seconds with 10secs stdev. This will force the application to
    // periodically stop and report statistics.
    //
    .AddAttribute ("TrafficLength",
                   "A random variable used to pick the traffic length [s].",
                   StringValue (
                     "ns3::NormalRandomVariable[Mean=90.0|Variance=100.0]"),
                   MakePointerAccessor (&AutoPilotClient::m_lengthRng),
                   MakePointerChecker <RandomVariableStream> ())
  ;
  return tid;
}

AutoPilotClient::AutoPilotClient ()
  : m_sendEvent (EventId ()),
  m_stopEvent (EventId ())
{
  NS_LOG_FUNCTION (this);

  // The client sends a 1KB packet with uniformly distributed average time
  // between packets ranging from 0.025 to 0.1 sec.
  m_pktSize = 1024;
  m_intervalRng = CreateObject<UniformRandomVariable> ();
  m_intervalRng->SetAttribute ("Min", DoubleValue (0.025));
  m_intervalRng->SetAttribute ("Max", DoubleValue (0.1));
}

AutoPilotClient::~AutoPilotClient ()
{
  NS_LOG_FUNCTION (this);
}

void
AutoPilotClient::Start ()
{
  NS_LOG_FUNCTION (this);

  // Schedule the ForceStop method to stop traffic generation on both sides
  // based on call length.
  Time sTime = Seconds (std::abs (m_lengthRng->GetValue ()));
  m_stopEvent = Simulator::Schedule (sTime, &AutoPilotClient::ForceStop, this);
  NS_LOG_INFO ("Set traffic length to " << sTime.GetSeconds () << "s.");

  // Chain up to reset statistics, notify server, and fire start trace source.
  SvelteClientApp::Start ();

  // Start traffic.
  m_sendEvent.Cancel ();
  m_sendEvent = Simulator::Schedule (Seconds (m_intervalRng->GetValue ()),
                                     &AutoPilotClient::SendPacket, this);
}

void
AutoPilotClient::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_lengthRng = 0;
  m_stopEvent.Cancel ();
  m_sendEvent.Cancel ();
  SvelteClientApp::DoDispose ();
}

void
AutoPilotClient::NotifyConstructionCompleted (void)
{
  NS_LOG_FUNCTION (this);

  SetAttribute ("AppName", StringValue ("Pilot"));

  SvelteClientApp::NotifyConstructionCompleted ();
}

void
AutoPilotClient::ForceStop ()
{
  NS_LOG_FUNCTION (this);

  // Cancel (possible) pending stop event and stop the traffic.
  m_stopEvent.Cancel ();
  m_sendEvent.Cancel ();

  // Chain up to notify server.
  SvelteClientApp::ForceStop ();

  // Notify the stopped application one second later.
  Simulator::Schedule (Seconds (1), &AutoPilotClient::NotifyStop, this, false);
}

void
AutoPilotClient::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_INFO ("Opening the UDP socket.");
  TypeId udpFactory = TypeId::LookupByName ("ns3::UdpSocketFactory");
  m_socket = Socket::CreateSocket (GetNode (), udpFactory);
  m_socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), m_localPort));
  m_socket->Connect (InetSocketAddress::ConvertFrom (m_serverAddress));
  m_socket->SetRecvCallback (
    MakeCallback (&AutoPilotClient::ReadPacket, this));
}

void
AutoPilotClient::StopApplication ()
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
AutoPilotClient::SendPacket ()
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
  m_sendEvent = Simulator::Schedule (Seconds (m_intervalRng->GetValue ()),
                                     &AutoPilotClient::SendPacket, this);
}

void
AutoPilotClient::ReadPacket (Ptr<Socket> socket)
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
