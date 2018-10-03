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

#ifndef VOIP_CLIENT_H
#define VOIP_CLIENT_H

#include "svelte-client-app.h"

namespace ns3 {

/**
 * \ingroup svelteApps
 * This is the client side of a VoIP traffic generator, sending and receiving
 * UDP datagrams following VoIP traffic pattern.
 */
class VoipClient : public SvelteClientApp
{
public:
  /**
   * \brief Register this type.
   * \return the object TypeId.
   */
  static TypeId GetTypeId (void);

  VoipClient ();             //!< Default constructor.
  virtual ~VoipClient ();    //!< Dummy destructor, see DoDispose.

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
  // Inherited from Application.
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  /**
   * \brief Socket receive callback.
   * \param socket Socket with data available to be read.
   */
  void ReadPacket (Ptr<Socket> socket);

  /**
   * \brief Handle a packet transmission.
   */
  void SendPacket ();

  Time                        m_interval;     //!< Interval between packets.
  uint32_t                    m_pktSize;      //!< Packet size.
  EventId                     m_sendEvent;    //!< SendPacket event.
  EventId                     m_stopEvent;    //!< Stop event.
  Ptr<RandomVariableStream>   m_lengthRng;    //!< Random traffic length.
};

} // namespace ns3
#endif /* VOIP_CLIENT_H */
