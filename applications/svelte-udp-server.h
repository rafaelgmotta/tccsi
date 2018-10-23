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

#ifndef SVELTE_UDP_SERVER_H
#define SVELTE_UDP_SERVER_H

#include "svelte-server.h"

namespace ns3 {

/**
 * \ingroup svelteApps
 * This is the server side of a generic UDP traffic generator, sending and
 * receiving UDP datagrams following the configure traffic pattern.
 */
class SvelteUdpServer : public SvelteServer
{
public:
  /**
   * \brief Register this type.
   * \return the object TypeId.
   */
  static TypeId GetTypeId (void);

  SvelteUdpServer ();             //!< Default constructor.
  virtual ~SvelteUdpServer ();    //!< Dummy destructor, see DoDispose.

protected:
  // Inherited from Object.
  virtual void DoDispose (void);

private:
  // Inherited from Application.
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  // Inherited from SvelteServer.
  void NotifyStart ();
  void NotifyForceStop ();

  /**
   * \brief Socket receive callback.
   * \param socket Socket with data available to be read.
   */
  void ReadPacket (Ptr<Socket> socket);

  /**
   * \brief Handle a packet transmission.
   */
  void SendPacket ();

  Ptr<RandomVariableStream>   m_pktInterRng;  //!< Pkt inter-arrival time.
  Ptr<RandomVariableStream>   m_pktSizeRng;   //!< Pkt size.
  EventId                     m_sendEvent;    //!< SendPacket event.
};

} // namespace ns3
#endif /* SVELTE_UDP_SERVER_H */
