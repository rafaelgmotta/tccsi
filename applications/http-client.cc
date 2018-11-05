/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013 Federal University of Uberlandia
 *               2015 University of Campinas (Unicamp)
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
 * Author: Saulo da Mata <damata.saulo@gmail.com>
 *         Luciano Chaves <luciano@lrc.ic.unicamp.br>
 */

#include "http-client.h"

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT \
  std::clog << "[" << GetAppName ()                       \
            << " client teid " << GetTeidHex () << "] ";

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("HttpClient");
NS_OBJECT_ENSURE_REGISTERED (HttpClient);

TypeId
HttpClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HttpClient")
    .SetParent<SvelteClient> ()
    .AddConstructor<HttpClient> ()
  ;
  return tid;
}

HttpClient::HttpClient ()
  : m_nextRequest (EventId ()),
  m_rxPacket (0),
  m_pagesLoaded (0),
  m_pendingBytes (0),
  m_pendingObjects (0),
  m_stopEvent (EventId ())
{
  NS_LOG_FUNCTION (this);

  // Random variable parameters was taken from paper 'An HTTP Web Traffic Model
  // Based on the Top One Million Visited Web Pages' by Rastin Pries et. al
  // (Table II).
  m_readingTimeStream = CreateObject<LogNormalRandomVariable> ();
  m_readingTimeStream->SetAttribute ("Mu", DoubleValue (-0.495204));
  m_readingTimeStream->SetAttribute ("Sigma", DoubleValue (2.7731));

  // The above model provides a lot of reading times < 1sec, which is not soo
  // good for simulations in LTE EPC + SDN scenarios. So, we are incresing the
  // reading time by a some uniform random value in [0,10] secs.
  m_readingTimeAdjustStream = CreateObject<UniformRandomVariable> ();
  m_readingTimeAdjustStream->SetAttribute ("Min", DoubleValue (0.));
  m_readingTimeAdjustStream->SetAttribute ("Max", DoubleValue (10.));
}

HttpClient::~HttpClient ()
{
  NS_LOG_FUNCTION (this);
}

void
HttpClient::Start ()
{
  NS_LOG_FUNCTION (this);

  // Schedule the ForceStop method to stop traffic based on traffic length.
  Time sTime = GetTrafficLength ();
  m_stopEvent = Simulator::Schedule (sTime, &HttpClient::ForceStop, this);
  NS_LOG_INFO ("Set traffic length to " << sTime.GetSeconds () << "s.");

  // Chain up to reset statistics, notify server, and fire start trace source.
  SvelteClient::Start ();

  NS_LOG_INFO ("Opening the TCP connection.");
  TypeId tcpFactory = TypeId::LookupByName ("ns3::TcpSocketFactory");
  m_socket = Socket::CreateSocket (GetNode (), tcpFactory);
  m_socket->Bind ();
  m_socket->Connect (InetSocketAddress::ConvertFrom (m_serverAddress));
  m_socket->SetConnectCallback (
    MakeCallback (&HttpClient::NotifyConnectionSucceeded, this),
    MakeCallback (&HttpClient::NotifyConnectionFailed, this));
}

void
HttpClient::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_rxPacket = 0;
  m_readingTimeStream = 0;
  m_readingTimeAdjustStream = 0;
  m_nextRequest.Cancel ();
  m_stopEvent.Cancel ();
  SvelteClient::DoDispose ();
}

void
HttpClient::ForceStop ()
{
  NS_LOG_FUNCTION (this);

  // Cancel (possible) pending stop event.
  m_stopEvent.Cancel ();

  // Chain up to set flag and notify server.
  SvelteClient::ForceStop ();

  // If we are on the reading time, cancel any further schedulled requests,
  // close the open socket and call NotifyStop (). Otherwise, the socket will
  // be closed after receiving the current object.
  if (m_nextRequest.IsRunning ())
    {
      m_nextRequest.Cancel ();
      m_socket->ShutdownRecv ();
      m_socket->Close ();
      m_socket = 0;
      NotifyStop (false);
    }
}

void
HttpClient::NotifyConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  NS_LOG_INFO ("Server accepted connection request.");
  socket->SetRecvCallback (MakeCallback (&HttpClient::DataReceived, this));

  // Preparing internal variables for new traffic cycle.
  m_pendingBytes = 0;
  m_pendingObjects = 0;
  m_pagesLoaded = 0;
  m_rxPacket = Create<Packet> (0);

  // Request the first object.
  SendRequest (socket, "main/object");
}

void
HttpClient::NotifyConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  NS_FATAL_ERROR ("Server refused connection request!");
}

void
HttpClient::DataReceived (Ptr<Socket> socket)
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
          if (contentTypeStr == "main/object")
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
          if (contentTypeStr == "inline/object")
            {
              m_pendingObjects--;
            }

          if (m_pendingObjects && !IsForceStop ())
            {
              // Request for the next object and retur.
              NS_LOG_DEBUG ("Request for inline/object " << m_pendingObjects);
              SendRequest (socket, "inline/object");
              continue;
            }

          // Why are we not requesting for more objects?
          if (!m_pendingObjects)
            {
              // No more objects to load. Set the reading time and return.
              NS_LOG_INFO ("HTTP page successfully received.");
              m_pagesLoaded++;
              SetReadingTime (socket);
              return;
            }
          else
            {
              // There are objects to load, but we were forced to stop traffic.
              // In this case close the socket, call NotifyStop (), and return.
              NS_LOG_INFO ("Can't send more requests on force stop mode.");
              socket->ShutdownRecv ();
              socket->Close ();
              m_socket = 0;
              NotifyStop (false);
              return;
            }
        }
    }
}

void
HttpClient::SendRequest (Ptr<Socket> socket, std::string url)
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

void
HttpClient::SetReadingTime (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  double randomSeconds = std::abs (m_readingTimeStream->GetValue ());
  double adjustSeconds = std::abs (m_readingTimeAdjustStream->GetValue ());
  Time readingTime = Seconds (randomSeconds + adjustSeconds);

  NS_LOG_INFO ("Set reading time to " << readingTime.GetSeconds () << "s.");
  m_nextRequest = Simulator::Schedule (readingTime, &HttpClient::SendRequest,
                                       this, socket, "main/object");
}

} // Namespace ns3
