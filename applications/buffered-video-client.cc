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

#include "buffered-video-client.h"

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT \
  std::clog << "[BuffVid client teid " << GetTeidHex () << "] ";

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BufferedVideoClient");
NS_OBJECT_ENSURE_REGISTERED (BufferedVideoClient);

TypeId
BufferedVideoClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BufferedVideoClient")
    .SetParent<SvelteClient> ()
    .AddConstructor<BufferedVideoClient> ()
  ;
  return tid;
}

BufferedVideoClient::BufferedVideoClient ()
  : m_rxPacket (0),
  m_pendingBytes (0),
  m_pendingObjects (0)
{
  NS_LOG_FUNCTION (this);
}

BufferedVideoClient::~BufferedVideoClient ()
{
  NS_LOG_FUNCTION (this);
}

void
BufferedVideoClient::Start ()
{
  NS_LOG_FUNCTION (this);

  // Chain up to reset statistics, notify server, and fire start trace source.
  SvelteClient::Start ();

  NS_LOG_INFO ("Opening the TCP connection.");
  TypeId tcpFactory = TypeId::LookupByName ("ns3::TcpSocketFactory");
  m_socket = Socket::CreateSocket (GetNode (), tcpFactory);
  m_socket->Bind ();
  m_socket->Connect (InetSocketAddress::ConvertFrom (m_serverAddress));
  m_socket->SetConnectCallback (
    MakeCallback (&BufferedVideoClient::NotifyConnectionSucceeded, this),
    MakeCallback (&BufferedVideoClient::NotifyConnectionFailed, this));
}

void
BufferedVideoClient::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_rxPacket = 0;
  SvelteClient::DoDispose ();
}

void
BufferedVideoClient::NotifyConstructionCompleted (void)
{
  NS_LOG_FUNCTION (this);

  SetAttribute ("AppName", StringValue ("BufVideo"));

  SvelteClient::NotifyConstructionCompleted ();
}

void
BufferedVideoClient::NotifyConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  NS_LOG_INFO ("Server accepted connection request.");
  socket->SetRecvCallback (
    MakeCallback (&BufferedVideoClient::DataReceived, this));

  m_pendingBytes = 0;
  m_pendingObjects = 0;
  m_rxPacket = Create<Packet> (0);

  // Request the first object.
  SendRequest (socket, "main/video");
}

void
BufferedVideoClient::NotifyConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  NS_FATAL_ERROR ("Server refused connection request!");
}

void
BufferedVideoClient::DataReceived (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  static std::string contentTypeStr = "";
  static uint32_t httpPacketSize = 0;

  // Repeat while we have data to process.
  while (socket->GetRxAvailable () || m_rxPacket->GetSize ())
    {
      // Get (more) data from socket, if available.
      if (socket->GetRxAvailable ())
        {
          m_rxPacket->AddAtEnd (socket->Recv ());
        }

      // This is the start of a new HTTP message.
      if (!m_pendingBytes)
        {
          // Check for valid HTTP header.
          HttpHeader httpHeader;
          m_rxPacket->RemoveHeader (httpHeader);
          NS_ASSERT (httpHeader.GetResponseStatusCode () == "200");
          httpPacketSize = httpHeader.GetSerializedSize ();

          // Get the content length for this message.
          std::string length = httpHeader.GetHeaderField ("ContentLength");
          m_pendingBytes = std::atoi (length.c_str ());
          httpPacketSize += m_pendingBytes;

          // Get the number of inline objects to load.
          contentTypeStr = httpHeader.GetHeaderField ("ContentType");
          if (contentTypeStr == "main/video")
            {
              std::string inObjs = httpHeader.GetHeaderField ("InlineObjects");
              m_pendingObjects = std::atoi (inObjs.c_str ());
            }
        }

      // Let's consume received data.
      uint32_t consume = std::min (m_rxPacket->GetSize (), m_pendingBytes);
      m_rxPacket->RemoveAtStart (consume);
      m_pendingBytes -= consume;
      NS_LOG_DEBUG ("Client RX " << consume << " bytes.");

      // This is the end of the HTTP message.
      if (!m_pendingBytes)
        {
          NS_ASSERT (m_rxPacket->GetSize () == 0);
          NS_LOG_DEBUG (contentTypeStr << " successfully received.");
          NotifyRx (httpPacketSize);

          // Check for a successfully received object.
          if (contentTypeStr == "video/chunk")
            {
              m_pendingObjects--;
            }

          if (m_pendingObjects && !IsForceStop ())
            {
              // Request for the next object and continue.
              NS_LOG_DEBUG ("Request for chunk/video " << m_pendingObjects);
              SendRequest (socket, "video/chunk");
              continue;
            }

          // Why are we not requesting for more objects?
          if (!m_pendingObjects)
            {
              // No more objects to load.
              NS_LOG_INFO ("Stored video successfully received.");
            }
          else
            {
              // There are objects to load, but we were forced to stop traffic.
              NS_LOG_INFO ("Can't send more requests on force stop mode.");
            }

          // In both cases close the socket, call NotifyStop (), and return.
          socket->ShutdownRecv ();
          socket->Close ();
          m_socket = 0;
          NotifyStop (false);
          return;
        }
    }
}

void
BufferedVideoClient::SendRequest (Ptr<Socket> socket, std::string url)
{
  NS_LOG_FUNCTION (this);

  // Setting request message.
  HttpHeader httpHeaderRequest;
  httpHeaderRequest.SetRequest ();
  httpHeaderRequest.SetVersion ("HTTP/1.1");
  httpHeaderRequest.SetRequestMethod ("GET");
  httpHeaderRequest.SetRequestUrl (url);
  NS_LOG_DEBUG ("Request for URL " << url);

  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (httpHeaderRequest);

  NotifyTx (packet->GetSize ());
  int bytes = socket->Send (packet);
  if (bytes != static_cast<int> (packet->GetSize ()))
    {
      NS_LOG_ERROR ("Not all bytes were copied to the socket buffer.");
    }
}

} // Namespace ns3
