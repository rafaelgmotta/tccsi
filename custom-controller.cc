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

  // Estamos considerand os valores manualmente adicionados ao TEID para
  // identificar a aplicação. As 4 primeiras são TCP, e as demais UDP.
  bool ehTcp = (app->GetTeid () & 0xF) <= 3;
  uint16_t port = app->GetTeid () + 10000;

  // Na política atual o IP do cliente é utilizado para definir o switch.
  // IPs pares são enviados para o HW e IP ímpares para o SW.
  Ptr<Ipv4> ipv4 = app->GetNode ()->GetObject<Ipv4>();
  Ipv4Address ipv4addr = ipv4->GetAddress (1,0).GetLocal ();
  uint32_t ipImpar = ipv4addr.Get () & 1;
  Ptr<OFSwitch13Device> switchDevice = ipImpar ? switchDeviceSw : switchDeviceHw;

  // String representando o TEID em hexadecimal.
  uint32_t teid = app->GetTeid ();
  char teidStr[11];
  sprintf (teidStr,"0x%08x",teid);

  // Verifica os recursos disponíveis no switch (processamento e uso de tablas)
  double usage = switchDevice->GetFlowTableUsage (0);
  double processing = switchDevice->GetProcessingUsage ();

  // Se uso de tabela excede o limiar de bloqueio, então bloqueia o tráfego.
  if (usage > usageLimit)
    {
      m_requestTrace (teid, false);
      return false;
    }

  // Se udo de processamento excede o limiar de bloquei, a decisão do bloqueio
  // se baseia no atributo de política de bloqueio.
  if (m_blockPol && processing > processingLimit)
    {
      m_requestTrace (teid, false);
      return false;
    }

  // Se o tráfego não foi bloqueado, então vamos instalar as regras de
  // encaminhamento no switch SW ou HW.
  std::ostringstream cmdUl, cmdDl;
  cmdUl << "flow-mod cmd=add,prio=64,table=0,cookie=" << teidStr
        << " eth_type=0x800,ip_src=" << ipv4addr;
  cmdDl << "flow-mod cmd=add,prio=64,table=0,cookie=" << teidStr
        << " eth_type=0x800,ip_dst=" << ipv4addr;
  if (ehTcp)
    {
      // Regras específicas para protocolo TCP
      cmdUl << ",ip_proto=6,tcp_dst=" << port;
      cmdDl << ",ip_proto=6,tcp_src=" << port;
    }
  else
    {
      // Regras específicas para protocolo UDP
      cmdUl << ",ip_proto=17,udp_dst=" << port;
      cmdDl << ",ip_proto=17,udp_src=" << port;
    }
  cmdUl << " write:group=1";
  cmdDl << " write:group=2";
  DpctlExecute (switchDevice->GetDatapathId (), cmdUl.str ());
  DpctlExecute (switchDevice->GetDatapathId (), cmdDl.str ());

  m_requestTrace (teid, true);
  return true;
}

bool
CustomController::DedicatedBearerRelease (Ptr<SvelteClient> app, uint64_t imsi)
{
  NS_LOG_FUNCTION (this << app << imsi);

  // String representando o TEID em hexadecimal.
  uint32_t teid = app->GetTeid ();
  char teidStr[11];
  sprintf (teidStr,"0x%08x",teid);

  // Na política atual o IP do cliente é utilizado para definir o switch.
  // IPs pares são enviados para o HW e IP ímpares para o SW.
  Ptr<Ipv4> ipv4 = app->GetNode ()->GetObject<Ipv4>();
  Ipv4Address ipv4addr = ipv4->GetAddress (1,0).GetLocal ();
  uint32_t ipImpar = ipv4addr.Get () & 1;
  Ptr<OFSwitch13Device> switchDevice = ipImpar ? switchDeviceSw : switchDeviceHw;

  // Remover todas as regras identificadas pelo cookie com o TEID do tráfego.
  std::ostringstream cmd;
  cmd << "flow-mod cmd=del"
      << ",cookie="       << teidStr
      << ",cookie_mask="  << COOKIE_STRICT_MASK_STR;
  DpctlExecute (switchDevice->GetDatapathId (), cmd.str ());

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

  // De acordo com a política de roteamento em vigor, faça a chamada para a
  // função que instala as regras adequadamente.
  ConfigureByIp ();
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

  OFSwitch13Controller::HandshakeSuccessful (swtch);
}

void
CustomController::ConfigureByIp ()
{
  NS_LOG_FUNCTION (this);

  // Nesta função vamos instalar as regras de roteamento interno com base no IP
  // do usuário: IPs pares são enviados para o switch HW e IPs ímpares são
  // enviados para o switch SW. Observe as regras sempre na tabela 1.

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

} // namespace ns3
