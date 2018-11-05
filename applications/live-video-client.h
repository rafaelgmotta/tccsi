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

#ifndef LIVE_VIDEO_CLIENT_H
#define LIVE_VIDEO_CLIENT_H

#include "svelte-client.h"

namespace ns3 {

/**
 * \ingroup svelteApps
 * This is the client side of a live video traffic generator, receiving
 * UDP datagrams following a MPEG video pattern with random video length.
 */
class LiveVideoClient : public SvelteClient
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  LiveVideoClient ();   //!< Default constructor.
  virtual ~LiveVideoClient ();  //!< Dummy destructor, see DosDipose.

  // Inherited from SvelteClient.
  void Start ();

protected:
  // Inherited from Object.
  virtual void DoDispose (void);

  // Inherited from SvelteClient.
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

  EventId                     m_stopEvent;    //!< Stop event.
};

} // Namespace ns3
#endif /* LIVE_VIDEO_CLIENT_H */
