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
#include "live-video-server.h"
#include <cstdlib>
#include <cstdio>
#include <fstream>

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT \
  std::clog << "[LiveVid server teid " << GetTeidHex () << "] ";

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LiveVideoServer");
NS_OBJECT_ENSURE_REGISTERED (LiveVideoServer);

/**
 * \brief Default trace to send.
 */
struct LiveVideoServer::TraceEntry
LiveVideoServer::g_defaultEntries[] =
{
  {
    0,  534, 'I'
  },
  {
    40, 1542, 'P'
  },
  {
    120,  134, 'B'
  },
  {
    80,  390, 'B'
  },
  {
    240,  765, 'P'
  },
  {
    160,  407, 'B'
  },
  {
    200,  504, 'B'
  },
  {
    360,  903, 'P'
  },
  {
    280,  421, 'B'
  },
  {
    320,  587, 'B'
  }
};

TypeId
LiveVideoServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LiveVideoServer")
    .SetParent<SvelteServerApp> ()
    .AddConstructor<LiveVideoServer> ()
    .AddAttribute ("MaxPayloadSize",
                   "The maximum payload size of packets [bytes].",
                   UintegerValue (1400),
                   MakeUintegerAccessor (&LiveVideoServer::m_pktSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("TraceFilename",
                   "Name of file to load a trace from.",
                   StringValue (""),
                   MakeStringAccessor (&LiveVideoServer::SetTraceFile),
                   MakeStringChecker ())
  ;
  return tid;
}

LiveVideoServer::LiveVideoServer ()
  : m_sendEvent (EventId ())
{
  NS_LOG_FUNCTION (this);
}

LiveVideoServer::~LiveVideoServer ()
{
  NS_LOG_FUNCTION (this);
}

void
LiveVideoServer::SetTraceFile (std::string traceFile)
{
  NS_LOG_FUNCTION (this << traceFile);

  if (traceFile == "")
    {
      LoadDefaultTrace ();
    }
  else
    {
      LoadTrace (traceFile);
    }
}

void
LiveVideoServer::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_sendEvent.Cancel ();
  m_entries.clear ();
  SvelteServerApp::DoDispose ();
}

void
LiveVideoServer::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_INFO ("Opening the UDP socket.");
  TypeId udpFactory = TypeId::LookupByName ("ns3::UdpSocketFactory");
  m_socket = Socket::CreateSocket (GetNode (), udpFactory);
  m_socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), m_localPort));
  m_socket->Connect (InetSocketAddress::ConvertFrom (m_clientAddress));
  m_socket->ShutdownRecv ();
}

void
LiveVideoServer::StopApplication ()
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
LiveVideoServer::NotifyStart ()
{
  NS_LOG_FUNCTION (this);

  // Chain up to reset statistics.
  SvelteServerApp::NotifyStart ();

  // Start streaming.
  m_sendEvent.Cancel ();
  m_currentEntry = 0;
  SendStream ();
}

void
LiveVideoServer::NotifyForceStop ()
{
  NS_LOG_FUNCTION (this);

  // Chain up just for log.
  SvelteServerApp::NotifyForceStop ();

  // Stop streaming.
  m_sendEvent.Cancel ();
}

void
LiveVideoServer::LoadTrace (std::string filename)
{
  NS_LOG_FUNCTION (this << filename);

  uint32_t time, index, size, prevTime = 0;
  char frameType;
  TraceEntry entry;
  m_entries.clear ();

  std::ifstream ifTraceFile;
  ifTraceFile.open (filename.c_str (), std::ifstream::in);
  if (!ifTraceFile.good ())
    {
      NS_LOG_WARN ("Trace file not found. Loading default trace.");
      LoadDefaultTrace ();
    }

  while (ifTraceFile.good ())
    {
      ifTraceFile >> index >> frameType >> time >> size;
      if (frameType == 'B')
        {
          entry.timeToSend = 0;
        }
      else
        {
          entry.timeToSend = time - prevTime;
          prevTime = time;
        }
      entry.packetSize = size;
      entry.frameType = frameType;
      m_entries.push_back (entry);
    }
  ifTraceFile.close ();
}

void
LiveVideoServer::LoadDefaultTrace (void)
{
  NS_LOG_FUNCTION (this);

  uint32_t prevTime = 0;
  for (uint32_t i = 0; i < (sizeof (g_defaultEntries) /
                            sizeof (struct TraceEntry)); i++)
    {
      struct TraceEntry entry = g_defaultEntries[i];
      if (entry.frameType == 'B')
        {
          entry.timeToSend = 0;
        }
      else
        {
          uint32_t tmp = entry.timeToSend;
          entry.timeToSend -= prevTime;
          prevTime = tmp;
        }
      m_entries.push_back (entry);
    }
}

void
LiveVideoServer::SendStream (void)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (m_sendEvent.IsExpired ());

  struct TraceEntry *entry = &m_entries[m_currentEntry];
  NS_LOG_DEBUG ("Frame no. " << m_currentEntry <<
                " with " << entry->packetSize << " bytes");
  do
    {
      for (uint32_t i = 0; i < entry->packetSize / m_pktSize; i++)
        {
          SendPacket (m_pktSize);
        }
      uint16_t sizeToSend = entry->packetSize % m_pktSize;
      SendPacket (sizeToSend);

      m_currentEntry++;
      m_currentEntry %= m_entries.size ();
      entry = &m_entries[m_currentEntry];
    }
  while (entry->timeToSend == 0);

  // Schedulle next transmission.
  m_sendEvent = Simulator::Schedule (MilliSeconds (entry->timeToSend),
                                     &LiveVideoServer::SendStream, this);
}

void
LiveVideoServer::SendPacket (uint32_t size)
{
  NS_LOG_FUNCTION (this << size);

  static const uint32_t seqTsSize = 12;

  // Create the packet and add the seq header without increasing packet size.
  uint32_t packetSize = size > seqTsSize ? size - seqTsSize : 0;
  Ptr<Packet> packet = Create<Packet> (packetSize);

  SeqTsHeader seqTs;
  seqTs.SetSeq (NotifyTx (packetSize + seqTsSize));
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
}

} // Namespace ns3
