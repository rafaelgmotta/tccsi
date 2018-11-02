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

#include "http-server.h"

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT                             \
  std::clog << "[" << GetAppName ()                       \
            << " server teid " << GetTeidHex () << "] ";

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("HttpServer");
NS_OBJECT_ENSURE_REGISTERED (HttpServer);

TypeId
HttpServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HttpServer")
    .SetParent<SvelteServer> ()
    .AddConstructor<HttpServer> ()
  ;
  return tid;
}

HttpServer::HttpServer ()
  : m_connected (false),
  m_pendingBytes (0)
{
  NS_LOG_FUNCTION (this);

  // Random variable parameters was taken from paper 'An HTTP Web Traffic Model
  // Based on the Top One Million Visited Web Pages' by Rastin Pries et. al.
  m_mainObjectSizeStream = CreateObject<WeibullRandomVariable> ();
  m_mainObjectSizeStream->SetAttribute ("Scale", DoubleValue (19104.9));
  m_mainObjectSizeStream->SetAttribute ("Shape", DoubleValue (0.771807));

  m_numOfInlineObjStream = CreateObject<ExponentialRandomVariable> ();
  m_numOfInlineObjStream->SetAttribute ("Mean", DoubleValue (31.9291));

  m_inlineObjSizeStream = CreateObject<LogNormalRandomVariable> ();
  m_inlineObjSizeStream->SetAttribute ("Mu", DoubleValue (8.91365));
  m_inlineObjSizeStream->SetAttribute ("Sigma", DoubleValue (1.24816));
}

HttpServer::~HttpServer ()
{
  NS_LOG_FUNCTION (this);
}

void
HttpServer::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_mainObjectSizeStream = 0;
  m_numOfInlineObjStream = 0;
  m_inlineObjSizeStream = 0;
  SvelteServer::DoDispose ();
}

void
HttpServer::StartApplication ()
{
  NS_LOG_FUNCTION (this);

  NS_LOG_INFO ("Creating the listening TCP socket.");
  TypeId tcpFactory = TypeId::LookupByName ("ns3::TcpSocketFactory");
  m_socket = Socket::CreateSocket (GetNode (), tcpFactory);
  m_socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), m_localPort));
  m_socket->Listen ();
  m_socket->SetAcceptCallback (
    MakeCallback (&HttpServer::NotifyConnectionRequest, this),
    MakeCallback (&HttpServer::NotifyNewConnectionCreated, this));
  m_socket->SetCloseCallbacks (
    MakeCallback (&HttpServer::NotifyNormalClose, this),
    MakeCallback (&HttpServer::NotifyErrorClose, this));
}

void
HttpServer::StopApplication ()
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
HttpServer::NotifyConnectionRequest (Ptr<Socket> socket,
                                     const Address& address)
{
  NS_LOG_FUNCTION (this << socket << address);

  Ipv4Address ipAddr = InetSocketAddress::ConvertFrom (address).GetIpv4 ();
  uint16_t port = InetSocketAddress::ConvertFrom (address).GetPort ();
  NS_LOG_INFO ("Connection request received from " << ipAddr << ":" << port);

  return !m_connected;
}

void
HttpServer::NotifyNewConnectionCreated (Ptr<Socket> socket,
                                        const Address& address)
{
  NS_LOG_FUNCTION (this << socket << address);

  Ipv4Address ipAddr = InetSocketAddress::ConvertFrom (address).GetIpv4 ();
  uint16_t port = InetSocketAddress::ConvertFrom (address).GetPort ();
  NS_LOG_INFO ("Connection established with " << ipAddr << ":" << port);
  m_connected = true;
  m_pendingBytes = 0;

  socket->SetSendCallback (MakeCallback (&HttpServer::SendData, this));
  socket->SetRecvCallback (MakeCallback (&HttpServer::DataReceived, this));
}

void
HttpServer::NotifyNormalClose (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  NS_LOG_INFO ("Connection successfully closed.");
  socket->ShutdownSend ();
  socket->ShutdownRecv ();
  m_connected = false;
  m_pendingBytes = 0;
}

void
HttpServer::NotifyErrorClose (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  NS_LOG_WARN ("Connection closed with errors.");
  socket->ShutdownSend ();
  socket->ShutdownRecv ();
  m_connected = false;
  m_pendingBytes = 0;
}

void
HttpServer::DataReceived (Ptr<Socket> socket)
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
HttpServer::SendData (Ptr<Socket> socket, uint32_t available)
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
HttpServer::ProccessHttpRequest (Ptr<Socket> socket, HttpHeader header)
{
  NS_LOG_FUNCTION (this << socket);

  // Check for requested URL.
  std::string url = header.GetRequestUrl ();
  NS_LOG_INFO ("Client requested " << url);
  if (url == "main/object")
    {
      // Set parameter values.
      m_pendingBytes = m_mainObjectSizeStream->GetInteger ();
      uint32_t numOfInlineObj = m_numOfInlineObjStream->GetInteger ();
      NS_LOG_INFO ("HTTP main object size is " << m_pendingBytes <<
                   " bytes with " << numOfInlineObj << " inline objects.");

      // Set the response message.
      HttpHeader httpHeaderOut;
      httpHeaderOut.SetResponse ();
      httpHeaderOut.SetVersion ("HTTP/1.1");
      httpHeaderOut.SetResponseStatusCode ("200");
      httpHeaderOut.SetResponsePhrase ("OK");
      httpHeaderOut.SetHeaderField ("ContentLength", m_pendingBytes);
      httpHeaderOut.SetHeaderField ("ContentType", "main/object");
      httpHeaderOut.SetHeaderField ("InlineObjects", numOfInlineObj);

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
  else if (url == "inline/object")
    {
      m_pendingBytes = m_inlineObjSizeStream->GetInteger ();
      NS_LOG_DEBUG ("HTTP inline object size (bytes): " << m_pendingBytes);

      // Set the response message.
      HttpHeader httpHeaderOut;
      httpHeaderOut.SetResponse ();
      httpHeaderOut.SetVersion ("HTTP/1.1");
      httpHeaderOut.SetResponseStatusCode ("200");
      httpHeaderOut.SetResponsePhrase ("OK");
      httpHeaderOut.SetHeaderField ("ContentLength", m_pendingBytes);
      httpHeaderOut.SetHeaderField ("ContentType", "inline/object");
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

} // Namespace ns3
