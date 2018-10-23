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

#include <iomanip>
#include <iostream>
#include <ns3/config-store-module.h>
#include <ns3/core-module.h>
#include <ns3/internet-module.h>
#include <ns3/ofswitch13-module.h>

#include "applications/auto-pilot-server.h"
#include "applications/auto-pilot-client.h"
#include "applications/buffered-video-server.h"
#include "applications/buffered-video-client.h"
#include "applications/http-server.h"
#include "applications/http-client.h"
#include "applications/live-video-server.h"
#include "applications/live-video-client.h"
#include "applications/svelte-app-helper.h"
#include "applications/svelte-client-app.h"
#include "applications/svelte-server-app.h"
#include "applications/voip-client.h"
#include "applications/voip-server.h"
#include "custom-controller.h"
#include "traffic-manager.h"
#include "traffic-stats-calculator.h"

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("Main");

// FIXME Levar pra algum stats calculator.
uint16_t aceitos = 0, bloqueados = 0;
void
RequestCounter (uint32_t teid, bool accepted)
{
  //contar quantos aceitos e quantos bloqueados e imprimir no fim da simulacao
  if (accepted)
    {
      cout << "Aceito, teid: " << teid << endl;
      aceitos++;
    }
  else
    {
      cout << "Bloqueado" << endl;
      bloqueados++;
    }
}

// Prefixes used by input and output filenames.
static ns3::GlobalValue
  g_inputPrefix ("InputPrefix", "Common prefix for output filenames.",
                 ns3::StringValue (""),
                 ns3::MakeStringChecker ());

static ns3::GlobalValue
  g_outputPrefix ("OutputPrefix", "Common prefix for input filenames.",
                  ns3::StringValue (""),
                  ns3::MakeStringChecker ());

// Dump timeout for logging statistics.
static ns3::GlobalValue
  g_dumpTimeout ("DumpStatsTimeout", "Periodic statistics dump interval.",
                 ns3::TimeValue (Seconds (1)),
                 ns3::MakeTimeChecker ());

// Number of client hosts in the simulation.
static ns3::GlobalValue
  g_numHosts ("NumHosts", "Number of client hosts.",
              ns3::UintegerValue (4),
              ns3::MakeUintegerChecker<uint16_t> ());

void ForceDefaults  ();
void EnableProgress (uint32_t);
void EnableVerbose  (bool);
void EnableOfsLogs  (bool);

int
main (int argc, char *argv[])
{
  bool        verbose  = false;
  bool        pcap     = false;
  bool        ofsLog   = false;
  uint32_t    progress = 0;
  uint32_t    simTime  = 250;
  std::string prefix   = "";

  // Parse command line arguments
  CommandLine cmd;
  cmd.AddValue ("Verbose",  "Enable verbose output.", verbose);
  cmd.AddValue ("Pcap",     "Enable pcap output.", pcap);
  cmd.AddValue ("OfsLog",   "Enable ofsoftswitch13 logs.", ofsLog);
  cmd.AddValue ("Progress", "Simulation progress interval (sec).", progress);
  cmd.AddValue ("Prefix",   "Common prefix for filenames.", prefix);
  cmd.AddValue ("SimTime",  "Simulation stop time (sec).", simTime);
  cmd.Parse (argc, argv);

  // Update input and output prefixes from command line prefix parameter.
  NS_ASSERT_MSG (prefix != "", "Unknown prefix.");
  std::ostringstream inputPrefix, outputPrefix;
  inputPrefix << prefix;
  char lastChar = *prefix.rbegin ();
  if (lastChar != '-')
    {
      inputPrefix << "-";
    }
  outputPrefix << inputPrefix.str () << RngSeedManager::GetRun () << "-";
  Config::SetGlobal ("InputPrefix", StringValue (inputPrefix.str ()));
  Config::SetGlobal ("OutputPrefix", StringValue (outputPrefix.str ()));

  // Read the configuration file. The file existence is mandatory.
  std::string cfgFilename = prefix + ".topo";
  std::ifstream testFile (cfgFilename.c_str (), std::ifstream::in);
  NS_ASSERT_MSG (testFile.good (), "Invalid topology file " << cfgFilename);
  testFile.close ();

  Config::SetDefault ("ns3::ConfigStore::Mode", StringValue ("Load"));
  Config::SetDefault ("ns3::ConfigStore::FileFormat", StringValue ("RawText"));
  Config::SetDefault ("ns3::ConfigStore::Filename", StringValue (cfgFilename));
  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults ();

  // Parse command line again so users can override values from configuration
  // file, and force some default attribute values that cannot be overridden.
  cmd.Parse (argc, argv);
  ForceDefaults ();

  // Enable verbose output and progress report for debug purposes.
  EnableVerbose (verbose);
  EnableOfsLogs (ofsLog);















  // Create the simulation scenario.
  NS_LOG_INFO ("Creating simulation scenario...");

  // Get the number of hosts from global attribute.
  UintegerValue uintegerValue;
  GlobalValue::GetValueByName ("NumHosts", uintegerValue);
  uint32_t numHosts = uintegerValue.Get ();
  NS_LOG_INFO ("Number of hosts set to " << numHosts);

//   // Use the CsmaHelper to connect host nodes to the switch node
//   CsmaHelper csmaHelper;
//   csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
//   csmaHelper.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (200))); //40KM Fiber cable

//   Ptr<OFSwitch13InternalHelper> of13Helper =
//     CreateObjectWithAttributes<OFSwitch13InternalHelper> ("ChannelType",EnumValue (OFSwitch13Helper::DEDICATEDP2P)); // configuracao do OF

//   Ptr<Node> controllerNode = CreateObject<Node> ();
//   Ptr<CustomController> controllerApp = CreateObject<CustomController> ();
//   of13Helper->InstallController (controllerNode,controllerApp);

//   // Create the switch node
//   Ptr<Node> switchNodeUl = CreateObject<Node> ();
//   Ptr<Node> switchNodeDl = CreateObject<Node> ();
//   Ptr<Node> switchNodeHw = CreateObject<Node> ();
//   Ptr<Node> switchNodeSw = CreateObject<Node> ();


//   Names::Add ("ul",switchNodeUl);
//   Names::Add ("dl",switchNodeDl);
//   Names::Add ("hw",switchNodeHw);
//   Names::Add ("sw",switchNodeSw);

//   Ptr<OFSwitch13Device> switchDeviceUl = of13Helper->InstallSwitch (switchNodeUl).Get (0); //1
//   Ptr<OFSwitch13Device> switchDeviceDl = of13Helper->InstallSwitch (switchNodeDl).Get (0); //2

//   of13Helper->SetDeviceAttribute ("ProcessingCapacity",StringValue ("1Gbps"));
//   of13Helper->SetDeviceAttribute ("FlowTableSize",UintegerValue (512));

//   Ptr<OFSwitch13Device> switchDeviceHw = of13Helper->InstallSwitch (switchNodeHw).Get (0); //3

//   of13Helper->SetDeviceAttribute ("ProcessingCapacity",StringValue ("10Mbps"));
//   of13Helper->SetDeviceAttribute ("FlowTableSize",UintegerValue (10000));
//   of13Helper->SetDeviceAttribute ("TcamDelay",TimeValue (MicroSeconds (100)));
//   Ptr<OFSwitch13Device> switchDeviceSw = of13Helper->InstallSwitch (switchNodeSw).Get (0); //4

//   NetDeviceContainer hw2ulLink = csmaHelper.Install (NodeContainer (switchNodeHw, switchNodeUl));
//   uint32_t hw2ulPort = switchDeviceHw->AddSwitchPort (hw2ulLink.Get (0))->GetPortNo ();
//   uint32_t ul2hwPort = switchDeviceUl->AddSwitchPort (hw2ulLink.Get (1))->GetPortNo ();

//   NetDeviceContainer sw2ulLink = csmaHelper.Install (NodeContainer (switchNodeSw, switchNodeUl));
//   uint32_t sw2ulPort = switchDeviceSw->AddSwitchPort (sw2ulLink.Get (0))->GetPortNo ();
//   uint32_t ul2swPort = switchDeviceUl->AddSwitchPort (sw2ulLink.Get (1))->GetPortNo ();

//   NetDeviceContainer hw2dlLink = csmaHelper.Install (NodeContainer (switchNodeHw, switchNodeDl));
//   uint32_t hw2dlPort = switchDeviceHw->AddSwitchPort (hw2dlLink.Get (0))->GetPortNo ();
//   uint32_t dl2hwPort = switchDeviceDl->AddSwitchPort (hw2dlLink.Get (1))->GetPortNo ();

//   NetDeviceContainer sw2dlLink = csmaHelper.Install (NodeContainer (switchNodeSw, switchNodeDl));
//   uint32_t sw2dlPort = switchDeviceSw->AddSwitchPort (sw2dlLink.Get (0))->GetPortNo ();
//   uint32_t dl2swPort = switchDeviceDl->AddSwitchPort (sw2dlLink.Get (1))->GetPortNo ();

//   Ptr<Node> routerDl = CreateObject<Node>();
//   Ptr<Node> routerUl = CreateObject<Node>();
//   NetDeviceContainer routerDevices;

//   csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("10Gbps")));
//   csmaHelper.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (0))); //40KM Fiber cable

//   NetDeviceContainer router2dlLink = csmaHelper.Install (NodeContainer (routerDl, switchNodeDl));
//   uint32_t dl2routerPort = switchDeviceDl->AddSwitchPort (router2dlLink.Get (1))->GetPortNo ();
//   routerDevices.Add (router2dlLink.Get (0));

//   NetDeviceContainer router2ulLink = csmaHelper.Install (NodeContainer (routerUl, switchNodeUl));
//   uint32_t ul2routerPort = switchDeviceUl->AddSwitchPort (router2ulLink.Get (1))->GetPortNo ();
//   routerDevices.Add (router2ulLink.Get (0));

//   controllerApp->NotifyHwSwitch (switchDeviceHw, hw2ulPort, hw2dlPort); //Nao trocar ordem
//   controllerApp->NotifySwSwitch (switchDeviceSw, sw2ulPort, sw2dlPort);
//   controllerApp->NotifyUlSwitch (switchDeviceUl, ul2hwPort, ul2swPort, ul2routerPort);
//   controllerApp->NotifyDlSwitch (switchDeviceDl, dl2hwPort, dl2swPort, dl2routerPort);

//   of13Helper->CreateOpenFlowChannels ();

//   //Create two host nodes
//   NodeContainer clientNodes;
//   NodeContainer serverNodes;
//   clientNodes.Create (numHosts);
//   serverNodes.Create (numHosts);

//   NetDeviceContainer clientDevices;
//   NetDeviceContainer serverDevices;
//   NetDeviceContainer clientRouterDevices;
//   NetDeviceContainer serverRouterDevices;

//   for (uint32_t i = 0; i < numHosts; i++)
//     {
//       NetDeviceContainer clientTemp = csmaHelper.Install (NodeContainer (routerUl, clientNodes.Get (i)));
//       clientRouterDevices.Add (clientTemp.Get (0));
//       clientDevices.Add (clientTemp.Get (1));

//       NetDeviceContainer serverTemp = csmaHelper.Install (NodeContainer (routerDl, serverNodes.Get (i)));

//       serverRouterDevices.Add (serverTemp.Get (0));
//       serverDevices.Add (serverTemp.Get (1));
//     }

//   // Install the TCP/IP stack into hosts nodes
//   InternetStackHelper internet;
//   internet.Install (clientNodes);
//   internet.Install (serverNodes);
//   internet.Install (routerDl);
//   internet.Install (routerUl);

//   // Set IPv4 host addresses
//   Ipv4AddressHelper ipv4helpr;
//   ipv4helpr.SetBase ("10.0.0.0", "255.0.0.0", "0.1.0.0");
//   //ipv4helpr.SetBase ("10.1.0.0", "255.255.0.0");
//   Ipv4InterfaceContainer clientIpIfaces = ipv4helpr.Assign (clientDevices);

//   //ipv4helpr.SetBase ("10.2.0.0", "255.255.0.0");
//   ipv4helpr.SetBase ("10.0.0.0", "255.0.0.0","0.2.0.0");//"255.255.0.0"
//   Ipv4InterfaceContainer serverIpIfaces = ipv4helpr.Assign (serverDevices);

//   SvelteAppHelper autoPilotHelper (AutoPilotClient::GetTypeId (),AutoPilotServer::GetTypeId ());
//   SvelteAppHelper bufferedVideoHelper (BufferedVideoClient::GetTypeId (),BufferedVideoServer::GetTypeId ());
//   SvelteAppHelper httpHelper (HttpClient::GetTypeId (),HttpServer::GetTypeId ());
//   SvelteAppHelper liveVideoHelper (LiveVideoClient::GetTypeId (),LiveVideoServer::GetTypeId ());
//   SvelteAppHelper voipHelper (VoipClient::GetTypeId (),VoipServer::GetTypeId ());

//   ObjectFactory managerFactory;
//   managerFactory.SetTypeId (TrafficManager::GetTypeId ());

//   for (uint32_t i = 0; i < clientDevices.GetN (); i++)
//     {
//       Ptr<Node> clientNode = clientNodes.Get (i);
//       Ptr<TrafficManager> manager = managerFactory.Create<TrafficManager>();
//       uint64_t imsi = i << 4;
//       manager->SetImsi (imsi);
//       manager->SetController (controllerApp);
//       clientNode->AggregateObject (manager);
// //tcp
//       //bufferedVideo
//       Ptr<SvelteClientApp> appBufferedVideo = bufferedVideoHelper.Install (clientNode, serverNodes.Get (i),
//                                                                            clientIpIfaces.GetAddress (i), serverIpIfaces.GetAddress (i), 10000 + imsi + 1);
//       appBufferedVideo->SetEpsBearer (EpsBearer (EpsBearer::NGBR_VIDEO_TCP_OPERATOR,GbrQosInformation ()));
//       appBufferedVideo->SetTeid (imsi + 1);
//       manager->AddSvelteClientApp (appBufferedVideo);

//       //http
//       Ptr<SvelteClientApp> appHttp = httpHelper.Install (clientNode, serverNodes.Get (i),
//                                                          clientIpIfaces.GetAddress (i), serverIpIfaces.GetAddress (i), 10000 + imsi + 2);
//       appHttp->SetEpsBearer (EpsBearer (EpsBearer::NGBR_VIDEO_TCP_PREMIUM,GbrQosInformation ()));
//       appHttp->SetTeid (imsi + 2);
//       manager->AddSvelteClientApp (appHttp);
// //udp
//       //autoPilot
//       Ptr<SvelteClientApp> appAutoPilot = autoPilotHelper.Install (clientNode, serverNodes.Get (i),
//                                                                    clientIpIfaces.GetAddress (i), serverIpIfaces.GetAddress (i), 10000 + imsi + 3);
//       appAutoPilot->SetEpsBearer (EpsBearer (EpsBearer::GBR_GAMING,GbrQosInformation ()));
//       appAutoPilot->SetTeid (imsi + 3);
//       manager->AddSvelteClientApp (appAutoPilot);

//       //liveVideo //FIXME TraceFilename
//       Ptr<SvelteClientApp> appLiveVideo = liveVideoHelper.Install (clientNode, serverNodes.Get (i),
//                                                                    clientIpIfaces.GetAddress (i), serverIpIfaces.GetAddress (i), 10000 + imsi + 4);
//       appLiveVideo->SetEpsBearer (EpsBearer (EpsBearer::GBR_NON_CONV_VIDEO,GbrQosInformation ()));
//       appLiveVideo->SetTeid (imsi + 4);
//       manager->AddSvelteClientApp (appLiveVideo);

//       //voip
//       Ptr<SvelteClientApp> appVoip = voipHelper.Install (clientNode, serverNodes.Get (i),
//                                                          clientIpIfaces.GetAddress (i), serverIpIfaces.GetAddress (i), 10000 + imsi + 5);
//       appVoip->SetEpsBearer (EpsBearer (EpsBearer::GBR_CONV_VOICE,GbrQosInformation ()));
//       appVoip->SetTeid (imsi + 5);
//       manager->AddSvelteClientApp (appVoip);

//     }

//   Ptr<TrafficStatsCalculator> stats = CreateObject<TrafficStatsCalculator>();

// /*
//   ApplicationContainer pingApps;

//   for (int i = 0; i < numHosts; i++)
//     {
//       // Configure ping application between hosts
//       V4PingHelper pingHelper = V4PingHelper (serverIpIfaces.GetAddress (i));
//       pingHelper.SetAttribute ("Verbose", BooleanValue (true));
//       pingApps.Add (pingHelper.Install (clientNodes.Get (i)));
//     }
//   pingApps.Start (Seconds (1));
// */


//   // Always enable datapath stats.
//   of13Helper->EnableDatapathStats ("switch-stats", true);

//   // Selective enable pcap traces at hosts and OpenFlow network.  
//   if (pcap)
//     {
//       of13Helper->EnableOpenFlowPcap ("openflow");

//       csmaHelper.EnablePcap ("clients", clientDevices);
//       csmaHelper.EnablePcap ("servers", serverDevices);
//       csmaHelper.EnablePcap ("switchUl", NodeContainer (switchNodeUl));
//       csmaHelper.EnablePcap ("switchDl", NodeContainer (switchNodeDl));
//       csmaHelper.EnablePcap ("switchHw", NodeContainer (switchNodeHw));
//       csmaHelper.EnablePcap ("switchSw", NodeContainer (switchNodeSw));
//     }

//   Config::ConnectWithoutContext (
//     "/NodeList/*/ApplicationList/*/$ns3::CustomController/BearerRequest",
//     MakeCallback (&RequestCounter));


  // Populating routing and ARP tables. The 'perfect' ARP used here comes from
  // the patch at https://www.nsnam.org/bugzilla/show_bug.cgi?id=187. This
  // patch uses a single ARP cache shared among all nodes. Some developers have
  // pointed that this implementation may fail if a node change what it thinks
  // that it's a local cache, or if there are global MAC hardware duplications.
  // Anyway, I've decided to use this to simplify the controller logic.
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  ArpCache::PopulateArpCaches ();

  // Run the simulation.
  std::cout << "Simulating..." << std::endl;
  EnableProgress (progress);
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();

  // FIXME Mover para um stats calculator.
  cout << "Aceitos: " << aceitos << endl;
  cout << "Bloqueados: " << bloqueados << endl;
}

void ForceDefaults ()
{
  //
  // Since we are using an external OpenFlow library that expects complete
  // network packets, we must enable checksum computations.
  //
  Config::SetGlobal (
    "ChecksumEnabled", BooleanValue (true));

  //
  // The minimum (default) value for TCP MSS is 536, and there's no dynamic MTU
  // discovery implemented yet in ns-3. To allow larger TCP packets, we defined
  // this value to 1400, based on 1500 bytes for Ethernet v2 MTU, and
  // considering 8 bytes for PPPoE header, 40 bytes for GTP/UDP/IP tunnel
  // headers, and 52 bytes for default TCP/IP headers. Don't use higher values
  // to avoid packet fragmentation.
  //
  Config::SetDefault (
    "ns3::TcpSocket::SegmentSize", UintegerValue (1400));

  //
  // The default number of TCP connection attempts before returning a failure
  // is set to 6 in ns-3, with an interval of 3 seconds between each attempt.
  // We are going to keep the number of attempts but with a small interval of
  // 500 ms between them.
  //
  Config::SetDefault (
    "ns3::TcpSocket::ConnTimeout", TimeValue (MilliSeconds (500)));

  //
  // The default TCP minimum retransmit timeout value is set to 1 second in
  // ns-3, according to RFC 6298. However, it takes to long to recover from
  // lost packets. Linux uses 200 ms as the default value, however, this was
  // leading to many unnecessary retransmitted packets. We are going to use an
  // intermediate value.
  //
  Config::SetDefault (
    "ns3::TcpSocketBase::MinRto", TimeValue (MilliSeconds (500)));

  //
  // Increasing the default MTU for virtual network devices, which are used as
  // OpenFlow virtual port devices.
  //
  Config::SetDefault (
    "ns3::VirtualNetDevice::Mtu", UintegerValue (3000));

  //
  // Whenever possible, use the full-duplex CSMA channel to improve throughput.
  // The code will automatically fall back to half-duplex mode for more than
  // two devices in the same channel.
  // This implementation is not available in default ns-3 code, and I got it
  // from https://codereview.appspot.com/187880044/
  //
  Config::SetDefault (
    "ns3::CsmaChannel::FullDuplex", BooleanValue (true));

  //
  // Fix the number of output priority queues on every switch port to 3.
  //
  Config::SetDefault (
    "ns3::OFSwitch13Queue::NumQueues", UintegerValue (3));

  //
  // Enable detailed OpenFlow datapath statistics.
  //
  Config::SetDefault (
    "ns3::OFSwitch13StatsCalculator::PipelineDetails", BooleanValue (true));
}

void
EnableProgress (uint32_t interval)
{
  if (interval)
    {
      int64_t now = Simulator::Now ().ToInteger (Time::S);
      std::cout << "Current simulation time: +" << now << ".0s" << std::endl;
      Simulator::Schedule (Seconds (interval), &EnableProgress, interval);
    }
}

void
EnableVerbose (bool enable)
{
  if (enable)
    {
      LogLevel logLevelWarn = static_cast<ns3::LogLevel> (
          LOG_PREFIX_FUNC | LOG_PREFIX_TIME | LOG_LEVEL_WARN);
      NS_UNUSED (logLevelWarn);

      LogLevel logLevelWarnInfo = static_cast<ns3::LogLevel> (
          LOG_PREFIX_FUNC | LOG_PREFIX_TIME | LOG_LEVEL_WARN | LOG_INFO);
      NS_UNUSED (logLevelWarnInfo);

      LogLevel logLevelInfo = static_cast<ns3::LogLevel> (
          LOG_PREFIX_FUNC | LOG_PREFIX_TIME | LOG_LEVEL_INFO);
      NS_UNUSED (logLevelInfo);

      LogLevel logLevelAll = static_cast<ns3::LogLevel> (
          LOG_PREFIX_FUNC | LOG_PREFIX_TIME | LOG_LEVEL_ALL);
      NS_UNUSED (logLevelAll);

      // Common components.
      LogComponentEnable ("Main",                     logLevelWarnInfo);

      // // Helper components.
      // LogComponentEnable ("TrafficHelper",            logLevelWarnInfo);

      // LogComponentEnable ("TrafficManager",           logLevelWarnInfo);


      // // Applications.
      // LogComponentEnable ("AppStatsCalculator",       logLevelWarnInfo);
      // LogComponentEnable ("BufferedVideoClient",      logLevelWarnInfo);
      // LogComponentEnable ("BufferedVideoServer",      logLevelWarnInfo);
      // LogComponentEnable ("HttpClient",               logLevelWarnInfo);
      // LogComponentEnable ("HttpServer",               logLevelWarnInfo);
      // LogComponentEnable ("LiveVideoClient",          logLevelWarnInfo);
      // LogComponentEnable ("LiveVideoServer",          logLevelWarnInfo);
      // LogComponentEnable ("SvelteClient",             logLevelWarnInfo);
      // LogComponentEnable ("SvelteServer",             logLevelWarnInfo);
      // LogComponentEnable ("SvelteUdpClient",          logLevelWarnInfo);
      // LogComponentEnable ("SvelteUdpServer",          logLevelWarnInfo);

      // // Statistic components.
      // LogComponentEnable ("AdmissionStatsCalculator", logLevelWarnInfo);
      // LogComponentEnable ("BackhaulStatsCalculator",  logLevelWarnInfo);
      // LogComponentEnable ("LteRrcStatsCalculator",    logLevelWarnInfo);
      // LogComponentEnable ("PgwTftStatsCalculator",    logLevelWarnInfo);
      // LogComponentEnable ("TrafficStatsCalculator",   logLevelWarnInfo);

      // OFSwitch13 module components.
      LogComponentEnable ("OFSwitch13Controller",     logLevelWarn);
      LogComponentEnable ("OFSwitch13Device",         logLevelWarn);
      LogComponentEnable ("OFSwitch13Helper",         logLevelWarn);
      LogComponentEnable ("OFSwitch13Interface",      logLevelWarn);
      LogComponentEnable ("OFSwitch13Port",           logLevelWarn);
      LogComponentEnable ("OFSwitch13Queue",          logLevelWarn);
      LogComponentEnable ("OFSwitch13SocketHandler",  logLevelWarn);
    }
}

void
EnableOfsLogs (bool enable)
{
  if (enable)
    {
      StringValue stringValue;
      GlobalValue::GetValueByName ("OutputPrefix", stringValue);
      std::string prefix = stringValue.Get ();
      ofs::EnableLibraryLog (true, prefix);
    }
}