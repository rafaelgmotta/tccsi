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

#include "traffic-helper.h"
#include "applications/buffered-video-client.h"
#include "applications/buffered-video-server.h"
#include "applications/http-client.h"
#include "applications/http-server.h"
#include "applications/live-video-client.h"
#include "applications/live-video-server.h"
#include "applications/svelte-udp-client.h"
#include "applications/svelte-udp-server.h"
#include "applications/app-stats-calculator.h"
#include "traffic-manager.h"
#include "custom-controller.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TrafficHelper");
NS_OBJECT_ENSURE_REGISTERED (TrafficHelper);

// Initial port number
uint16_t TrafficHelper::m_port = 10000;

// Trace files directory
const std::string TrafficHelper::m_videoDir = "./scratch/tccsi/movies/";

// Trace files are sorted in increasing gbr bit rate
const std::string TrafficHelper::m_videoTrace [] = {
  "office-cam-low.txt", "office-cam-medium.txt", "office-cam-high.txt",
  "first-contact.txt", "star-wars-iv.txt", "ard-talk.txt", "mr-bean.txt",
  "n3-talk.txt", "the-firm.txt", "ard-news.txt", "jurassic-park.txt",
  "from-dusk-till-dawn.txt", "formula1.txt", "soccer.txt",
  "silence-of-the-lambs.txt"
};

// ------------------------------------------------------------------------ //
TrafficHelper::TrafficHelper (Ptr<CustomController> controller,
                              Ptr<Node> webNode, NodeContainer ueNodes)
{
  NS_LOG_FUNCTION (this);

  m_controller = controller;
  m_webNode = webNode;
  m_ueContainer = ueNodes;
}

TrafficHelper::~TrafficHelper ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
TrafficHelper::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TrafficHelper")
    .SetParent<Object> ()

    // Traffic manager attributes.
    .AddAttribute ("PoissonInterArrival",
                   "An exponential random variable used to get "
                   "application inter-arrival start times.",
                   TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                   StringValue ("ns3::ExponentialRandomVariable[Mean=180.0]"),
                   MakePointerAccessor (&TrafficHelper::m_poissonRng),
                   MakePointerChecker <RandomVariableStream> ())
    .AddAttribute ("RestartApps",
                   "Continuously restart applications after stop events.",
                   TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                   BooleanValue (true),
                   MakeBooleanAccessor (&TrafficHelper::m_restartApps),
                   MakeBooleanChecker ())
    .AddAttribute ("StartAppsAfter",
                   "The time before starting the applications.",
                   TimeValue (Seconds (1)),
                   MakeTimeAccessor (&TrafficHelper::m_startAppsAfter),
                   MakeTimeChecker ())
    .AddAttribute ("StopRestartAppsAt",
                   "The time to disable the RestartApps attribute.",
                   TimeValue (Seconds (0)),
                   MakeTimeAccessor (&TrafficHelper::m_stopRestartAppsAt),
                   MakeTimeChecker ())

    // Applications to be installed.
    .AddAttribute ("EnableDftHttpPage",
                   "Enable Non-GBR HTTP webpage traffic over default bearer.",
                   TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                   BooleanValue (false),
                   MakeBooleanAccessor (&TrafficHelper::m_dftHttpPage),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableGbrAutPilot",
                   "Enable GBR auto-pilot traffic.",
                   TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                   BooleanValue (false),
                   MakeBooleanAccessor (&TrafficHelper::m_gbrAutPilot),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableGbrGameOpen",
                   "Enable GBR game Open Arena traffic.",
                   TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                   BooleanValue (false),
                   MakeBooleanAccessor (&TrafficHelper::m_gbrGameOpen),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableGbrGameTeam",
                   "Enable GBR game Team Fortress traffic.",
                   TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                   BooleanValue (false),
                   MakeBooleanAccessor (&TrafficHelper::m_gbrGameTeam),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableGbrLivVideo",
                   "Enable GBR live video streaming traffic.",
                   TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                   BooleanValue (false),
                   MakeBooleanAccessor (&TrafficHelper::m_gbrLivVideo),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableGbrVoipCall",
                   "Enable GBR VoIP call traffic.",
                   TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                   BooleanValue (false),
                   MakeBooleanAccessor (&TrafficHelper::m_gbrVoipCall),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableNonAutPilot",
                   "Enable Non-GBR auto-pilot traffic.",
                   TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                   BooleanValue (false),
                   MakeBooleanAccessor (&TrafficHelper::m_nonAutPilot),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableNonBikeRace",
                   "Enable Non-GBR bicycle race traffic.",
                   TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                   BooleanValue (false),
                   MakeBooleanAccessor (&TrafficHelper::m_nonBikeRace),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableNonBufVideo",
                   "Enable Non-GBR buffered video traffic.",
                   TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                   BooleanValue (false),
                   MakeBooleanAccessor (&TrafficHelper::m_nonBufVideo),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableNonGpsTrack",
                   "Enable Non-GBR GPS team tracking traffic.",
                   TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                   BooleanValue (false),
                   MakeBooleanAccessor (&TrafficHelper::m_nonGpsTrack),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableNonHttpPage",
                   "Enable Non-GBR HTTP webpage traffic.",
                   TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                   BooleanValue (false),
                   MakeBooleanAccessor (&TrafficHelper::m_nonHttpPage),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableNonLivVideo",
                   "Enable Non-GBR live video streaming traffic.",
                   TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                   BooleanValue (false),
                   MakeBooleanAccessor (&TrafficHelper::m_nonLivVideo),
                   MakeBooleanChecker ())
  ;
  return tid;
}

void
TrafficHelper::DoDispose ()
{
  NS_LOG_FUNCTION (this);

  m_poissonRng = 0;
  m_webNode = 0;
  m_ueManager = 0;
  m_ueNode = 0;
  m_gbrVidRng = 0;
  m_nonVidRng = 0;
  Object::DoDispose ();
}

void
TrafficHelper::NotifyConstructionCompleted ()
{
  NS_LOG_FUNCTION (this);

  // Saving server metadata.
  NS_ASSERT_MSG (m_webNode->GetNDevices () == 2, "Single device expected.");
  Ptr<NetDevice> webDev = m_webNode->GetDevice (1);
  m_webAddr = Ipv4AddressHelper::GetAddress (webDev);

  // Configure the traffic manager object factory.
  m_managerFac.SetTypeId (TrafficManager::GetTypeId ());
  m_managerFac.Set ("PoissonInterArrival", PointerValue (m_poissonRng));
  m_managerFac.Set ("RestartApps", BooleanValue (m_restartApps));
  m_managerFac.Set ("StartAppsAfter", TimeValue (m_startAppsAfter));
  m_managerFac.Set ("StopRestartAppsAt", TimeValue (m_stopRestartAppsAt));

  // Configure random video selections.
  m_gbrVidRng = CreateObject<UniformRandomVariable> ();
  m_gbrVidRng->SetAttribute ("Min", DoubleValue (0));
  m_gbrVidRng->SetAttribute ("Max", DoubleValue (2));

  m_nonVidRng = CreateObject<UniformRandomVariable> ();
  m_nonVidRng->SetAttribute ("Min", DoubleValue (3));
  m_nonVidRng->SetAttribute ("Max", DoubleValue (14));

  // Configure the helpers and install the applications.
  ConfigureHelpers ();
  ConfigureApplications ();

  Object::NotifyConstructionCompleted ();
}

void
TrafficHelper::ConfigureHelpers ()
{
  NS_LOG_FUNCTION (this);

  // -------------------------------------------------------------------------
  // Configuring HTC application helpers.
  //

  // BufferedVideo and LiveVideo applications are based on MPEG-4 video traces
  // from http://www-tkn.ee.tu-berlin.de/publications/papers/TKN0006.pdf.
  // We just instantiate their helpers here.
  m_bufVideoHelper = ApplicationHelper (
      BufferedVideoClient::GetTypeId (),
      BufferedVideoServer::GetTypeId ());
  m_livVideoHelper = ApplicationHelper (
      LiveVideoClient::GetTypeId (),
      LiveVideoServer::GetTypeId ());


  // The HTTP model is based on the distributions indicated in the paper 'An
  // HTTP Web Traffic Model Based on the Top One Million Visited Web Pages' by
  // Rastin Pries et. al. Each client will send a get request to the server and
  // will get the page content back including inline content. These requests
  // repeats after a reading time period, until MaxPages are loaded or
  // MaxReadingTime is reached. We just instantiate its helper here.
  m_httpPageHelper = ApplicationHelper (
      HttpClient::GetTypeId (),
      HttpServer::GetTypeId ());


  // The VoIP application simulating the G.729 codec (~8.0 kbps for payload).
  // Check http://goo.gl/iChPGQ for bandwidth calculation and discussion.
  m_voipCallHelper = ApplicationHelper (
      SvelteUdpClient::GetTypeId (),
      SvelteUdpServer::GetTypeId ());
  m_voipCallHelper.SetClientAttribute ("AppName", StringValue ("VoipCall"));

  // For traffic length, we are considering an estimative from Vodafone that
  // the average call length is 1 min and 40 sec. We are including a normal
  // standard deviation of 10 sec. See http://tinyurl.com/pzmyys2 and
  // http://www.theregister.co.uk/2013/01/30/mobile_phone_calls_shorter for
  // more information on this topic.
  m_voipCallHelper.SetClientAttribute (
    "TrafficLength",
    StringValue ("ns3::NormalRandomVariable[Mean=100.0|Variance=100.0]"));

  // Model chosen: 20B packets sent in both directions every 0.02 seconds.
  m_voipCallHelper.SetClientAttribute (
    "PktSize",
    StringValue ("ns3::ConstantRandomVariable[Constant=20]"));
  m_voipCallHelper.SetClientAttribute (
    "PktInterval",
    StringValue ("ns3::ConstantRandomVariable[Constant=0.02]"));
  m_voipCallHelper.SetServerAttribute (
    "PktSize",
    StringValue ("ns3::ConstantRandomVariable[Constant=20]"));
  m_voipCallHelper.SetServerAttribute (
    "PktInterval",
    StringValue ("ns3::ConstantRandomVariable[Constant=0.02]"));


  // The online game Open Arena.
  m_gameOpenHelper = ApplicationHelper (
      SvelteUdpClient::GetTypeId (),
      SvelteUdpServer::GetTypeId ());
  m_gameOpenHelper.SetClientAttribute ("AppName", StringValue ("GameOpen"));

  // For traffic length, we are using a synthetic average length of 90 seconds
  // with 10 sec stdev. This will force the application to periodically stop
  // and report statistics.
  m_gameOpenHelper.SetClientAttribute (
    "TrafficLength",
    StringValue ("ns3::NormalRandomVariable[Mean=90.0|Variance=100.0]"));

  // Traffic model.
  m_gameOpenHelper.SetClientAttribute (
    "PktSize",
    StringValue ("ns3::NormalRandomVariable[Mean=42.199|Variance=4.604]"));
  m_gameOpenHelper.SetClientAttribute (
    "PktInterval",
    StringValue ("ns3::UniformRandomVariable[Min=0.069|Max=0.103]"));
  m_gameOpenHelper.SetServerAttribute (
    "PktSize",
    StringValue ("ns3::NormalRandomVariable[Mean=172.400|Variance=85.821]"));
  m_gameOpenHelper.SetServerAttribute (
    "PktInterval",
    StringValue ("ns3::UniformRandomVariable[Min=0.041|Max=0.047]"));


  // The online game Team Fortress.
  m_gameTeamHelper = ApplicationHelper (
      SvelteUdpClient::GetTypeId (),
      SvelteUdpServer::GetTypeId ());
  m_gameTeamHelper.SetClientAttribute ("AppName", StringValue ("GameTeam"));

  // For traffic length, we are using a synthetic average length of 90 seconds
  // with 10 sec stdev. This will force the application to periodically stop
  // and report statistics.
  m_gameTeamHelper.SetClientAttribute (
    "TrafficLength",
    StringValue ("ns3::NormalRandomVariable[Mean=90.0|Variance=100.0]"));

  // Traffic model.
  m_gameTeamHelper.SetClientAttribute (
    "PktSize",
    StringValue ("ns3::NormalRandomVariable[Mean=76.523|Variance=13.399]"));
  m_gameTeamHelper.SetClientAttribute (
    "PktInterval",
    StringValue ("ns3::UniformRandomVariable[Min=0.031|Max=0.042]"));
  m_gameTeamHelper.SetServerAttribute (
    "PktSize",
    StringValue ("ns3::NormalRandomVariable[Mean=240.752|Variance=79.339]"));
  m_gameTeamHelper.SetServerAttribute (
    "PktInterval",
    StringValue ("ns3::UniformRandomVariable[Min=0.039|Max=0.046]"));


  // -------------------------------------------------------------------------
  // Configuring MTC application helpers.
  //
  // The following applications were adapted from the MTC models presented on
  // the "Machine-to-Machine Communications: Architectures, Technology,
  // Standards, and Applications" book, chapter 3: "M2M traffic and models".

  // The auto-pilot includes both vehicle collision detection and avoidance on
  // highways. Clients sending data on position, in time intervals depending on
  // vehicle speed, while server performs calculations, collision detection
  // etc., and sends back control information.
  m_autPilotHelper = ApplicationHelper (
      SvelteUdpClient::GetTypeId (),
      SvelteUdpServer::GetTypeId ());
  m_autPilotHelper.SetClientAttribute ("AppName", StringValue ("AutPilot"));

  // For traffic length, we are using a synthetic average length of 90 seconds
  // with 10 sec stdev. This will force the application to periodically stop
  // and report statistics.
  m_autPilotHelper.SetClientAttribute (
    "TrafficLength",
    StringValue ("ns3::NormalRandomVariable[Mean=90.0|Variance=100.0]"));

  // Model chosen: 1kB packets sent towards the server with uniformly
  // distributed inter-arrival time ranging from 0.025 to 0.1s, server responds
  // every second with 1kB message.
  m_autPilotHelper.SetClientAttribute (
    "PktSize",
    StringValue ("ns3::ConstantRandomVariable[Constant=1024]"));
  m_autPilotHelper.SetClientAttribute (
    "PktInterval",
    StringValue ("ns3::UniformRandomVariable[Min=0.025|Max=0.1]"));
  m_autPilotHelper.SetServerAttribute (
    "PktSize",
    StringValue ("ns3::ConstantRandomVariable[Constant=1024]"));
  m_autPilotHelper.SetServerAttribute (
    "PktInterval",
    StringValue ("ns3::UniformRandomVariable[Min=0.999|Max=1.001]"));


  // The bicycle race is a virtual game where two or more players exchange real
  // data on bicycle position, speed etc. They are used by the application to
  // calculate the equivalent positions of the participants and to show them
  // the corresponding state of the race.
  m_bikeRaceHelper = ApplicationHelper (
      SvelteUdpClient::GetTypeId (),
      SvelteUdpServer::GetTypeId ());
  m_bikeRaceHelper.SetClientAttribute ("AppName", StringValue ("BikeRace"));

  // For traffic length, we are using a synthetic average length of 90 seconds
  // with 10 sec stdev. This will force the application to periodically stop
  // and report statistics.
  m_bikeRaceHelper.SetClientAttribute (
    "TrafficLength",
    StringValue ("ns3::NormalRandomVariable[Mean=90.0|Variance=100.0]"));

  // Model chosen: 1kB packets exchanged with uniformly distributed inter-
  // arrival time ranging from 0.1 to 0.5s.
  m_bikeRaceHelper.SetClientAttribute (
    "PktSize",
    StringValue ("ns3::ConstantRandomVariable[Constant=1024]"));
  m_bikeRaceHelper.SetClientAttribute (
    "PktInterval",
    StringValue ("ns3::UniformRandomVariable[Min=0.1|Max=0.5]"));
  m_bikeRaceHelper.SetServerAttribute (
    "PktSize",
    StringValue ("ns3::ConstantRandomVariable[Constant=1024]"));
  m_bikeRaceHelper.SetServerAttribute (
    "PktInterval",
    StringValue ("ns3::UniformRandomVariable[Min=0.1|Max=0.5]"));


  // The GPS Keep Alive messages in Team Tracking application model clients
  // with team members sending data on position, depending on activity.
  m_gpsTrackHelper = ApplicationHelper (
      SvelteUdpClient::GetTypeId (),
      SvelteUdpServer::GetTypeId ());
  m_gpsTrackHelper.SetClientAttribute ("AppName", StringValue ("GpsTrack"));

  // For traffic length, we are using a synthetic average length of 90 seconds
  // with 10 sec stdev. This will force the application to periodically stop
  // and report statistics.
  m_gpsTrackHelper.SetClientAttribute (
    "TrafficLength",
    StringValue ("ns3::NormalRandomVariable[Mean=90.0|Variance=100.0]"));

  // Model chosen: 0.5kB packets sent with uniform inter-arrival time
  // distribution ranging from 1s to 25s.
  m_gpsTrackHelper.SetClientAttribute (
    "PktSize",
    StringValue ("ns3::ConstantRandomVariable[Constant=512]"));
  m_gpsTrackHelper.SetClientAttribute (
    "PktInterval",
    StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=25.0]"));
  m_gpsTrackHelper.SetServerAttribute (
    "PktSize",
    StringValue ("ns3::ConstantRandomVariable[Constant=512]"));
  m_gpsTrackHelper.SetServerAttribute (
    "PktInterval",
    StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=25.0]"));
}

void
TrafficHelper::ConfigureApplications ()
{
  NS_LOG_FUNCTION (this);

  // Install traffic manager and applications into UE nodes.
  for (uint32_t u = 0; u < m_ueContainer.GetN (); u++)
    {
      uint64_t ueImsi = u << 4;

      m_ueNode = m_ueContainer.Get (u);
      Ptr<Ipv4> clientIpv4 = m_ueNode->GetObject<Ipv4> ();
      m_ueAddr = clientIpv4->GetAddress (1, 0).GetLocal ();

      // Each UE gets one traffic manager.
      m_ueManager = m_managerFac.Create<TrafficManager> ();
      m_ueManager->SetImsi (ueImsi);
      m_ueManager->SetController (m_controller);
      m_ueNode->AggregateObject (m_ueManager);

      // Install enabled applications into this UE.

      // TCP Apps
      //
      // Buffered video streaming
      if (m_nonBufVideo)
        {
          int videoIdx = m_nonVidRng->GetInteger ();
          m_bufVideoHelper.SetServerAttribute (
            "TraceFilename", StringValue (GetVideoFilename (videoIdx)));
          InstallAppDefault (m_bufVideoHelper, ueImsi + 1);
        }
      
      // HTTP webpage traffic
      if (m_nonHttpPage)
        {
          InstallAppDefault (m_httpPageHelper, ueImsi + 2);
        }

      // HTTP webpage traffic
      if (m_dftHttpPage)
        {
          InstallAppDefault (m_httpPageHelper, ueImsi + 3);
        }

      // UDP Apps
      //
      // Auto-pilot traffic
      if (m_gbrAutPilot)
        {
          InstallAppDefault (m_autPilotHelper, ueImsi + 4);
        }

      // Auto-pilot traffic
      if (m_nonAutPilot)
        {
          InstallAppDefault (m_autPilotHelper, ueImsi + 5);
        }

      // Open Arena game
      if (m_gbrGameOpen)
        {
          InstallAppDefault (m_gameOpenHelper, ueImsi + 6);
        }

      // Team Fortress game
      if (m_gbrGameTeam)
        {
          InstallAppDefault (m_gameTeamHelper, ueImsi + 7);
        }

      // VoIP call
      if (m_gbrVoipCall)
        {
          InstallAppDefault (m_voipCallHelper, ueImsi + 8);
        }

      // Virtual bicycle race traffic
      if (m_nonBikeRace)
        {
          InstallAppDefault (m_bikeRaceHelper, ueImsi + 9);
        }

      // GPS Team Tracking traffic
      if (m_nonGpsTrack)
        {
          InstallAppDefault (m_gpsTrackHelper, ueImsi + 10);
        }

      // Live video streaming
      if (m_gbrLivVideo)
        {
          int videoIdx = m_gbrVidRng->GetInteger ();
          m_livVideoHelper.SetServerAttribute (
            "TraceFilename", StringValue (GetVideoFilename (videoIdx)));
          InstallAppDefault (m_livVideoHelper, ueImsi + 11);
        }

      // Live video streaming
      if (m_nonLivVideo)
        {
          int videoIdx = m_nonVidRng->GetInteger ();
          m_livVideoHelper.SetServerAttribute (
            "TraceFilename", StringValue (GetVideoFilename (videoIdx)));
          InstallAppDefault (m_livVideoHelper, ueImsi + 12);
        }
    }
  m_ueManager = 0;
  m_ueNode = 0;
}

uint16_t
TrafficHelper::GetNextPortNo ()
{
  NS_ABORT_MSG_IF (m_port == 0xFFFF, "No more ports available for use.");
  return m_port++;
}

const std::string
TrafficHelper::GetVideoFilename (uint8_t idx)
{
  return m_videoDir + m_videoTrace [idx];
}

void
TrafficHelper::InstallAppDefault (ApplicationHelper& helper, uint32_t teid)
{
  NS_LOG_FUNCTION (this);

  // Create and install the applications.
  uint16_t port = 1000 + teid;
  Ptr<SvelteClient> app = helper.Install (m_ueNode, m_webNode, m_ueAddr, m_webAddr, port);
  app->SetTeid (teid);
  m_ueManager->AddSvelteClient (app);
}

} // namespace ns3
