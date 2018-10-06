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
 * Author:  Luciano Chaves <luciano@lrc.ic.unicamp.br>
 */

#include "custom-controller.h"
#include <iomanip>
#include <iostream>
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
    // .AddAttribute ("EnableMeter",
    //                "Enable per-flow mettering.",
    //                BooleanValue (false),
    //                MakeBooleanAccessor (&CustomController::m_meterEnable),
    //                MakeBooleanChecker ())
    // .AddAttribute ("MeterRate",
    //                "Per-flow meter rate.",
    //                DataRateValue (DataRate ("256Kbps")),
    //                MakeDataRateAccessor (&CustomController::m_meterRate),
    //                MakeDataRateChecker ())
    // .AddAttribute ("LinkAggregation",
    //                "Enable link aggregation.",
    //                BooleanValue (true),
    //                MakeBooleanAccessor (&CustomController::m_linkAggregation),
    //                MakeBooleanChecker ())
    // .AddAttribute ("ServerIpAddr",
    //                "Server IPv4 address.",
    //                AddressValue (Address (Ipv4Address ("10.1.1.1"))),
    //                MakeAddressAccessor (&CustomController::m_serverIpAddress),
    //                MakeAddressChecker ())
    // .AddAttribute ("ServerTcpPort",
    //                "Server TCP port.",
    //                UintegerValue (9),
    //                MakeUintegerAccessor (&CustomController::m_serverTcpPort),
    //                MakeUintegerChecker<uint64_t> ())
    // .AddAttribute ("ServerMacAddr",
    //                "Server MAC address.",
    //                AddressValue (Address (Mac48Address ("00:00:00:00:00:01"))),
    //                MakeAddressAccessor (&CustomController::m_serverMacAddress),
    //                MakeAddressChecker ())
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

  cmd1 << "flow-mod cmd=add,prio=1,table=0"
       << " eth_type=0x800,in_port=" << hw2ulPort
       << " apply:output=" << hw2dlPort;

  cmd2 << "flow-mod cmd=add,prio=1,table=0"
       << " eth_type=0x800,in_port=" << hw2dlPort
       << " apply:output=" << hw2ulPort;

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

  cmd1 << "flow-mod cmd=add,prio=1,table=0"
       << " eth_type=0x800,in_port=" << sw2ulPort
       << " apply:output=" << sw2dlPort;

  cmd2 << "flow-mod cmd=add,prio=1,table=0"
       << " eth_type=0x800,in_port=" << sw2dlPort
       << " apply:output=" << sw2ulPort;

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
          << ",ip_src=0.0.0.0/0.0.0.1"
          << " apply:output=" << dl2hwPort;

  cmdSvSw << "flow-mod cmd=add,prio=64,table=0"
          << " eth_type=0x800,in_port=" << dl2svPort
          << ",ip_src=0.0.0.1/0.0.0.1"
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
 bool CustomController::DedicatedBearerRequest (EpsBearer bearer, uint64_t imsi,
                                       uint32_t teid)
 {
return false;
 }

 bool CustomController::DedicatedBearerRelease (EpsBearer bearer, uint64_t imsi,
                                       uint32_t teid)
 {
return true;
 }

uint64_t CustomController::GetSwitchId(uint32_t teid)
{

}

}//namespace ns3

