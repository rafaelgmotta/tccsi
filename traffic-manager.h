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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Rafael G. Motta <rafaelgmotta@gmail.com>
 *         Luciano J. Chaves <ljerezchaves@gmail.com> 
 */

#ifndef TRAFFIC_MANAGER_H
#define TRAFFIC_MANAGER_H

#include <ns3/core-module.h>
#include <ns3/lte-module.h>
#include <ns3/network-module.h>
#include <ns3/internet-module.h>

namespace ns3 {

class CustomController;
class SvelteClientApp;

/**
 * \ingroup svelteLogical
 * Traffic manager which handles SVELTE client applications start/stop events.
 * It interacts with the SVELTE architecture to request and release bearers.
 * Each LteUeNetDevice has one TrafficManager object aggregated to it.
 */
class TrafficManager : public Object
{
public:
  TrafficManager ();          //!< Default constructor.
  virtual ~TrafficManager (); //!< Dummy destructor, see DoDispose.

  /**
   * Register this type.
   * \return The object TypeId.
   */
  static TypeId GetTypeId (void);

  /**
   * Add a new application to this manager.
   * \param app The application pointer.
   */
  void AddSvelteClientApp (Ptr<SvelteClientApp> app);

  /**
   * Trace sink notified when new session is created.
   * This will be used to get the teid for each bearer created.
   * \param imsi The IMSI UE identifier.
   * \param bearerList The list of context bearers created.
   */
  //void SessionCreatedCallback (uint64_t imsi, BearerContextList_t bearerList);

  /**
   * Set the IMSI attribute.
   * \param value The ISMI value.
   */
  void SetImsi (uint64_t value);
  void SetController (Ptr<CustomController> ptr);

protected:
  /** Destructor implementation. */
  virtual void DoDispose ();

  // Inherited from ObjectBase
  void NotifyConstructionCompleted (void);

private:
  /**
   * Attempt to (re)start this application. This method will request for bearer
   * resources to the controller before starting the application. If the
   * controller accept the request, the application starts.
   * \internal The teid approach only works because we currently have a single
   * application associated with each bearer/tunnel.
   * \param app The application pointer.
   */
  void AppStartTry (Ptr<SvelteClientApp> app);

  /**
   * Member function called by applications to notify this manager when traffic
   * stops. This method will fire network statistics (EPC) and schedule
   * application restart attempt.
   * \param app The application pointer.
   */
  void NotifyAppStop (Ptr<SvelteClientApp> app);

  /**
   * Set the time for the next attempt to start the application.
   * \internal We will use this interval to limit the current traffic duration
   * to avoid overlapping traffic. This is necessary to respect inter-arrival
   * times for the Poisson process and reuse apps and bearers along the
   * simulation.
   * \param app The application pointer.
   */
  void SetNextAppStartTry (Ptr<SvelteClientApp> app);

  /**
   * Get the absolute time for the next attemp to start the application.
   * \param app The application pointer.
   */
  Time GetNextAppStartTry (Ptr<SvelteClientApp> app) const;

  Ptr<RandomVariableStream> m_poissonRng;       //!< Inter-arrival traffic.
  bool                      m_restartApps;      //!< Continuously restart apps.
  Time                      m_startAppsAfter;   //!< Time before starting apps.
  Time                      m_stopRestartAppsAt; //!< Stop restart apps time.
  Ptr<CustomController>     m_ctrlApp;          //!< OpenFlow slice controller.
  uint64_t                  m_imsi;             //!< UE IMSI identifier.
  uint32_t                  m_defaultTeid;      //!< Default UE tunnel TEID.

  /** Map saving application pointer / next start time. */
  typedef std::map<Ptr<SvelteClientApp>, Time> AppTimeMap_t;
  AppTimeMap_t              m_timeByApp;        //!< Application map.
};

} // namespace ns3
#endif // TRAFFIC_MANAGER_H
