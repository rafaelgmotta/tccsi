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

#include "buffered-video-server.h"
#include <cstdlib>
#include <cstdio>
#include <fstream>

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT \
  std::clog << "[BuffVid server teid " << GetTeidHex () << "] ";

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BufferedVideoServer");
NS_OBJECT_ENSURE_REGISTERED (BufferedVideoServer);

/**
 * \brief Default trace to send.
 */
struct BufferedVideoServer::TraceEntry
BufferedVideoServer::g_defaultEntries[] =
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
BufferedVideoServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BufferedVideoServer")
    .SetParent<SvelteServer> ()
    .AddConstructor<BufferedVideoServer> ()
    .AddAttribute ("TraceFilename",
                   "Name of file to load a trace from.",
                   StringValue (""),
                   MakeStringAccessor (&BufferedVideoServer::SetTraceFile),
                   MakeStringChecker ())
    //
    // For traffic length, we are considering a statistic that the majority of
    // YouTube brand videos are somewhere between 31 and 120 seconds long. So
    // we are using the average length of 1min 30sec, with 15sec stdev. See
    // http://tinyurl.com/q5xkwnn and http://tinyurl.com/klraxum for more
    // information on this topic. Note that this length will only be used to
    // get the size of the video which will be sent to the client over a TCP
    // connection.
    //
    .AddAttribute ("TrafficLength",
                   "A random variable used to pick the traffic length [s].",
                   StringValue (
                     "ns3::NormalRandomVariable[Mean=90.0|Variance=225.0]"),
                   MakePointerAccessor (&BufferedVideoServer::m_lengthRng),
                   MakePointerChecker <RandomVariableStream> ())
  ;
  return tid;
}

BufferedVideoServer::BufferedVideoServer ()
  : m_connected (false),
  m_pendingBytes (0),
  m_chunkSize (128000)
{
  NS_LOG_FUNCTION (this);
}

BufferedVideoServer::~BufferedVideoServer ()
{
  NS_LOG_FUNCTION (this);
}

void
BufferedVideoServer::SetTraceFile (std::string traceFile)
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
BufferedVideoServer::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_lengthRng = 0;
  m_entries.clear ();
  SvelteServer::DoDispose ();
}

void
BufferedVideoServer::StartApplication ()
{
  NS_LOG_FUNCTION (this);

  NS_LOG_INFO ("Creating the listening TCP socket.");
  TypeId tcpFactory = TypeId::LookupByName ("ns3::TcpSocketFactory");
  m_socket = Socket::CreateSocket (GetNode (), tcpFactory);
  m_socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), m_localPort));
  m_socket->Listen ();
  m_socket->SetAcceptCallback (
    MakeCallback (&BufferedVideoServer::NotifyConnectionRequest, this),
    MakeCallback (&BufferedVideoServer::NotifyNewConnectionCreated, this));
  m_socket->SetCloseCallbacks (
    MakeCallback (&BufferedVideoServer::NotifyNormalClose, this),
    MakeCallback (&BufferedVideoServer::NotifyErrorClose, this));
}

void
BufferedVideoServer::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0)
    {
      m_socket->Close ();
      m_socket->Dispose ();
      m_socket = 0;
    }
}

bool
BufferedVideoServer::NotifyConnectionRequest (Ptr<Socket> socket,
                                              const Address& address)
{
  NS_LOG_FUNCTION (this << socket << address);

  Ipv4Address ipAddr = InetSocketAddress::ConvertFrom (address).GetIpv4 ();
  uint16_t port = InetSocketAddress::ConvertFrom (address).GetPort ();
  NS_LOG_INFO ("Connection request received from " << ipAddr << ":" << port);

  return !m_connected;
}

void
BufferedVideoServer::NotifyNewConnectionCreated (Ptr<Socket> socket,
                                                 const Address& address)
{
  NS_LOG_FUNCTION (this << socket << address);

  Ipv4Address ipAddr = InetSocketAddress::ConvertFrom (address).GetIpv4 ();
  uint16_t port = InetSocketAddress::ConvertFrom (address).GetPort ();
  NS_LOG_INFO ("Connection established with " << ipAddr << ":" << port);
  m_connected = true;
  m_pendingBytes = 0;

  socket->SetSendCallback (
    MakeCallback (&BufferedVideoServer::SendData, this));
  socket->SetRecvCallback (
    MakeCallback (&BufferedVideoServer::DataReceived, this));
}

void
BufferedVideoServer::NotifyNormalClose (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  NS_LOG_INFO ("Connection successfully closed.");
  socket->ShutdownSend ();
  socket->ShutdownRecv ();
  m_connected = false;
  m_pendingBytes = 0;
}

void
BufferedVideoServer::NotifyErrorClose (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  NS_LOG_WARN ("Connection closed with errors.");
  socket->ShutdownSend ();
  socket->ShutdownRecv ();
  m_connected = false;
  m_pendingBytes = 0;
}

void
BufferedVideoServer::DataReceived (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  // This application expects to receive only
  // a single HTTP request message at a time.
  Ptr<Packet> packet = socket->Recv ();
  NotifyRx (packet->GetSize ());

  HttpHeader httpHeaderRequest;
  packet->RemoveHeader (httpHeaderRequest);
  NS_ASSERT_MSG (httpHeaderRequest.IsRequest (), "Invalid HTTP request.");
  NS_ASSERT_MSG (packet->GetSize () == 0, "Invalid RX data.");

  ProccessHttpRequest (socket, httpHeaderRequest);
}

void
BufferedVideoServer::SendData (Ptr<Socket> socket, uint32_t available)
{
  NS_LOG_FUNCTION (this << socket << available);

  if (!m_pendingBytes)
    {
      NS_LOG_DEBUG ("No pending data to send.");
      return;
    }

  uint32_t pktSize = std::min (available, m_pendingBytes);
  Ptr<Packet> packet = Create<Packet> (pktSize);
  int bytes = socket->Send (packet);
  if (bytes > 0)
    {
      NS_LOG_DEBUG ("Server TX " << bytes << " bytes.");
      m_pendingBytes -= static_cast<uint32_t> (bytes);
    }
  else
    {
      NS_LOG_ERROR ("Server TX error.");
    }
}

void
BufferedVideoServer::ProccessHttpRequest (Ptr<Socket> socket,
                                          HttpHeader header)
{
  NS_LOG_FUNCTION (this << socket);

  // Check for requested URL.
  std::string url = header.GetRequestUrl ();
  NS_LOG_INFO ("Client requested " << url);
  if (url == "main/video")
    {
      // Set parameter values.
      m_pendingBytes = m_chunkSize;
      Time videoLength = Seconds (std::abs (m_lengthRng->GetValue ()));
      uint32_t numChunks = GetVideoChunks (videoLength) - 1;
      NS_LOG_INFO ("Video with " << numChunks << " chunks of " <<
                   m_chunkSize << " bytes each.");

      // Set the response message.
      HttpHeader httpHeaderOut;
      httpHeaderOut.SetResponse ();
      httpHeaderOut.SetVersion ("HTTP/1.1");
      httpHeaderOut.SetResponseStatusCode ("200");
      httpHeaderOut.SetResponsePhrase ("OK");
      httpHeaderOut.SetHeaderField ("ContentLength", m_pendingBytes);
      httpHeaderOut.SetHeaderField ("ContentType", "main/video");
      httpHeaderOut.SetHeaderField ("InlineObjects", numChunks);

      Ptr<Packet> outPacket = Create<Packet> (0);
      outPacket->AddHeader (httpHeaderOut);

      NotifyTx (outPacket->GetSize () + m_pendingBytes);
      int bytes = socket->Send (outPacket);
      if (bytes != static_cast<int> (outPacket->GetSize ()))
        {
          NS_LOG_ERROR ("Not all bytes were copied to the socket buffer.");
        }

      // Start sending the payload.
      SendData (socket, socket->GetTxAvailable ());
    }
  else if (url == "video/chunk")
    {
      m_pendingBytes = m_chunkSize;
      NS_LOG_DEBUG ("Video chunk size (bytes): " << m_pendingBytes);

      // Set the response message.
      HttpHeader httpHeaderOut;
      httpHeaderOut.SetResponse ();
      httpHeaderOut.SetVersion ("HTTP/1.1");
      httpHeaderOut.SetResponseStatusCode ("200");
      httpHeaderOut.SetResponsePhrase ("OK");
      httpHeaderOut.SetHeaderField ("ContentLength", m_pendingBytes);
      httpHeaderOut.SetHeaderField ("ContentType", "video/chunk");
      httpHeaderOut.SetHeaderField ("InlineObjects", 0);

      Ptr<Packet> outPacket = Create<Packet> (0);
      outPacket->AddHeader (httpHeaderOut);

      NotifyTx (outPacket->GetSize () + m_pendingBytes);
      int bytes = socket->Send (outPacket);
      if (bytes != static_cast<int> (outPacket->GetSize ()))
        {
          NS_LOG_ERROR ("Not all bytes were copied to the socket buffer.");
        }

      // Start sending the payload.
      SendData (socket, socket->GetTxAvailable ());
    }
  else
    {
      NS_FATAL_ERROR ("Invalid URL requested.");
    }
}

void
BufferedVideoServer::LoadTrace (std::string filename)
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
BufferedVideoServer::LoadDefaultTrace (void)
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

uint32_t
BufferedVideoServer::GetVideoChunks (Time length)
{
  uint32_t currentEntry = 0;
  Time elapsed = Seconds (0);
  struct TraceEntry *entry;
  uint32_t total = 0;
  while (elapsed < length)
    {
      entry = &m_entries[currentEntry];
      total += entry->packetSize;
      elapsed += MilliSeconds (entry->timeToSend);
      currentEntry++;
      currentEntry = currentEntry % m_entries.size ();
    }
  return total / m_chunkSize;
}

} // Namespace ns3
