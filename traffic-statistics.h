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

#ifndef TRAFFIC_STATS_CALCULATOR_H
#define TRAFFIC_STATS_CALCULATOR_H

#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include "applications/app-stats-calculator.h"

namespace ns3 {

class SvelteClient;


/**
 * \ingroup svelteStats
 * This class monitors the traffic QoS statistics at application L7 level for
 * end-to-end traffic, and also at L2 OpenFlow link level for traffic within
 * the LTE EPC.
 */
class TrafficStatistics : public Object
{
public:
  /** Traffic direction. */
  enum Direction
  {
    DLINK = 0,  //!< Downlink traffic.
    ULINK = 1   //!< Uplink traffic.
  };

  TrafficStatistics ();          //!< Default constructor.
  virtual ~TrafficStatistics (); //!< Dummy destructor, see DoDispose.

  /**
   * Register this type.
   * \return The object TypeId.
   */
  static TypeId GetTypeId (void);

  /**
   * Get the string representing the given direction.
   * \param dir The link direction.
   * \return The link direction string.
   */
  static std::string DirectionStr (Direction dir);


protected:
  /** Destructor implementation. */
  virtual void DoDispose ();

  // Inherited from ObjectBase.
  virtual void NotifyConstructionCompleted (void);

private:
  /** Metadata associated to admission. */
  struct AdmStats
  {
    uint64_t tempReleases;      //!< Temp number of releases.
    uint64_t tempRequests;      //!< Temp number of requests.
    uint64_t tempAccepted;      //!< Temp number of requests accepted.
    uint64_t tempBlocked;       //!< Temp number of requests blocked.
    uint64_t activeBearers;     //!< Number of active bearers.
    uint64_t totalReleases;     //!< Total number of releases.
    uint64_t totalRequests;     //!< Total number of requests.
    uint64_t totalAccepted;     //!< Total number of requests accepted.
    uint64_t totalBlocked;      //!< Total number of requests blocked.
  };

  /** Metadata associated to packet drops. */
  struct DropStats
  {
    uint64_t tempLoad;        //!< Temp number of overload drops.
    uint64_t tempMeter;       //!< Temp number of meter drops.
    uint64_t tempQueue;       //!< Temp number of queue drops.
    uint64_t totalLoad;       //!< Total number of overload drops.
    uint64_t totalMeter;      //!< Total number of meter drops.
    uint64_t totalQueue;      //!< Total number of queue drops.
  };

  /**
   * Dump admission statistics into file.
   */
  void DumpAdmission ();

  /**
   * Dump admission statistics into file.
   */
  void DumpDrop ();

  /**
   * Dump traffic statistics into file.
   * Trace sink fired when application traffic stops.
   * \param context Context information.
   * \param app The client application.
   */
  void DumpTraffic (std::string context, Ptr<SvelteClient> app);

  /**
   * Notify a new traffic request.
   * \param context Context information.
   * \param teid The traffic TEID.
   * \param accepted The request status.
   */
  void NotifyRequest (std::string context, uint32_t teid, bool accepted);

  /**
   * Notify a traffic release.
   * \param context Context information.
   * \param teid The traffic TEID.
   */
  void NotifyRelease (std::string context, uint32_t teid);

  /**
   * Trace sink fired when a packet is dropped while exceeding pipeline load
   * capacity.
   * \param context Context information.
   * \param packet The dropped packet.
   */
  void OverloadDropPacket (std::string context, Ptr<const Packet> packet);

  /**
   * Trace sink fired when a packets is dropped by meter band.
   * \param context Context information.
   * \param packet The dropped packet.
   * \param meterId The meter ID that dropped the packet.
   */
  void MeterDropPacket (std::string context, Ptr<const Packet> packet,
                        uint32_t meterId);

  /**
   * Trace sink fired when a packet is dropped by OpenFlow port queues.
   * \param context Context information.
   * \param packet The dropped packet.
   */
  void QueueDropPacket (std::string context, Ptr<const Packet> packet);

  AdmStats                  m_admStats;     //!< Admission stats.
  DropStats                 m_drpStats;

  std::string               m_admFilename;  //!< AdmStats filename.
  Ptr<OutputStreamWrapper>  m_admWrapper;   //!< AdmStats file wrapper.
  std::string               m_appFilename;  //!< AppStats filename.
  Ptr<OutputStreamWrapper>  m_appWrapper;   //!< AppStats file wrapper.
  std::string               m_drpFilename;  //!< DrpStats filename.
  Ptr<OutputStreamWrapper>  m_drpWrapper;   //!< DrpStats file wrapper.
};

} // namespace ns3
#endif /* TRAFFIC_STATS_CALCULATOR_H */
