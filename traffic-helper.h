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

#ifndef TRAFFIC_HELPER_H
#define TRAFFIC_HELPER_H

#include <ns3/core-module.h>
#include <ns3/lte-module.h>
#include <ns3/network-module.h>
#include <ns3/internet-module.h>
#include "application-helper.h"

namespace ns3 {

class TrafficManager;
class CustomController;

/**
 * \ingroup svelte
 * Traffic helper which installs client and server applications for all
 * applications into UEs and WebServer. This helper creates and aggregates a
 * traffic manager for each UE.
 */
class TrafficHelper : public Object
{
public:
  /**
   * Complete constructor.
   * \param controller The OpenFlow controller.
   * \param webNode The server node.
   * \param ueNodes The client nodes.
   */
  TrafficHelper (Ptr<CustomController> controller,
                 Ptr<Node> webNode, NodeContainer ueNodes);
  virtual ~TrafficHelper ();  //!< Dummy destructor, see DoDispose.

  /**
   * Register this type.
   * \return The object TypeId.
   */
  static TypeId GetTypeId (void);

protected:
  /** Destructor implementation. */
  virtual void DoDispose ();

  // Inherited from ObjectBase
  void NotifyConstructionCompleted (void);

private:
  /**
   * Install a traffic manager into each UE and configure the EPS bearers and
   * TFT packet filters for enable applications
   * \attention The QCIs used here for each application are strongly related to
   *     the DSCP mapping, which will reflect on the priority queues used by
   *     both OpenFlow switches and traffic control module. Be careful if you
   *     intend to change it.
   * \internal Some notes about internal GbrQosInformation usage:
   * \li The Maximum Bit Rate field is used by controller to install meter
   *     rules for this traffic. When this value is left to 0, no meter rules
   *     will be installed.
   * \li The Guaranteed Bit Rate field is used by the controller to reserve the
   *     requested bandwidth in OpenFlow EPC network (only for GBR beares).
   */
  void ConfigureApplications ();

  /**
   * Configure application helpers for different traffic patterns.
   */
  void ConfigureHelpers ();

  /**
   * Get the next port number available for use.
   * \return The port number to use.
   */
  static uint16_t GetNextPortNo ();

  /**
   * Get complete filename for video trace files.
   * \param idx The video index (see movies folder).
   * \return The complete file path.
   */
  static const std::string GetVideoFilename (uint8_t idx);

  /**
   * Create the pair of client/server applications and install them into UE,
   * using the default EPS bearer for this traffic.
   * \param helper The reference to the application helper.
   * \param teid The TEID for this application.
   */
  void InstallAppDefault (ApplicationHelper& helper, uint32_t teid);

  // Traffic helper.
  static uint16_t             m_port;             //!< Port numbers for apps.
  Ptr<CustomController>       m_controller;       //!< OpenFlow controller.

  // Traffic manager.
  ObjectFactory               m_managerFac;       //!< Traffic manager factory.
  Ptr<RandomVariableStream>   m_poissonRng;       //!< Inter-arrival traffic.
  bool                        m_restartApps;      //!< Continuous restart apps.
  Time                        m_startAppsAfter;   //!< Time before start apps.
  Time                        m_stopRestartAppsAt; //!< Stop restart apps time.

  // Enabled applications.
  bool                        m_dftHttpPage;      //!< Non-GBR default HTTP.
  bool                        m_gbrAutPilot;      //!< GBR auto-pilot.
  bool                        m_gbrGameOpen;      //!< GBR game open.
  bool                        m_gbrGameTeam;      //!< GBR game team.
  bool                        m_gbrLivVideo;      //!< GBR live video.
  bool                        m_gbrVoipCall;      //!< GBR VoIP call.
  bool                        m_nonAutPilot;      //!< Non-GBR auto-pilot.
  bool                        m_nonBikeRace;      //!< Non-GBR bicycle race.
  bool                        m_nonBufVideo;      //!< Non-GBR buffered video.
  bool                        m_nonGpsTrack;      //!< Non-GBR GPS team track.
  bool                        m_nonHttpPage;      //!< Non-GBR HTTP.
  bool                        m_nonLivVideo;      //!< Non-GBR live video.

  // Application helpers.
  ApplicationHelper           m_autPilotHelper;   //!< Auto-pilot helper.
  ApplicationHelper           m_bikeRaceHelper;   //!< Bicycle race helper.
  ApplicationHelper           m_bufVideoHelper;   //!< Buffered video helper.
  ApplicationHelper           m_gameOpenHelper;   //!< Open Arena helper.
  ApplicationHelper           m_gameTeamHelper;   //!< Team Fortress helper.
  ApplicationHelper           m_gpsTrackHelper;   //!< GPS tracking helper.
  ApplicationHelper           m_httpPageHelper;   //!< HTTP page helper.
  ApplicationHelper           m_livVideoHelper;   //!< Live video helper.
  ApplicationHelper           m_voipCallHelper;   //!< VoIP call helper.

  // Temporary variables used only when installing applications.
  Ptr<TrafficManager>         m_ueManager;        //!< Traffic manager obj.
  Ptr<Node>                   m_webNode;          //!< Server node.
  Ipv4Address                 m_webAddr;          //!< Server address.
  Ptr<Node>                   m_ueNode;           //!< Client node.
  Ipv4Address                 m_ueAddr;           //!< Client address.
  NodeContainer               m_ueContainer;      //!< Client nodes.

  // Video traces.
  Ptr<UniformRandomVariable>  m_gbrVidRng;        //!< GBR random live video.
  Ptr<UniformRandomVariable>  m_nonVidRng;        //!< Non-GBR random video.
  static const std::string    m_videoDir;         //!< Video trace directory.
  static const std::string    m_videoTrace [];    //!< Video trace filenames.
};

} // namespace ns3
#endif // TRAFFIC_HELPER_H
