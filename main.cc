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
#include "custom-controller.h"
#include "traffic-manager.h"
#include "traffic-helper.h"
#include "traffic-statistics.h"

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("Main");

// Prefixes used by input and output filenames.
static ns3::GlobalValue
  g_inputPrefix ("InputPrefix", "Common prefix for output filenames.",
                 ns3::StringValue (""),
                 ns3::MakeStringChecker ());

static ns3::GlobalValue
  g_outputPrefix ("OutputPrefix", "Common prefix for input filenames.",
                  ns3::StringValue (""),
                  ns3::MakeStringChecker ());

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

  // Configure the CsmaHelper to connect OpenFlow switches (40KM Fiber cable)
  CsmaHelper csmaHelper;
  csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csmaHelper.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (200)));

  // Create the OpenFlow helper.
  Ptr<OFSwitch13InternalHelper> of13Helper =
    CreateObjectWithAttributes<OFSwitch13InternalHelper> (
      "ChannelType", EnumValue (OFSwitch13Helper::DEDICATEDP2P));

  // Create the OpenFlow controller.
  Ptr<Node> controllerNode = CreateObject<Node> ();
  Names::Add ("ct", controllerNode);
  Ptr<CustomController> controllerApp = CreateObject<CustomController> ();
  of13Helper->InstallController (controllerNode, controllerApp);

  // Create and name the switch nodes.
  NodeContainer switchNodes;
  switchNodes.Create (4);
  Ptr<Node> switchNodeUl = switchNodes.Get (0);
  Ptr<Node> switchNodeDl = switchNodes.Get (1);
  Ptr<Node> switchNodeHw = switchNodes.Get (2);
  Ptr<Node> switchNodeSw = switchNodes.Get (3);
  Names::Add ("ul", switchNodeUl);
  Names::Add ("dl", switchNodeDl);
  Names::Add ("hw", switchNodeHw);
  Names::Add ("sw", switchNodeSw);

  // Configure switch nodes UL and DL as standard OpenFlow switches.
  of13Helper->SetDeviceAttribute ("PipelineTables", UintegerValue (3));
  Ptr<OFSwitch13Device> switchDeviceUl = of13Helper->InstallSwitch (switchNodeUl).Get (0);
  Ptr<OFSwitch13Device> switchDeviceDl = of13Helper->InstallSwitch (switchNodeDl).Get (0);

  // Configure switch node HW as a hardware-based OpenFlow switch.
  of13Helper->SetDeviceAttribute ("PipelineTables", UintegerValue (1));
  of13Helper->SetDeviceAttribute ("ProcessingCapacity", StringValue ("1Gbps"));
  of13Helper->SetDeviceAttribute ("FlowTableSize", UintegerValue (512));
  Ptr<OFSwitch13Device> switchDeviceHw = of13Helper->InstallSwitch (switchNodeHw).Get (0);

  // Configure switch node SW as a software-based OpenFlow switch.
  of13Helper->SetDeviceAttribute ("PipelineTables", UintegerValue (1));
  of13Helper->SetDeviceAttribute ("ProcessingCapacity", StringValue ("10Mbps"));
  of13Helper->SetDeviceAttribute ("FlowTableSize", UintegerValue (10000));
  of13Helper->SetDeviceAttribute ("TcamDelay", TimeValue (MicroSeconds (100)));
  Ptr<OFSwitch13Device> switchDeviceSw = of13Helper->InstallSwitch (switchNodeSw).Get (0);

  // Connecting switches.
  NetDeviceContainer hw2ulLink = csmaHelper.Install (switchNodeHw, switchNodeUl);
  uint32_t hw2ulPort = switchDeviceHw->AddSwitchPort (hw2ulLink.Get (0))->GetPortNo ();
  uint32_t ul2hwPort = switchDeviceUl->AddSwitchPort (hw2ulLink.Get (1))->GetPortNo ();

  NetDeviceContainer sw2ulLink = csmaHelper.Install (switchNodeSw, switchNodeUl);
  uint32_t sw2ulPort = switchDeviceSw->AddSwitchPort (sw2ulLink.Get (0))->GetPortNo ();
  uint32_t ul2swPort = switchDeviceUl->AddSwitchPort (sw2ulLink.Get (1))->GetPortNo ();

  NetDeviceContainer hw2dlLink = csmaHelper.Install (switchNodeHw, switchNodeDl);
  uint32_t hw2dlPort = switchDeviceHw->AddSwitchPort (hw2dlLink.Get (0))->GetPortNo ();
  uint32_t dl2hwPort = switchDeviceDl->AddSwitchPort (hw2dlLink.Get (1))->GetPortNo ();

  NetDeviceContainer sw2dlLink = csmaHelper.Install (switchNodeSw, switchNodeDl);
  uint32_t sw2dlPort = switchDeviceSw->AddSwitchPort (sw2dlLink.Get (0))->GetPortNo ();
  uint32_t dl2swPort = switchDeviceDl->AddSwitchPort (sw2dlLink.Get (1))->GetPortNo ();

  // Notify the controller about switches (don't change the order!)
  controllerApp->NotifyHwSwitch (switchDeviceHw, hw2ulPort, hw2dlPort);
  controllerApp->NotifySwSwitch (switchDeviceSw, sw2ulPort, sw2dlPort);

  controllerApp->NotifyUlSwitch (switchDeviceUl, ul2hwPort, ul2swPort);
  controllerApp->NotifyDlSwitch (switchDeviceDl, dl2hwPort, dl2swPort);


  // Get the number of hosts from global attribute.
  UintegerValue uintegerValue;
  GlobalValue::GetValueByName ("NumHosts", uintegerValue);
  uint32_t numHosts = uintegerValue.Get ();
  NS_LOG_INFO ("Number of hosts set to " << numHosts);

  // Create the server and set its name.
  Ptr<Node> serverNode = CreateObject<Node> ();
  Names::Add ("sv", serverNode);

  // Create the client nodes and set their names.
  NodeContainer clientNodes;
  clientNodes.Create (numHosts);
  for (uint32_t i = 0; i < numHosts; i++)
    {
      std::ostringstream name;
      name << "cl" << i + 1;
      Names::Add (name.str (), clientNodes.Get (i));
    }

  // Install the TCP/IP stack into hosts nodes.
  InternetStackHelper internet;
  internet.Install (serverNode);
  internet.Install (clientNodes);

  // Configure IPv4 addresses helper.
  Ipv4AddressHelper ipv4helpr;
  ipv4helpr.SetBase ("10.0.0.0", "255.0.0.0");

  // Configure the CsmaHelper to connect hosts to switches.
  csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("10Gbps")));
  csmaHelper.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (0)));

  // Connect the single server node to the DL switch.
  NetDeviceContainer dl2svLink = csmaHelper.Install (switchNodeDl, serverNode);
  uint32_t dl2svPort = switchDeviceDl->AddSwitchPort (dl2svLink.Get (0))->GetPortNo ();
  NetDeviceContainer serverDevice (dl2svLink.Get (1));

  // Assign IP to the server node and notify the controller.
  Ipv4InterfaceContainer serverIpIface = ipv4helpr.Assign (serverDevice);
  controllerApp->NotifyDl2Sv (dl2svPort, serverIpIface.GetAddress (0));

  NetDeviceContainer clientDevices;
  Ipv4InterfaceContainer clientIpIfaces;
  for (uint32_t i = 0; i < numHosts; i++)
    {
      // Connect each client node to the UL switch.
      NetDeviceContainer ul2clLink = csmaHelper.Install (switchNodeUl, clientNodes.Get (i));
      uint32_t ul2clPort = switchDeviceUl->AddSwitchPort (ul2clLink.Get (0))->GetPortNo ();
      clientDevices.Add (ul2clLink.Get (1));

      // Assign IP to the client node and notify the controller.
      Ipv4InterfaceContainer tempIpIface = ipv4helpr.Assign (ul2clLink.Get (1));
      clientIpIfaces.Add (tempIpIface);
      controllerApp->NotifyUl2Cl (ul2clPort, tempIpIface.GetAddress (0));
    }

  // Configure the OpenFlow channel and notify the controller that we are done.
  of13Helper->CreateOpenFlowChannels ();
  controllerApp->NotifyTopologyBuilt ();

  // Configure the traffic helper.
  Ptr<TrafficHelper> trafficHelper =
    CreateObject<TrafficHelper> (controllerApp, serverNode, clientNodes);

  // Configure the stats calculator.
  Ptr<TrafficStatistics> stats = CreateObject<TrafficStatistics> ();

  // Always enable datapath stats.
  StringValue stringValue;
  GlobalValue::GetValueByName ("OutputPrefix", stringValue);
  std::string outPrefix = stringValue.Get ();
  of13Helper->EnableDatapathStats (outPrefix + "switch-stats", true);

  // Selective enable pcap traces at hosts and OpenFlow network.
  if (pcap)
    {
      of13Helper->EnableOpenFlowPcap (outPrefix + "openflow", true);

      csmaHelper.EnablePcap (outPrefix + "client", clientDevices, true);
      csmaHelper.EnablePcap (outPrefix + "server", serverDevice,  true);
      csmaHelper.EnablePcap (outPrefix + "switch", switchNodes,   true);
    }

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
}

void ForceDefaults ()
{
  //
  // Since we are using an external OpenFlow library that expects complete
  // network packets, we must enable checksum computations.
  //
  Config::SetGlobal ("ChecksumEnabled", BooleanValue (true));

  //
  // The minimum (default) value for TCP MSS is 536, and there's no dynamic MTU
  // discovery implemented yet in ns-3. To allow larger TCP packets, we defined
  // this value to 1400, based on 1500 bytes for Ethernet v2 MTU, and
  // considering 8 bytes for PPPoE header, 40 bytes for GTP/UDP/IP tunnel
  // headers, and 52 bytes for default TCP/IP headers. Don't use higher values
  // to avoid packet fragmentation.
  //
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1400));

  //
  // The default number of TCP connection attempts before returning a failure
  // is set to 6 in ns-3, with an interval of 3 seconds between each attempt.
  // We are going to keep the number of attempts but with a small interval of
  // 500 ms between them.
  //
  Config::SetDefault ("ns3::TcpSocket::ConnTimeout", TimeValue (MilliSeconds (500)));

  //
  // The default TCP minimum retransmit timeout value is set to 1 second in
  // ns-3, according to RFC 6298. However, it takes to long to recover from
  // lost packets. Linux uses 200 ms as the default value, however, this was
  // leading to many unnecessary retransmitted packets. We are going to use an
  // intermediate value.
  //
  Config::SetDefault ("ns3::TcpSocketBase::MinRto", TimeValue (MilliSeconds (500)));

  //
  // Increasing the default MTU for virtual network devices, which are used as
  // OpenFlow virtual port devices.
  //
  Config::SetDefault ("ns3::VirtualNetDevice::Mtu", UintegerValue (3000));

  //
  // Whenever possible, use the full-duplex CSMA channel to improve throughput.
  // The code will automatically fall back to half-duplex mode for more than
  // two devices in the same channel.
  // This implementation is not available in default ns-3 code, and I got it
  // from https://codereview.appspot.com/187880044/
  //
  Config::SetDefault ("ns3::CsmaChannel::FullDuplex", BooleanValue (true));

  //
  // Fix the number of output priority queues on every switch port to 3.
  //
  Config::SetDefault ("ns3::OFSwitch13Queue::NumQueues", UintegerValue (3));

  //
  // Enable detailed OpenFlow datapath statistics.
  //
  Config::SetDefault ("ns3::OFSwitch13StatsCalculator::PipelineDetails", BooleanValue (true));
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

      // Scenario components.
      LogComponentEnable ("Main",                     logLevelAll);
      LogComponentEnable ("CustomController",         logLevelAll);
      LogComponentEnable ("TrafficHelper",            logLevelAll);
      LogComponentEnable ("TrafficManager",           logLevelAll);
      LogComponentEnable ("TrafficStatistics",        logLevelAll);

      // Applications.
      LogComponentEnable ("AppStatsCalculator",       logLevelWarnInfo);
      LogComponentEnable ("BufferedVideoClient",      logLevelWarnInfo);
      LogComponentEnable ("BufferedVideoServer",      logLevelWarnInfo);
      LogComponentEnable ("HttpClient",               logLevelWarnInfo);
      LogComponentEnable ("HttpServer",               logLevelWarnInfo);
      LogComponentEnable ("LiveVideoClient",          logLevelWarnInfo);
      LogComponentEnable ("LiveVideoServer",          logLevelWarnInfo);
      LogComponentEnable ("SvelteClient",             logLevelWarnInfo);
      LogComponentEnable ("SvelteServer",             logLevelWarnInfo);
      LogComponentEnable ("SvelteUdpClient",          logLevelWarnInfo);
      LogComponentEnable ("SvelteUdpServer",          logLevelWarnInfo);

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