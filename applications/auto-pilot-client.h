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

#ifndef AUTO_PILOT_CLIENT_H
#define AUTO_PILOT_CLIENT_H

#include "svelte-client-app.h"

namespace ns3 {

/**
 * \ingroup svelteApps
 * This is the client side of a auto pilot MTC traffic generator, sending and
 * receiving UDP datagrams following auto pilot traffic pattern.
 */
class AutoPilotClient : public SvelteClientApp
{
public:
  /**
   * \brief Register this type.
   * \return the object TypeId.
   */
  static TypeId GetTypeId (void);

  AutoPilotClient ();             //!< Default constructor.
  virtual ~AutoPilotClient ();    //!< Dummy destructor, see DoDispose.

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

  Ptr<RandomVariableStream>   m_intervalRng;  //!< Rand interval between pkts.
  uint32_t                    m_pktSize;      //!< Packet size.
  EventId                     m_sendEvent;    //!< SendPacket event.
  EventId                     m_stopEvent;    //!< Stop event.
  Ptr<RandomVariableStream>   m_lengthRng;    //!< Random traffic length.
};

} // namespace ns3
#endif /* AUTO_PILOT_CLIENT_H */
