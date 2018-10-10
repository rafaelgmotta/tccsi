/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 University of Campinas (Unicamp)
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
 *         Vitor M. Eichemberger <vitor.marge@gmail.com>
 */
/*
*
*
*         link1+------------------+     link4    +------------------+     link3    +------------------+ link2
*   Host 0 === | OpenFlow switch esquerda1 |  ==========  | OpenFlow switch3 |  ==========  | OpenFlow switch direita2 |  === Host 1
*              +------------------+              +------------------+              +------------------+
*              port 1:host          \                                          /  port 1: host
*              port 2:switch 2       \  link5   +------------------+   link6  /   port 2: switch 2
*              port 3:switch 4         =======  | OpenFlow switch4 |  =======     port 3: switch 4
*                                                        +------------------+

host 0 para host 1: switches 1,2,3
host 1 para host 0: switches 3,4,1

*/

#include <ns3/csma-module.h>
#include <ns3/internet-apps-module.h>
#include "custom-controller.h"
#include "traffic-manager.h"
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
#include "traffic-stats-calculator.h"

using namespace ns3;
using namespace std;
int
main (int argc, char *argv[])
{
  uint16_t simTime = 100;
  uint16_t numHosts = 4;
  bool verbose = false;
  bool trace = false;

  // Configure command line parameters
  CommandLine cmd;
  cmd.AddValue ("simTime", "Simulation time (seconds)", simTime);
  cmd.AddValue ("verbose", "Enable verbose output", verbose);
  cmd.AddValue ("trace", "Enable datapath stats and pcap traces", trace);
  cmd.AddValue ("hosts", "Number of pairs os hosts", numHosts);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      OFSwitch13Helper::EnableDatapathLogs ();
      LogComponentEnable ("OFSwitch13Interface", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Device", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Port", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Queue", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13SocketHandler", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Controller", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13LearningController", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13Helper", LOG_LEVEL_ALL);
      LogComponentEnable ("OFSwitch13InternalHelper", LOG_LEVEL_ALL);
      LogComponentEnable ("SvelteClientApp", LOG_LEVEL_ALL);
      LogComponentEnable ("SvelteServerApp", LOG_LEVEL_ALL);
      LogComponentEnable ("VoipClient", LOG_LEVEL_ALL);
      LogComponentEnable ("VoipServer", LOG_LEVEL_ALL);

    }
    LogComponentEnable ("TrafficManager", LOG_LEVEL_ALL);
  // Enable checksum computations (required by OFSwitch13 module)
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));




  // Use the CsmaHelper to connect host nodes to the switch node
  CsmaHelper csmaHelper;
  csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csmaHelper.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (200))); //40KM Fiber cable

  Ptr<OFSwitch13InternalHelper> of13Helper =
    CreateObjectWithAttributes<OFSwitch13InternalHelper> ("ChannelType",EnumValue (OFSwitch13Helper::DEDICATEDP2P)); // configuracao do OF

  Ptr<Node> controllerNode = CreateObject<Node> ();
  Ptr<CustomController> controllerApp = CreateObject<CustomController> ();
  of13Helper->InstallController (controllerNode,controllerApp);

  // Create the switch node
  Ptr<Node> switchNodeUl = CreateObject<Node> ();
  Ptr<Node> switchNodeDl = CreateObject<Node> ();
  Ptr<Node> switchNodeHw = CreateObject<Node> ();
  Ptr<Node> switchNodeSw = CreateObject<Node> ();
  Names::Add("ul",switchNodeUl);
  Names::Add("dl",switchNodeDl);
  Names::Add("hw",switchNodeHw);
  Names::Add("sw",switchNodeSw);

  Ptr<OFSwitch13Device> switchDeviceUl = of13Helper->InstallSwitch (switchNodeUl).Get (0); //1
  Ptr<OFSwitch13Device> switchDeviceDl = of13Helper->InstallSwitch (switchNodeDl).Get (0); //2
  
  of13Helper->SetDeviceAttribute("ProcessingCapacity",StringValue("1Gbps"));
  of13Helper->SetDeviceAttribute("FlowTableSize",UintegerValue(128));

  Ptr<OFSwitch13Device> switchDeviceHw = of13Helper->InstallSwitch (switchNodeHw).Get (0); //3
  
  of13Helper->SetDeviceAttribute("ProcessingCapacity",StringValue("10Mbps"));
  of13Helper->SetDeviceAttribute("FlowTableSize",UintegerValue(10000));
  of13Helper->SetDeviceAttribute("TcamDelay",TimeValue(MicroSeconds(100)));
  Ptr<OFSwitch13Device> switchDeviceSw = of13Helper->InstallSwitch (switchNodeSw).Get (0); //4

  NetDeviceContainer hw2ulLink = csmaHelper.Install (NodeContainer (switchNodeHw, switchNodeUl));
  uint32_t hw2ulPort = switchDeviceHw->AddSwitchPort (hw2ulLink.Get (0))->GetPortNo ();
  uint32_t ul2hwPort = switchDeviceUl->AddSwitchPort (hw2ulLink.Get (1))->GetPortNo ();

  NetDeviceContainer sw2ulLink = csmaHelper.Install (NodeContainer (switchNodeSw, switchNodeUl));
  uint32_t sw2ulPort = switchDeviceSw->AddSwitchPort (sw2ulLink.Get (0))->GetPortNo ();
  uint32_t ul2swPort = switchDeviceUl->AddSwitchPort (sw2ulLink.Get (1))->GetPortNo ();

  NetDeviceContainer hw2dlLink = csmaHelper.Install (NodeContainer (switchNodeHw, switchNodeDl));
  uint32_t hw2dlPort = switchDeviceHw->AddSwitchPort (hw2dlLink.Get (0))->GetPortNo ();
  uint32_t dl2hwPort = switchDeviceDl->AddSwitchPort (hw2dlLink.Get (1))->GetPortNo ();

  NetDeviceContainer sw2dlLink = csmaHelper.Install (NodeContainer (switchNodeSw, switchNodeDl));
  uint32_t sw2dlPort = switchDeviceSw->AddSwitchPort (sw2dlLink.Get (0))->GetPortNo ();
  uint32_t dl2swPort = switchDeviceDl->AddSwitchPort (sw2dlLink.Get (1))->GetPortNo ();

  //Create two host nodes
  NodeContainer clientNodes;
  NodeContainer serverNodes;
  clientNodes.Create (numHosts);//criar mais hosts e instalar apps pra gerar trafego
  serverNodes.Create (numHosts);

  NetDeviceContainer clientDevices;
  NetDeviceContainer serverDevices;

  CsmaHelper csmaHelperClSv;
  csmaHelperClSv.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("10Gbps")));
  csmaHelperClSv.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (1)));

  NetDeviceContainer cl2ulLink = csmaHelperClSv.Install (NodeContainer (switchNodeUl,clientNodes));
  uint32_t ul2clPort = switchDeviceUl->AddSwitchPort (cl2ulLink.Get (0))->GetPortNo ();

  NetDeviceContainer dl2svLink = csmaHelperClSv.Install (NodeContainer (switchNodeDl,serverNodes));
  uint32_t dl2svPort = switchDeviceDl->AddSwitchPort (dl2svLink.Get (0))->GetPortNo ();

  for (uint32_t i = 1; i < cl2ulLink.GetN (); i++)
    {
      clientDevices.Add (cl2ulLink.Get (i));
      serverDevices.Add (dl2svLink.Get (i));
    }
  controllerApp->NotifyHwSwitch (switchDeviceHw, hw2ulPort, hw2dlPort); //Nao trocar ordem
  controllerApp->NotifySwSwitch (switchDeviceSw, sw2ulPort, sw2dlPort);
  controllerApp->NotifyUlSwitch (switchDeviceUl, ul2hwPort, ul2swPort, ul2clPort);
  controllerApp->NotifyDlSwitch (switchDeviceDl, dl2hwPort, dl2swPort, dl2svPort);


  of13Helper->CreateOpenFlowChannels ();

  // Install the TCP/IP stack into hosts nodes
  InternetStackHelper internet;
  internet.Install (clientNodes);
  internet.Install (serverNodes);

  // Set IPv4 host addresses
  Ipv4AddressHelper ipv4helpr;
  ipv4helpr.SetBase ("10.0.0.0", "255.0.0.0", "0.1.0.0");
  Ipv4InterfaceContainer clientIpIfaces = ipv4helpr.Assign (clientDevices);

  ipv4helpr.SetBase ("10.0.0.0", "255.0.0.0","0.2.0.0");
  Ipv4InterfaceContainer serverIpIfaces = ipv4helpr.Assign (serverDevices);

  SvelteAppHelper autoPilotHelper(AutoPilotClient::GetTypeId(),AutoPilotServer::GetTypeId());
  SvelteAppHelper bufferedVideoHelper(BufferedVideoClient::GetTypeId(),BufferedVideoServer::GetTypeId());
  SvelteAppHelper httpHelper(HttpClient::GetTypeId(),HttpServer::GetTypeId());
  SvelteAppHelper liveVideoHelper(LiveVideoClient::GetTypeId(),LiveVideoServer::GetTypeId());
  SvelteAppHelper voipHelper(VoipClient::GetTypeId(),VoipServer::GetTypeId());

  ObjectFactory managerFactory;
  managerFactory.SetTypeId(TrafficManager::GetTypeId());

  for (uint32_t i = 0; i < clientDevices.GetN(); i++)
  {
      Ptr<Node> clientNode = clientNodes.Get(i);
      Ptr<TrafficManager> manager = managerFactory.Create<TrafficManager>();
      uint64_t imsi = i<<4;
      manager->SetImsi(imsi);
      manager->SetController(controllerApp);
      clientNode->AggregateObject(manager); 
//tcp
      //bufferedVideo
      Ptr<SvelteClientApp> appBufferedVideo = bufferedVideoHelper.Install(clientNode, serverNodes.Get(i),
        clientIpIfaces.GetAddress(i), serverIpIfaces.GetAddress(i), 10000+imsi+1);
      appBufferedVideo->SetEpsBearer(EpsBearer(EpsBearer::NGBR_VIDEO_TCP_OPERATOR,GbrQosInformation()));
      appBufferedVideo->SetTeid(imsi+1);
      manager->AddSvelteClientApp(appBufferedVideo);
      
      //http
      Ptr<SvelteClientApp> appHttp = httpHelper.Install(clientNode, serverNodes.Get(i),
        clientIpIfaces.GetAddress(i), serverIpIfaces.GetAddress(i), 10000+imsi+2);
      appHttp->SetEpsBearer(EpsBearer(EpsBearer::NGBR_VIDEO_TCP_PREMIUM,GbrQosInformation()));
      appHttp->SetTeid(imsi+2);
      manager->AddSvelteClientApp(appHttp);
//udp
      //autoPilot
      Ptr<SvelteClientApp> appAutoPilot = autoPilotHelper.Install(clientNode, serverNodes.Get(i),
        clientIpIfaces.GetAddress(i), serverIpIfaces.GetAddress(i), 10000+imsi+3);
      appAutoPilot->SetEpsBearer(EpsBearer(EpsBearer::GBR_GAMING,GbrQosInformation()));
      appAutoPilot->SetTeid(imsi+3);
      manager->AddSvelteClientApp(appAutoPilot);
      
      //liveVideo //FIXME TraceFilename
      Ptr<SvelteClientApp> appLiveVideo = liveVideoHelper.Install(clientNode, serverNodes.Get(i),
        clientIpIfaces.GetAddress(i), serverIpIfaces.GetAddress(i), 10000+imsi+4);
      appLiveVideo->SetEpsBearer(EpsBearer(EpsBearer::GBR_NON_CONV_VIDEO,GbrQosInformation()));
      appLiveVideo->SetTeid(imsi+4);
      manager->AddSvelteClientApp(appLiveVideo);
      
      //voip
      Ptr<SvelteClientApp> appVoip = voipHelper.Install(clientNode, serverNodes.Get(i),
        clientIpIfaces.GetAddress(i), serverIpIfaces.GetAddress(i), 10000+imsi+5);
      appVoip->SetEpsBearer(EpsBearer(EpsBearer::GBR_CONV_VOICE,GbrQosInformation()));
      appVoip->SetTeid(imsi+5);
      manager->AddSvelteClientApp(appVoip);

}

Ptr<TrafficStatsCalculator> stats = CreateObject<TrafficStatsCalculator>();

/*
  ApplicationContainer pingApps;

  for (int i = 0; i < numHosts; i++)
    {
      // Configure ping application between hosts
      V4PingHelper pingHelper = V4PingHelper (serverIpIfaces.GetAddress (i));
      pingHelper.SetAttribute ("Verbose", BooleanValue (true));
      pingApps.Add (pingHelper.Install (clientNodes.Get (i)));
    }
  pingApps.Start (Seconds (1));
*/

  // Enable datapath stats and pcap traces at hosts, switch(es), and controller(s)
  of13Helper->EnableDatapathStats ("switch-stats");
  if (trace)
    {
      of13Helper->EnableOpenFlowPcap ("openflow");
      
      csmaHelper.EnablePcap ("clients", clientDevices);
      csmaHelper.EnablePcap ("servers", serverDevices);
      csmaHelper.EnablePcap ("switchUl", NodeContainer (switchNodeUl));
      csmaHelper.EnablePcap ("switchDl", NodeContainer (switchNodeDl));
      csmaHelper.EnablePcap ("switchHw", NodeContainer (switchNodeHw));
      csmaHelper.EnablePcap ("switchSw", NodeContainer (switchNodeSw));
      ofs::EnableLibraryLog (true,"log");

    }
  

  ArpCache::PopulateArpCaches ();
  // Run the simulation

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
}

