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

#pragma once
#include <ns3/ofswitch13-module.h>
#include <ns3/internet-module.h>
#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/lte-module.h>
#include "applications/svelte-client-app.h"

namespace ns3 {

class CustomController : public OFSwitch13Controller
{
public:
  CustomController ();
  virtual ~CustomController ();
  static TypeId GetTypeId (void);
  void NotifyHwSwitch (Ptr<OFSwitch13Device> switchDevice, uint32_t ulPort, uint32_t dlPort);
  void NotifySwSwitch (Ptr<OFSwitch13Device> switchDevice, uint32_t ulPort, uint32_t dlPort);
  void NotifyUlSwitch (Ptr<OFSwitch13Device> switchDevice, uint32_t hwPort, uint32_t swPort, uint32_t clPort);
  void NotifyDlSwitch (Ptr<OFSwitch13Device> switchDevice, uint32_t hwPort, uint32_t swPort, uint32_t svPort);
  uint64_t GetSwitchId(uint32_t teid);
/**
   * Request a new dedicated EPS bearer. This is used to check for necessary
   * resources in the network (mainly available data rate for GBR bearers).
   * When returning false, it aborts the bearer creation process.
   * \internal Current implementation assumes that each application traffic
   *           flow is associated with a unique bearer/tunnel. Because of that,
   *           we can use only the teid for the tunnel to prepare and install
   *           route. If we would like to aggregate traffic from several
   *           applications into same bearer we will need to revise this.
   * \param teid The teid for this bearer, if already defined.
   * \param imsi uint64_t IMSI UE identifier.
   * \param bearer EpsBearer bearer QoS characteristics of the bearer.
   * \returns True if succeeded (the bearer creation process will proceed),
   *          false otherwise (the bearer creation process will abort).
   */
  virtual bool DedicatedBearerRequest (Ptr<SvelteClientApp> app, uint64_t imsi);

  /**
   * Release a dedicated EPS bearer.
   * \internal Current implementation assumes that each application traffic
   *           flow is associated with a unique bearer/tunnel. Because of that,
   *           we can use only the teid for the tunnel to prepare and install
   *           route. If we would like to aggregate traffic from several
   *           applications into same bearer we will need to revise this.
   * \param teid The teid for this bearer, if already defined.
   * \param imsi uint64_t IMSI UE identifier.
   * \param bearer EpsBearer bearer QoS characteristics of the bearer.
   * \return True if succeeded, false otherwise.
   */
  virtual bool DedicatedBearerRelease (Ptr<SvelteClientApp> app, uint64_t imsi);

protected:
  virtual void DoDispose ();
  virtual void NotifyConstructionCompleted (void);
  virtual void HandshakeSuccessful (Ptr<const RemoteSwitch> swtch);

private:
  Ptr<OFSwitch13Device> switchDeviceUl;
  Ptr<OFSwitch13Device> switchDeviceDl;
  Ptr<OFSwitch13Device> switchDeviceHw;
  Ptr<OFSwitch13Device> switchDeviceSw;
  uint32_t hw2ulPort;
  uint32_t ul2hwPort;
  uint32_t sw2ulPort;
  uint32_t ul2swPort;
  uint32_t hw2dlPort;
  uint32_t dl2hwPort;
  uint32_t sw2dlPort;
  uint32_t dl2swPort;
  uint32_t ul2clPort;
  uint32_t dl2svPort;

};

}//namespace ns3