#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include <clicknet/tcp.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "brn2_loadbalancer_foreign_flow_handler.hh"

CLICK_DECLS

LoadBalancerForeignFlowHandler::LoadBalancerForeignFlowHandler():
  _debug(0)
{
}

LoadBalancerForeignFlowHandler::~LoadBalancerForeignFlowHandler()
{
}

int LoadBalancerForeignFlowHandler::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
    "ETHERADDRESS", cpkP+cpkM, cpEtherAddress, /*"ether address",*/ &_me,
    "DEBUG", cpkP, cpInteger, /*"debug",*/ &_debug,
    cpEnd) < 0)
      return -1;

  return 0;
}

int LoadBalancerForeignFlowHandler::initialize(ErrorHandler *)
{
  return 0;
}

void LoadBalancerForeignFlowHandler::push( int port, Packet *packet )
{
  ForeignFlowInfo *foreignFlow;

  EtherAddress *fromEther;
//  EtherAddress *toEther;
  IPAddress *fromIP;
  IPAddress *toIP;
  uint8_t headerLen;
  uint16_t srcPort;
  uint16_t dstPort;

  uint8_t *p_data;

	if ( port == 0 )
  {
//    click_chatter("Paket from Neighbor! Try to push it to Internet");

//    click_ether *ether = (click_ether *)packet->data();
    click_ip *ip_header = (click_ip *)&packet->data()[14];

    p_data = (uint8_t *)packet->data();

//    toEther = new EtherAddress(&p_data[0]);
    fromEther = new EtherAddress(&p_data[6]);

    headerLen = ip_header->ip_hl;

    toIP = new IPAddress(ip_header->ip_dst);
    fromIP = new IPAddress(ip_header->ip_src);

    click_udp *udp_header = (click_udp*) &packet->data()[sizeof(click_ether) + (headerLen * 4 )];
    srcPort = ntohs(udp_header->uh_sport);
    dstPort = ntohs(udp_header->uh_dport);

//    click_chatter("Ether: %s src: %s:%d dst: %s:%d",fromEther->unparse().c_str(),fromIP->unparse().c_str(),srcPort,toIP->unparse().c_str(),dstPort);

    foreignFlow = (ForeignFlowInfo*)getFlow(fromIP, toIP, srcPort, dstPort);

    if ( foreignFlow == NULL )
    {
      foreignFlow = (ForeignFlowInfo*)addFlow(fromEther, fromIP, toIP, srcPort, dstPort);
    }

    output(1).push(packet);
  }
  else
  {
//    click_chatter("Paket from Net! push back to src");

    click_ip *ip_header = (click_ip *)packet->data();

    headerLen = ip_header->ip_hl;

    toIP = new IPAddress(ip_header->ip_dst);
    fromIP = new IPAddress(ip_header->ip_src);

    click_udp *udp_header = (click_udp*) &packet->data()[(headerLen * 4 )];
    srcPort = ntohs(udp_header->uh_sport);
    dstPort = ntohs(udp_header->uh_dport);

    foreignFlow = (ForeignFlowInfo*)getFlow(toIP, fromIP, dstPort, srcPort);
    
//    click_chatter("Ether: %s src: %s:%d dst: %s:%d",foreignFlow->_src_address->unparse().c_str(),fromIP->unparse().c_str(),srcPort,toIP->unparse().c_str(),dstPort);

    if (foreignFlow != NULL )
    {
      WritablePacket *q = packet->push_mac_header(14);

      if ( q == NULL )
      {
        click_chatter("Error on set MAC-Header");
        packet->kill();
        return;
      }

  //    click_chatter("Found flow");

      click_ether *ether = (click_ether *)q->data();
      p_data = (uint8_t *)q->data();

      memcpy( ether->ether_dhost, foreignFlow->_src_address->data(), 6);
      memcpy( ether->ether_shost, _me.data(), 6 );
      ether->ether_type = 0x8880; //0
      q->set_ether_header(ether);

      output(0).push(q);
    }
    else
    {
      WritablePacket *q = packet->push_mac_header(14);

      if ( q == NULL )
      {
        click_chatter("Error on set MAC-Header");
        packet->kill();
        return;
      }

      click_ether *ether = (click_ether *)q->data();
      p_data = (uint8_t *)q->data();

      memcpy( ether->ether_dhost, _me.data(), 6);
      memcpy( ether->ether_shost, _me.data(), 6 );
      ether->ether_type = 0x8880; //0
      q->set_ether_header(ether);

      click_chatter("No flow, try local");
      output(2).push(q);
    }
  }

	return;
}

void *LoadBalancerForeignFlowHandler::addFlow(EtherAddress *fromAP, IPAddress *srcIP, IPAddress *dstIP, uint16_t srcPort, uint16_t dstPort)
{
  ForeignFlowInfo *newFlow;

  newFlow = new ForeignFlowInfo(fromAP, srcIP, dstIP, srcPort, dstPort);
  foreignFlows.push_back(newFlow);

  return newFlow;
}

void LoadBalancerForeignFlowHandler::removeFlow(IPAddress *srcIP, IPAddress *dstIP, uint16_t srcPort, uint16_t dstPort)
{
  int i;
  ForeignFlowInfo *acFlow;

  for ( i = 0; i < foreignFlows.size(); i++ ) {
    acFlow = foreignFlows[i];

    if ( ( *(acFlow->_src_ip) == *srcIP ) && (*(acFlow->_dst_ip) == *dstIP ) && ( (acFlow->_src_port) == srcPort ) && ( (acFlow->_dst_port) = dstPort ) )
    {
      foreignFlows.erase( foreignFlows.begin() + i );
      return;
    }
  }
}

void *LoadBalancerForeignFlowHandler::getFlow(IPAddress *srcIP, IPAddress *dstIP, uint16_t srcPort, uint16_t dstPort)
{
  int i;
  ForeignFlowInfo *acFlow;

  for ( i = 0; i < foreignFlows.size(); i++ ) {
    acFlow = foreignFlows[i];

    if ( ( *(acFlow->_src_ip) == *srcIP ) && (*(acFlow->_dst_ip) == *dstIP ) && ( (acFlow->_src_port) == srcPort ) && ( (acFlow->_dst_port) = dstPort ) )
    {
      return (void*)acFlow;
    }
  }

  return NULL;

}


void LoadBalancerForeignFlowHandler::add_handlers()
{
}

CLICK_ENDDECLS
EXPORT_ELEMENT(LoadBalancerForeignFlowHandler)
