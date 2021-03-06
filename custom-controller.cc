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
#include "applications/svelte-client.h"
#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <ns3/ofswitch13-module.h>
#include <set>

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
                   "Switch overloaded block policy (true for block).",
                   BooleanValue (true),
                   MakeBooleanAccessor (&CustomController::m_blockPol),
                   MakeBooleanChecker ())
    .AddAttribute ("SmartRouting",
                   "True for QoS routing, false for IP routing.",
                   BooleanValue (true),
                   MakeBooleanAccessor (&CustomController::m_qosRoute),
                   MakeBooleanChecker ())
    .AddAttribute ("Timeout",
                   "Controller timeout interval.",
                   TimeValue (Seconds (15)),
                   MakeTimeAccessor (&CustomController::m_timeout),
                   MakeTimeChecker ())

    .AddTraceSource ("Request", "The request trace source.",
                     MakeTraceSourceAccessor (&CustomController::m_requestTrace),
                     "ns3::CustomController::RequestTracedCallback")
    .AddTraceSource ("Release", "The release trace source.",
                     MakeTraceSourceAccessor (&CustomController::m_releaseTrace),
                     "ns3::CustomController::ReleaseTracedCallback")
  ;
  return tid;
}

bool
CustomController::DedicatedBearerRequest (Ptr<SvelteClient> app, uint64_t imsi)
{
  NS_LOG_FUNCTION (this << app << imsi);

  // Salvando o endereço IP do cliente para este TEID.
  uint32_t teid = app->GetTeid ();
  Ptr<Ipv4> ipv4 = app->GetNode ()->GetObject<Ipv4>();
  Ipv4Address ipv4addr = ipv4->GetAddress (1,0).GetLocal ();
  m_teidAddr [teid] = ipv4addr;

  // Definindo o switch (HW/SW) que irá receber este tráfego.
  Ptr<OFSwitch13Device> switchDevice;
  if (m_qosRoute)
    {
      // Para o roteamento por QoS, o switch padrão é o SW.
      switchDevice = switchDeviceSw;
    }
  else
    {
      // Para o roteamento por IP, o switch SW atende tráfegos de IP ímpar e o
      // switch HW atende tráfegos de IP par.
      bool ipImpar = ipv4addr.Get () & 1;
      switchDevice = ipImpar ? switchDeviceSw : switchDeviceHw;
    }

  // Verifica os recursos disponíveis no switch (processamento e uso de tabela)
  double tabUse = switchDevice->GetFlowTableUsage (0);
  double cpuUse = switchDevice->GetCpuUsage ();

  // Bloquear o tráfego se a tabela exceder o limite de bloqueio.
  if (tabUse > m_blockThs)
    {
      m_requestTrace (teid, false);
      return false;
    }

  // Bloquear o tráfego se o uso de cpu exceder o limite de bloqueio e a
  // política de bloqueio por excesso de carga estiver ativa.
  if (cpuUse > m_blockThs && m_blockPol)
    {
      m_requestTrace (teid, false);
      return false;
    }

  // Instalar as regras para este tráfego.
  InstallTrafficRules (switchDevice, teid);
  m_requestTrace (teid, true);
  return true;
}

bool
CustomController::DedicatedBearerRelease (Ptr<SvelteClient> app, uint64_t imsi)
{
  NS_LOG_FUNCTION (this << app << imsi);

  // Removendo potenciais regras de todos os switches.
  uint32_t teid = app->GetTeid ();
  RemoveTrafficRules (switchDeviceSw, teid);
  RemoveTrafficRules (switchDeviceHw, teid);
  RemoveTrafficRules (switchDeviceUl, teid);
  RemoveTrafficRules (switchDeviceDl, teid);

  m_releaseTrace (teid);
  return true;
}


void
CustomController::NotifyHwSwitch (Ptr<OFSwitch13Device> switchDevice,
                                  uint32_t ulPort, uint32_t dlPort)
{
  NS_LOG_FUNCTION (this << switchDevice << ulPort << dlPort);

  // Salvando switch e número de portas.
  switchDeviceHw = switchDevice;
  hw2dlPort = dlPort;
  hw2ulPort = ulPort;

  // Neste switch estamos configurando dois grupos:
  // Grupo 1, usado para enviar pacotes na direção de uplink.
  // Grupo 2, usado para enviar pacotes na direção de downlink.
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

void
CustomController::NotifySwSwitch (Ptr<OFSwitch13Device> switchDevice,
                                  uint32_t ulPort, uint32_t dlPort)
{
  NS_LOG_FUNCTION (this << switchDevice << ulPort << dlPort);

  // Salvando switch e número de portas.
  switchDeviceSw = switchDevice;
  sw2dlPort = dlPort;
  sw2ulPort = ulPort;

  // Neste switch estamos configurando dois grupos:
  // Grupo 1, usado para enviar pacotes na direção de uplink.
  // Grupo 2, usado para enviar pacotes na direção de downlink.
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

void
CustomController::NotifyUlSwitch (Ptr<OFSwitch13Device> switchDevice,
                                  uint32_t hwPort, uint32_t swPort)
{
  NS_LOG_FUNCTION (this << switchDevice << hwPort << swPort);

  // Salvando switch e número de portas.
  switchDeviceUl = switchDevice;
  ul2hwPort = hwPort;
  ul2swPort = swPort;

  // O switch UL tem 3 tabelas:
  //
  // Tabela 0: Identifica se o pacote é na direção uplink (para o servidor) ou
  // downlink (para o cliente). Esta tabela direciona o pacote para uma das
  // próximas tabelas do pipeline, de acordo com o destino.
  //
  // Para identificar a direção do tráfego (uplink ou downlink), vamos usar a
  // informação da porta de entrada. Se for de uma das duas portas vindas como
  // parâmetro nesta função então é tráfego de downlink, senão é tráfego de
  // downlink. Usamos prioridade maior para as portas específicas de downlink,
  // e deixamos o uplink com prioridade menor.
  std::ostringstream cmdDl1, cmdDl2;
  cmdDl1 << "flow-mod cmd=add,prio=64,table=0"
         << " eth_type=0x800,in_port=" << hwPort
         << " goto:2";

  cmdDl2 << "flow-mod cmd=add,prio=64,table=0"
         << " eth_type=0x800,in_port=" << swPort
         << " goto:2";

  std::ostringstream cmdUl;
  cmdUl << "flow-mod cmd=add,prio=32,table=0"
        << " eth_type=0x800 goto:1";

  DpctlSchedule (switchDeviceUl->GetDatapathId (), cmdDl1.str ());
  DpctlSchedule (switchDeviceUl->GetDatapathId (), cmdDl2.str ());
  DpctlSchedule (switchDeviceUl->GetDatapathId (), cmdUl.str ());

  // Tabela 1: Faz o mapeamento de portas para o tráfego de uplink, decidindo
  // por encaminhar o pacote para o switch HW ou SW. Nesta tabela que este
  // controlador pode implementar diferentes políticas de roteamento.
  //
  // As regras serão instaladas aqui na função ConfigureBy* ().

  // Tabela 2: Faz o mapemamento de portas para o tráfego de downlink,
  // decidindo por encaminhar o pacote para a porta correta de acordo
  // com o IP do cliente.
  //
  // As regras serão instaladas aqui na função NotifyUl2Cl ()
}

void
CustomController::NotifyDlSwitch (Ptr<OFSwitch13Device> switchDevice,
                                  uint32_t hwPort, uint32_t swPort)
{
  NS_LOG_FUNCTION (this << switchDevice << hwPort << swPort);

  // Salvando switch e número de portas.
  switchDeviceDl = switchDevice;
  dl2hwPort = hwPort;
  dl2swPort = swPort;

  // O switch UL tem 3 tabelas:
  //
  // Tabela 0: Identifica se o pacote é na direção uplink (para o servidor) ou
  // downlink (para o cliente). Esta tabela direciona o pacote para uma das
  // próximas tabelas do pipeline, de acordo com o destino.
  //
  // Para identificar a direção do tráfego (uplink ou downlink), vamos usar a
  // informação da porta de entrada. Se for de uma das duas portas vindas como
  // parâmetro nesta função então é tráfego de uplink, senão é tráfego de
  // downlink. Usamos prioridade maior para as portas específicas de uplink, e
  // deixamos o downlink com prioridade menor.
  std::ostringstream cmdUl1, cmdUl2;
  cmdUl1 << "flow-mod cmd=add,prio=64,table=0"
         << " eth_type=0x800,in_port=" << hwPort
         << " goto:2";

  cmdUl2 << "flow-mod cmd=add,prio=64,table=0"
         << " eth_type=0x800,in_port=" << swPort
         << " goto:2";

  std::ostringstream cmdDl;
  cmdDl << "flow-mod cmd=add,prio=32,table=0"
        << " eth_type=0x800 goto:1";

  DpctlSchedule (switchDeviceDl->GetDatapathId (), cmdUl1.str ());
  DpctlSchedule (switchDeviceDl->GetDatapathId (), cmdUl2.str ());
  DpctlSchedule (switchDeviceDl->GetDatapathId (), cmdDl.str ());

  // Tabela 1: Faz o mapeamento de portas para o tráfego de downlink, decidindo
  // por encaminhar o pacote para o switch HW ou SW. Nesta tabela que este
  // controlador pode implementar diferentes políticas de roteamento.
  //
  // As regras serão instaladas aqui na função ConfigureBy* ().

  // Tabela 2: Faz o mapemamento de portas para o tráfego de uplink,
  // decidindo por encaminhar o pacote para a porta correta de acordo
  // com o IP do servidor.
  //
  // As regras serão instaladas aqui na função NotifyDl2Sv ()
}

void
CustomController::NotifyDl2Sv (uint32_t portNo, Ipv4Address ipAddr)
{
  NS_LOG_FUNCTION (this << portNo << ipAddr);

  // Inserindo na tabela 2 a regra que mapeia IP de destino na porta de saída.
  std::ostringstream cmd;
  cmd << "flow-mod cmd=add,prio=64,table=2"
      << " eth_type=0x800,ip_dst=" << ipAddr
      << " apply:output=" << portNo;
  DpctlSchedule (switchDeviceDl->GetDatapathId (), cmd.str ());
}

void
CustomController::NotifyUl2Cl (uint32_t portNo, Ipv4Address ipAddr)
{
  NS_LOG_FUNCTION (this << portNo << ipAddr);

  // Inserindo na tabela 2 a regra que mapeia IP de destino na porta de saída.
  std::ostringstream cmd;
  cmd << "flow-mod cmd=add,prio=64,table=2"
      << " eth_type=0x800,ip_dst=" << ipAddr
      << " apply:output=" << portNo;
  DpctlSchedule (switchDeviceUl->GetDatapathId (), cmd.str ());
}

void
CustomController::NotifyTopologyBuilt ()
{
  NS_LOG_FUNCTION (this);

  // Configura as regras nos switches de acordo com a política de roteamento.
  if (m_qosRoute)
    {
      ConfigureByQos ();
    }
  else
    {
      ConfigureByIp ();
    }
}

void
CustomController::DoDispose ()
{
  NS_LOG_FUNCTION (this);

  switchDeviceUl = 0;
  switchDeviceDl = 0;
  switchDeviceHw = 0;
  switchDeviceSw = 0;
  m_teidAddr.clear ();
  OFSwitch13Controller::DoDispose ();
}

void
CustomController::NotifyConstructionCompleted (void)
{
  NS_LOG_FUNCTION (this);

  // Escalona a primeira operação de timeout para o controlador.
  Simulator::Schedule (m_timeout, &CustomController::ControllerTimeout, this);

  OFSwitch13Controller::NotifyConstructionCompleted ();
}

void
CustomController::HandshakeSuccessful (Ptr<const RemoteSwitch> swtch)
{
  NS_LOG_FUNCTION (this << swtch);

  OFSwitch13Controller::HandshakeSuccessful (swtch);
}

void
CustomController::ConfigureByIp ()
{
  NS_LOG_FUNCTION (this);

  // Vamos instalar as regras de roteamento interno com base no IP do usuário:
  // IPs pares são enviados para o switch HW e IPs ímpares são enviados para o
  // switch SW. Observe as regras sempre na tabela 1.

  // Pacotes originados nos clientes, que estão entrando através do switch UL.
  {
    std::ostringstream cmdHw, cmdSw;
    cmdHw << "flow-mod cmd=add,prio=64,table=1"
          << " eth_type=0x800,ip_src=0.0.0.0/0.0.0.1"
          << " apply:output=" << ul2hwPort;

    cmdSw << "flow-mod cmd=add,prio=64,table=1"
          << " eth_type=0x800,ip_src=0.0.0.1/0.0.0.1"
          << " apply:output=" << ul2swPort;

    DpctlSchedule (switchDeviceUl->GetDatapathId (), cmdHw.str ());
    DpctlSchedule (switchDeviceUl->GetDatapathId (), cmdSw.str ());
  }

  // Pacotes originados no servidor, que estão entrando através do switch DL.
  {
    std::ostringstream cmdHw, cmdSw;
    cmdHw << "flow-mod cmd=add,prio=64,table=1"
          << " eth_type=0x800,ip_dst=0.0.0.0/0.0.0.1"
          << " apply:output=" << dl2hwPort;

    cmdSw << "flow-mod cmd=add,prio=64,table=1"
          << " eth_type=0x800,ip_dst=0.0.0.1/0.0.0.1"
          << " apply:output=" << dl2swPort;

    DpctlSchedule (switchDeviceDl->GetDatapathId (), cmdHw.str ());
    DpctlSchedule (switchDeviceDl->GetDatapathId (), cmdSw.str ());
  }
}

void
CustomController::ConfigureByQos ()
{
  NS_LOG_FUNCTION (this);

  // Vamos instalar as regras de roteamento interno sempre no switch de SW.

  // Pacotes originados nos clientes, que estão entrando através do switch UL.
  {
    std::ostringstream cmdSw;
    cmdSw << "flow-mod cmd=add,prio=64,table=1"
          << " eth_type=0x800"
          << " apply:output=" << ul2swPort;

    DpctlSchedule (switchDeviceUl->GetDatapathId (), cmdSw.str ());
  }

  // Pacotes originados no servidor, que estão entrando através do switch DL.
  {
    std::ostringstream cmdSw;
    cmdSw << "flow-mod cmd=add,prio=64,table=1"
          << " eth_type=0x800"
          << " apply:output=" << dl2swPort;

    DpctlSchedule (switchDeviceDl->GetDatapathId (), cmdSw.str ());
  }
}

void
CustomController::InstallTrafficRules (Ptr<OFSwitch13Device> switchDevice,
                                       uint32_t teid)
{
  NS_LOG_FUNCTION (this << switchDevice << teid);

  // Recuperando o IP do cliente.
  Ipv4Address ipv4addr = m_teidAddr [teid];

  // Estamos considerando os valores manualmente adicionados ao TEID para
  // identificar a aplicação. As 4 primeiras são TCP, e as demais UDP.
  bool tcpApp = (teid & 0xF) <= 3;
  uint16_t port = teid + 10000;

  // Instalar as regras identificando o trafego pelo teid no cookie.
  std::ostringstream cmdUl, cmdDl;
  cmdUl << "flow-mod cmd=add,prio=64,table=0,cookie=" << GetUint32Hex (teid)
        << " eth_type=0x800,ip_src=" << ipv4addr;
  cmdDl << "flow-mod cmd=add,prio=64,table=0,cookie=" << GetUint32Hex (teid)
        << " eth_type=0x800,ip_dst=" << ipv4addr;

  if (tcpApp)
    {
      // Regras específicas para protocolo TCP.
      cmdUl << ",ip_proto=6,tcp_dst=" << port;
      cmdDl << ",ip_proto=6,tcp_src=" << port;
    }
  else
    {
      // Regras específicas para protocolo UDP.
      cmdUl << ",ip_proto=17,udp_dst=" << port;
      cmdDl << ",ip_proto=17,udp_src=" << port;
    }

  cmdUl << " write:group=1";
  cmdDl << " write:group=2";

  DpctlExecute (switchDevice->GetDatapathId (), cmdUl.str ());
  DpctlExecute (switchDevice->GetDatapathId (), cmdDl.str ());
}

void
CustomController::RemoveTrafficRules (Ptr<OFSwitch13Device> switchDevice,
                                      uint32_t teid)
{
  NS_LOG_FUNCTION (this << switchDevice << teid);

  // Usar o teid como identificador da regra pelo campo cookie.
  std::ostringstream cmd;
  cmd << "flow-mod cmd=del,cookie=" << GetUint32Hex (teid)
      << ",cookie_mask=0xFFFFFFFFFFFFFFFF";

  DpctlExecute (switchDevice->GetDatapathId (), cmd.str ());
}

void
CustomController::MoveTrafficRules (Ptr<OFSwitch13Device> srcSwitchDevice,
                                    Ptr<OFSwitch13Device> dstSwitchDevice,
                                    uint32_t teid)
{
  NS_LOG_FUNCTION (this << srcSwitchDevice << dstSwitchDevice << teid);

  // Instala regras no switch de destino e escalona remoção no switch de origem.
  InstallTrafficRules (dstSwitchDevice, teid);
  Simulator::Schedule (MilliSeconds (500), &CustomController::UpdateDlUlRules,
                       this, teid);
  Simulator::Schedule (Seconds (1), &CustomController::RemoveTrafficRules,
                       this, srcSwitchDevice, teid);
}

void
CustomController::UpdateDlUlRules (uint32_t teid)
{
  NS_LOG_FUNCTION (this << teid);

  // Instalar regras com maior prioridade nos switches UL e DL.

  // Recuperando o IP do cliente.
  Ipv4Address ipv4addr = m_teidAddr [teid];

  // Estamos considerando os valores manualmente adicionados ao TEID para
  // identificar a aplicação. As 4 primeiras são TCP, e as demais UDP.
  bool tcpApp = (teid & 0xF) <= 3;
  uint16_t port = teid + 10000;

  // Instalar as regras identificando o trafego pelo teid no cookie.
  std::ostringstream cmdUl, cmdDl;
  cmdUl << "flow-mod cmd=add,prio=128,table=1,cookie=" << GetUint32Hex (teid)
        << " eth_type=0x800,ip_src=" << ipv4addr;
  cmdDl << "flow-mod cmd=add,prio=128,table=1,cookie=" << GetUint32Hex (teid)
        << " eth_type=0x800,ip_dst=" << ipv4addr;

  if (tcpApp)
    {
      // Regras específicas para protocolo TCP.
      cmdUl << ",ip_proto=6,tcp_dst=" << port;
      cmdDl << ",ip_proto=6,tcp_src=" << port;
    }
  else
    {
      // Regras específicas para protocolo UDP.
      cmdUl << ",ip_proto=17,udp_dst=" << port;
      cmdDl << ",ip_proto=17,udp_src=" << port;
    }

  cmdUl << " apply:output=" << ul2hwPort;
  cmdDl << " apply:output=" << dl2hwPort;

  DpctlExecute (switchDeviceUl->GetDatapathId (), cmdUl.str ());
  DpctlExecute (switchDeviceDl->GetDatapathId (), cmdDl.str ());
}

// Declarando tipo de par TEID / vazão.
typedef std::pair<uint32_t, DataRate> TeidThp_t;

// Declarando tipo de função que recever dois pares TeidThp_t e retorna bool.
typedef std::function<bool (TeidThp_t, TeidThp_t)> TeidThpComp_t;

// Comparador para ordenar o mapa de vazão por tráfego.
TeidThpComp_t thpComp = [] (TeidThp_t elem1, TeidThp_t elem2)
  {
    return elem1.second > elem2.second;
  };

void
CustomController::ControllerTimeout ()
{
  NS_LOG_FUNCTION (this);

  // Escalona a próxima operação de timeout para o controlador.
  Simulator::Schedule (m_timeout, &CustomController::ControllerTimeout, this );

  // Para o roteamento por IP não há nada a ser feito aqui.
  if (!m_qosRoute)
    {
      return;
    }

  // O roteamento é por QoS. Vamos percorrer a tabela do switch SW e montar uma
  // lista ordenada dos tráfegos com vazão decrescente para que possamos mover
  // os tráfegos de maior vazão para o switch de HW sem extrapolar sua
  // capacidade máxima.

  struct datapath *datapath = switchDeviceSw->GetDatapathStruct ();
  struct flow_table *table = datapath->pipeline->tables[0];
  struct flow_entry *entry;

  std::map<uint32_t, DataRate> thpByTeid;
  bool firstRule = true;
  double bytes;

  // Percorrendo tabela e recuperando informações sobre os tráfegos.
  LIST_FOR_EACH (entry, struct flow_entry, match_node, &table->match_entries)
  {
    struct ofl_flow_stats *stats = entry->stats;
    Time active = Simulator::Now () - MilliSeconds (entry->created);

    // Temos sempre duas regras para cada tráfego (uplink e downlink).
    if (firstRule)
      {
        bytes = entry->stats->byte_count;
        firstRule = false;
        continue;
      }
    bytes += stats->byte_count;
    firstRule = true;

    // Calculando a vazão total para o tráfego.
    DataRate throughput (bytes * 8 / active.GetSeconds ());
    thpByTeid [entry->stats->cookie] = throughput;
    NS_LOG_DEBUG ("Traffic " << entry->stats->cookie <<
                  " with throughput " << throughput);
  }

  // Construindo um set com as vazões ordenadas em descrescente.
  std::set<TeidThp_t, TeidThpComp_t> thpSorted (
    thpByTeid.begin (), thpByTeid.end (), thpComp);

  // Verificando os recursos disponíveis no switch de HW:
  uint32_t tabHwFree =
    switchDeviceHw->GetFlowTableSize (0) * m_blockThs -
    switchDeviceHw->GetFlowTableEntries (0);
  uint64_t bpsHwFree =
    switchDeviceHw->GetCpuCapacity ().GetBitRate () * m_blockThs -
    switchDeviceHw->GetCpuLoad ().GetBitRate ();
  NS_LOG_DEBUG ("Resources on HW switch: " << tabHwFree <<
                " table entries and " << bpsHwFree << " CPU bps free.");

  // Percore a lista de tráfego movendo os primeiros para o switch de HW.
  for (auto element : thpSorted)
    {
      if (tabHwFree < 2 || bpsHwFree < element.second.GetBitRate ())
        {
          // Parar se não houver mais recursos disponíveis no HW.
          break;
        }

      // Move o tráfego do switch de SW para o switch de HW.
      NS_LOG_DEBUG ("Moving traffic " << element.first << " to HW switch.");
      MoveTrafficRules (switchDeviceSw, switchDeviceHw, element.first);
      tabHwFree -= 2;
      bpsHwFree -= element.second.GetBitRate ();
    }
}

} // namespace ns3
