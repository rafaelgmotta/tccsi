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

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "svelte-client-app.h"

namespace ns3 {

/**
 * \ingroup svelteApps
 * This is the client side of a HTTP Traffic Generator. The client establishes
 * a TCP connection with the server and sends a request for the main object of
 * a given web page. When client gets the main object, it process the message
 * and start to request the inline objects of the given web page. After
 * receiving all inline objects, the client waits an interval (reading time)
 * before it requests a new main object of a new web page. The implementation
 * of this application is simplistic and it does not support pipelining in this
 * current version. The model used is based on the distributions indicated in
 * the paper "An HTTP Web Traffic Model Based on the Top One Million Visited
 * Web Pages" by Rastin Pries et.  al. This simplistic approach was taken since
 * this traffic generator was developed primarily to help users evaluate their
 * proposed algorithm in other modules of ns-3. To allow deeper studies about
 * the HTTP Protocol it needs some improvements.
 */
class HttpClient : public SvelteClientApp
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId.
   */
  static TypeId GetTypeId (void);

  HttpClient ();          //!< Default constructor.
  virtual ~HttpClient (); //!< Dummy destructor, see DoDispose.

  // Inherited from SvelteClientApp.
  void Start ();

protected:
  // Inherited from Object.
  virtual void DoDispose (void);

  // Inherited from ObjectBase.
  virtual void NotifyConstructionCompleted (void);

  // Inherited from SvelteClientApp.
  void ForceStop ();

private:
  /**
   * Callback for a connection successfully established.
   * \param socket The connected socket.
   */
  void NotifyConnectionSucceeded (Ptr<Socket> socket);

  /**
   * Callback for a connection failed.
   * \param socket The connected socket.
   */
  void NotifyConnectionFailed (Ptr<Socket> socket);

  /**
   * Callback for in-order bytes available in receive buffer.
   * \param socket The connected socket.
   */
  void DataReceived (Ptr<Socket> socket);

  /**
   * \brief Send the request to server.
   * \param socket The connected socket.
   * \param url The URL of the object requested.
   */
  void SendRequest (Ptr<Socket> socket, std::string url);

  /**
   * \brief Set a reading time before requesting a new main object.
   * \param socket socket that sends requests.
   */
  void SetReadingTime (Ptr<Socket> socket);

  uint16_t                     m_maxPages;                //!< Pages thres
  Time                         m_maxReadingTime;          //!< Reading thres
  EventId                      m_nextRequest;             //!< Next request
  Ptr<Packet>                  m_rxPacket;                //!< RX packet
  uint16_t                     m_pagesLoaded;             //!< Pages loaded
  uint32_t                     m_pendingBytes;            //!< Pending bytes
  uint32_t                     m_pendingObjects;          //!< Pending objects
  Ptr<LogNormalRandomVariable> m_readingTimeStream;       //!< Reading time
  Ptr<UniformRandomVariable>   m_readingTimeAdjustStream; //!< Time adjust
};

} // Namespace ns3
#endif /* HTTP_CLIENT_H */
