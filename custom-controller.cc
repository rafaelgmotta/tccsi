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

#include "custom-controller.h"
#include <iomanip>
#include <iostream>
#define COOKIE_STRICT_MASK_STR  "0xFFFFFFFFFFFFFFFF"
using namespace std;
namespace ns3 {
NS_LOG_COMPONENT_DEFINE ("CustomController");
NS_OBJECT_ENSURE_REGISTERED (CustomController);

CustomController::CustomController ()
{
  NS_LOG_FUNCTION (this);
}

CustomController::~CustomController ()
{
  NS_LOG_FUNCTION (this);
}

void
CustomController::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  switchDeviceUl = 0;
  switchDeviceDl = 0;
  switchDeviceHw = 0;
  switchDeviceSw = 0;
  OFSwitch13Controller::DoDispose ();
}

TypeId
CustomController::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::CustomController")
    .SetParent<OFSwitch13Controller> ()
    .AddConstructor<CustomController> ()
    .AddAttribute ("BlockThs",
                   "Switch overloaded block threshold.",
                   DoubleValue (0.9),
                   MakeDoubleAccessor (&CustomController::m_blockThs),
                   MakeDoubleChecker<double> (0.8, 1.0))
    .AddAttribute ("BlockPolicy",
                   "Switch overloaded block policy. (true aceita, false bloqueia)",
                   BooleanValue (true),
                   MakeBooleanAccessor (&CustomController::m_blockPol),
                   MakeBooleanChecker ())
    .AddTraceSource ("BearerRequest", "The bearer request trace source.",
                     MakeTraceSourceAccessor (
                       &CustomController::m_bearerRequestTrace),
                     "ns3::CustomController::RequestTracedCallback")
  ;
  return tid;
}

void
CustomController::NotifyConstructionCompleted (void)
{
  NS_LOG_FUNCTION (this);
  OFSwitch13Controller::NotifyConstructionCompleted ();
}

void
CustomController::HandshakeSuccessful (Ptr<const RemoteSwitch> swtch)
{
  NS_LOG_FUNCTION (this << swtch);

}
void CustomController::NotifyHwSwitch (Ptr<OFSwitch13Device> switchDevice, uint32_t ulPort, uint32_t dlPort)
{
  NS_LOG_FUNCTION (this << switchDevice << ulPort << dlPort );

  switchDeviceHw = switchDevice;
  hw2ulPort = ulPort;
  hw2dlPort = dlPort;

  std::ostringstream cmd1, cmd2;

  cmd1 << "group-mod cmd=add,type=ind,group=1"
      << " weight=0,port=any,group=any"
      << " output=" << hw2dlPort;

  cmd2 << "group-mod cmd=add,type=ind,group=2"
      << " weight=0,port=any,group=any"
      << " output=" << hw2ulPort;

  DpctlSchedule (switchDeviceHw->GetDatapathId (), cmd1.str ());
  DpctlSchedule (switchDeviceHw->GetDatapathId (), cmd2.str ());

}

void CustomController::NotifySwSwitch (Ptr<OFSwitch13Device> switchDevice, uint32_t ulPort, uint32_t dlPort)
{
  NS_LOG_FUNCTION (this << switchDevice << ulPort << dlPort );

  switchDeviceSw = switchDevice;
  sw2ulPort = ulPort;
  sw2dlPort = dlPort;

  std::ostringstream cmd1, cmd2;

  cmd1 << "group-mod cmd=add,type=ind,group=1"
      << " weight=0,port=any,group=any"
      << " output=" << sw2dlPort;
      
  cmd2 << "group-mod cmd=add,type=ind,group=2"
      << " weight=0,port=any,group=any"
      << " output=" << sw2ulPort;

  DpctlSchedule (switchDeviceSw->GetDatapathId (), cmd1.str ());
  DpctlSchedule (switchDeviceSw->GetDatapathId (), cmd2.str ());

}

void CustomController::NotifyUlSwitch (Ptr<OFSwitch13Device> switchDevice, uint32_t hwPort, uint32_t swPort, uint32_t clPort)
{
  NS_LOG_FUNCTION (this << switchDevice << hwPort << swPort << clPort );

  switchDeviceUl = switchDevice;
  ul2hwPort = hwPort;
  ul2swPort = swPort;
  ul2clPort = clPort;

  std::ostringstream cmdClHw, cmdClSw, cmdHwCl, cmdSwCl;

  cmdClHw << "flow-mod cmd=add,prio=64,table=0"
          << " eth_type=0x800,in_port=" << ul2clPort
          << ",ip_src=0.0.0.0/0.0.0.1"
          << " apply:output=" << ul2hwPort;

  cmdClSw << "flow-mod cmd=add,prio=64,table=0"
          << " eth_type=0x800,in_port=" << ul2clPort
          << ",ip_src=0.0.0.1/0.0.0.1"
          << " apply:output=" << ul2swPort;

  cmdHwCl << "flow-mod cmd=add,prio=64,table=0"
          << " eth_type=0x800,in_port=" << ul2hwPort
          << " apply:output=" << ul2clPort;

  cmdSwCl << "flow-mod cmd=add,prio=64,table=0"
          << " eth_type=0x800,in_port=" << ul2swPort
          << " apply:output=" << ul2clPort;


  DpctlSchedule (switchDeviceUl->GetDatapathId (),cmdClHw.str ());
  DpctlSchedule (switchDeviceUl->GetDatapathId (),cmdClSw.str ());
  DpctlSchedule (switchDeviceUl->GetDatapathId (),cmdHwCl.str ());
  DpctlSchedule (switchDeviceUl->GetDatapathId (),cmdSwCl.str ());

}
void CustomController::NotifyDlSwitch (Ptr<OFSwitch13Device> switchDevice, uint32_t hwPort, uint32_t swPort, uint32_t svPort)
{
  NS_LOG_FUNCTION (this << switchDevice << hwPort << swPort << svPort );

  switchDeviceDl = switchDevice;
  dl2hwPort = hwPort;
  dl2swPort = swPort;
  dl2svPort = svPort;


  std::ostringstream cmdSvHw, cmdSvSw, cmdHwSv, cmdSwSv;

  cmdSvHw << "flow-mod cmd=add,prio=64,table=0"
          << " eth_type=0x800,in_port=" << dl2svPort
          << ",ip_dst=0.0.0.0/0.0.0.1"
          << " apply:output=" << dl2hwPort;

  cmdSvSw << "flow-mod cmd=add,prio=64,table=0"
          << " eth_type=0x800,in_port=" << dl2svPort
          << ",ip_dst=0.0.0.1/0.0.0.1"
          << " apply:output=" << dl2swPort;

  cmdHwSv << "flow-mod cmd=add,prio=64,table=0"
          << " eth_type=0x800,in_port=" << dl2hwPort
          << " apply:output=" << dl2svPort;

  cmdSwSv << "flow-mod cmd=add,prio=64,table=0"
          << " eth_type=0x800,in_port=" << dl2swPort
          << " apply:output=" << dl2svPort;

  DpctlSchedule (switchDeviceDl->GetDatapathId (),cmdSvHw.str ());
  DpctlSchedule (switchDeviceDl->GetDatapathId (),cmdSvSw.str ());
  DpctlSchedule (switchDeviceDl->GetDatapathId (),cmdHwSv.str ());
  DpctlSchedule (switchDeviceDl->GetDatapathId (),cmdSwSv.str ());
}
bool CustomController::DedicatedBearerRequest  (Ptr<SvelteClientApp> app, uint64_t imsi)
{
  Ptr<Node> node = app->GetNode();
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
  Ipv4Address ipv4addr = ipv4->GetAddress(1,0).GetLocal();
  

  uint32_t bit = ipv4addr.Get()&1;
  
  Ptr<OFSwitch13Device> switchDevice;

  uint16_t port = app->GetTeid()+10000;
  
  bool tcp = (app->GetTeid() & 0xF) <= 2;

  if(!bit)
    switchDevice = switchDeviceHw;
  else
    switchDevice = switchDeviceSw;

  uint32_t teid = app->GetTeid ();
  char teidStr[11];
  sprintf(teidStr,"0x%08x",teid);

  //consultar switch e verificar disponibilidade de processamento e espaco

  //switchDevice->GetFlowTableUsage (tableId);
  //acima do espaco: m_bearerRequestTrace(teid, false);
                    //return false;
  //acima do processamento: verifica policy


  double usage = switchDevice->GetFlowTableUsage (0);

  if(usage > usageLimit){
    m_bearerRequestTrace(teid, false);
    return false;
  }

  double processing = switchDevice->GetProcessingUsage();

  if(processing > processingLimit){
    m_bearerRequestTrace(teid, m_blockPol);
    return m_blockPol;
  }


  //app tcps nao passam, verificar regras

  std::ostringstream cmdUl, cmdDl;

  cmdUl << "flow-mod cmd=add,prio=64,table=0,cookie=" << teidStr
        << " eth_type=0x800,ip_src=" << ipv4addr;
  cmdDl << "flow-mod cmd=add,prio=64,table=0,cookie=" << teidStr
        << " eth_type=0x800,ip_dst=" << ipv4addr;
  if (tcp){
    cmdUl << ",ip_proto=6,tcp_dst=" << port;
    cmdDl << ",ip_proto=6,tcp_src=" << port;
  }
  else{
    cmdUl << ",ip_proto=17,udp_dst=" << port;
    cmdDl << ",ip_proto=17,udp_src=" << port;
  }

  cmdUl << " write:group=1";
  cmdDl << " write:group=2";

  DpctlExecute (switchDevice->GetDatapathId (),cmdUl.str ());
  DpctlExecute (switchDevice->GetDatapathId (),cmdDl.str ());

  m_bearerRequestTrace(teid, true);
  return true;
}

bool CustomController::DedicatedBearerRelease (Ptr<SvelteClientApp> app, uint64_t imsi)
{
  uint32_t teid = app->GetTeid ();
  char teidStr[11];
  sprintf(teidStr,"0x%08x",teid);

  Ptr<Node> node = app->GetNode();
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
  Ipv4Address ipv4addr = ipv4->GetAddress(1,0).GetLocal();
  
  uint32_t bit = ipv4addr.Get()&1;

  Ptr<OFSwitch13Device> switchDevice;

  if(!bit)
    switchDevice = switchDeviceHw;
  else
    switchDevice = switchDeviceSw;

  std::ostringstream cmd;
  cmd << "flow-mod cmd=del"
      << ",cookie="       << teidStr
      << ",cookie_mask="  << COOKIE_STRICT_MASK_STR;
  DpctlExecute (switchDevice->GetDatapathId (), cmd.str ());

  return true;
}

uint64_t CustomController::GetSwitchId(uint32_t teid)
{

  return 0;
}

}//namespace ns3

