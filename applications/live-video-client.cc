/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 University of Campinas (Unicamp)
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

#include <ns3/seq-ts-header.h>
#include "live-video-client.h"

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT \
  std::clog << "[LiveVid client teid " << GetTeidHex () << "] ";

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LiveVideoClient");
NS_OBJECT_ENSURE_REGISTERED (LiveVideoClient);

TypeId
LiveVideoClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LiveVideoClient")
    .SetParent<SvelteClientApp> ()
    .AddConstructor<LiveVideoClient> ()
    //
    // For traffic length, we are considering a statistic that the majority of
    // YouTube brand videos are somewhere between 31 and 120 seconds long. So
    // we are using the average length of 1min 30sec, with 15sec stdev. See
    // http://tinyurl.com/q5xkwnn and http://tinyurl.com/klraxum for more
    // information on this topic.
    //
    .AddAttribute ("TrafficLength",
                   "A random variable used to pick the traffic length [s].",
                   StringValue (
                     "ns3::NormalRandomVariable[Mean=90.0|Variance=225.0]"),
                   MakePointerAccessor (&LiveVideoClient::m_lengthRng),
                   MakePointerChecker <RandomVariableStream> ())
  ;
  return tid;
}

LiveVideoClient::LiveVideoClient ()
  : m_stopEvent (EventId ())
{
  NS_LOG_FUNCTION (this);
}

LiveVideoClient::~LiveVideoClient ()
{
  NS_LOG_FUNCTION (this);
}

void
LiveVideoClient::Start ()
{
  NS_LOG_FUNCTION (this);

  // Schedule the ForceStop method to stop traffic generation on server side
  // based on video length.
  Time sTime = Seconds (std::abs (m_lengthRng->GetValue ()));
  m_stopEvent = Simulator::Schedule (sTime, &LiveVideoClient::ForceStop, this);
  NS_LOG_INFO ("Set traffic length to " << sTime.GetSeconds () << "s.");

  // Chain up to reset statistics, notify server, and fire start trace source.
  SvelteClientApp::Start ();
}

void
LiveVideoClient::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_lengthRng = 0;
  m_stopEvent.Cancel ();
  SvelteClientApp::DoDispose ();
}

void
LiveVideoClient::NotifyConstructionCompleted (void)
{
  NS_LOG_FUNCTION (this);

  SetAttribute ("AppName", StringValue ("LiveVid"));

  SvelteClientApp::NotifyConstructionCompleted ();
}

void
LiveVideoClient::ForceStop ()
{
  NS_LOG_FUNCTION (this);

  // Cancel (possible) pending stop event.
  m_stopEvent.Cancel ();

  // Chain up to notify server.
  SvelteClientApp::ForceStop ();

  // Notify the stopped application one second later.
  Simulator::Schedule (Seconds (1), &LiveVideoClient::NotifyStop, this, false);
}

void
LiveVideoClient::StartApplication ()
{
  NS_LOG_FUNCTION (this);

  NS_LOG_INFO ("Opening the UDP socket.");
  TypeId udpFactory = TypeId::LookupByName ("ns3::UdpSocketFactory");
  m_socket = Socket::CreateSocket (GetNode (), udpFactory);
  m_socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), m_localPort));
  m_socket->ShutdownSend ();
  m_socket->SetRecvCallback (
    MakeCallback (&LiveVideoClient::ReadPacket, this));
}

void
LiveVideoClient::StopApplication ()
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
LiveVideoClient::ReadPacket (Ptr<Socket> socket)
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
