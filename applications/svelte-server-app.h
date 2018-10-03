/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 University of Campinas (Unicamp)
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

#ifndef SVELTE_SERVER_APP_H
#define SVELTE_SERVER_APP_H

#include <ns3/core-module.h>
#include <ns3/lte-module.h>
#include <ns3/network-module.h>
#include <ns3/internet-module.h>
#include <ns3/lte-module.h>
#include "app-stats-calculator.h"

namespace ns3 {

class SvelteClientApp;

/**
 * \ingroup svelteApps
 * This class extends the Application class to proper work with SVELTE
 * architecture. Only server applications (those which will be installed into
 * web server node) should extend this class.
 */
class SvelteServerApp : public Application
{
  friend class SvelteClientApp;

public:
  SvelteServerApp ();            //!< Default constructor.
  virtual ~SvelteServerApp ();   //!< Dummy destructor, see DoDispose.

  /**
   * Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /** \name Private member accessors. */
  //\{
  std::string GetAppName (void) const;
  bool IsActive (void) const;
  bool IsForceStop (void) const;
  std::string GetTeidHex (void) const;
  Ptr<SvelteClientApp> GetClientApp (void) const;
  Ptr<const AppStatsCalculator> GetAppStats (void) const;
  //\}

  /**
   * \brief Set the client application.
   * \param clientApp The pointer to client application.
   * \param clientAddress The Inet socket address of the client.
   */
  void SetClient (Ptr<SvelteClientApp> clientApp, Address clientAddress);

protected:
  /** Destructor implementation. */
  virtual void DoDispose (void);

  /**
   * Notify this server of a start event on the client application. The server
   * must reset internal counters and start traffic when applicable.
   */
  virtual void NotifyStart ();

  /**
   * Notify this server of a force stop event on the client application. When
   * applicable, the server must stop generating traffic.
   */
  virtual void NotifyForceStop ();

  /**
   * Update TX counter for a new transmitted packet on client stats calculator.
   * \param txBytes The total number of bytes in this packet.
   * \return The next TX sequence number to use.
   */
  uint32_t NotifyTx (uint32_t txBytes);

  /**
   * Update RX counter for a new received packet on server stats calculator.
   * \param rxBytes The total number of bytes in this packet.
   * \param timestamp The timestamp when this packet was sent.
   */
  void NotifyRx (uint32_t rxBytes, Time timestamp = Simulator::Now ());

  /**
   * Reset the QoS statistics.
   */
  void ResetAppStats ();

  Ptr<AppStatsCalculator> m_appStats;         //!< QoS statistics.
  Ptr<Socket>             m_socket;           //!< Local socket.
  uint16_t                m_localPort;        //!< Local port.
  Address                 m_clientAddress;    //!< Client address.
  Ptr<SvelteClientApp>    m_clientApp;        //!< Client application.
};

} // namespace ns3
#endif /* SVELTE_SERVER_APP_H */
