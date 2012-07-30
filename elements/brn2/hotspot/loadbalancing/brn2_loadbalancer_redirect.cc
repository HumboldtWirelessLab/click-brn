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


#include "brn2_loadbalancer_redirect.hh"

#include "elements/brn2/routing/linkstat/brn2_brnlinkstat.hh"
#include "brn2_lb_rerouting.hh"


CLICK_DECLS

LoadBalancerRedirect::LoadBalancerRedirect():
  _linkstat(NULL),
  _debug(0)
{
}

LoadBalancerRedirect::~LoadBalancerRedirect()
{
}

int LoadBalancerRedirect::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
    "ETHERADDRESS", cpkP+cpkM, cpEtherAddress, /*"ether address",*/ &_me,
    "LINKSTAT", cpkP+cpkM, cpElement, /*"LinkStat",*/ &_linkstat,
    "ROUTING", cpkP+cpkM, cpElement, &_rerouting,
    "DEBUG", cpkP, cpInteger, /*"debug",*/ &_debug,
    cpEnd) < 0)
      return -1;

  if (!_linkstat || !_linkstat->cast("BRNLinkStat"))
  {
    _linkstat = NULL;
    click_chatter("kein Linkstat");
  }

  click_chatter("Name: %s",_rerouting->lb_routing_name());

  return 0;
}

int LoadBalancerRedirect::initialize(ErrorHandler *)
{
  click_srandom(_me.hashcode());
  redirectFlows.clear();
  return 0;
}

void LoadBalancerRedirect::push( int port, Packet *packet )
{
	uint8_t *p_data = (uint8_t *)packet->data();
  //FlowInfo *flow;
  EtherAddress *redirectNode;

  EtherAddress *fromEther;
//  EtherAddress *toEther;
  IPAddress *fromIP;
  IPAddress *toIP;
  uint8_t headerLen;
  uint16_t srcPort;
  uint16_t dstPort;

  if ( _linkstat == NULL )
	{
		output(0).push(packet);
		return;
	}
 
  if ( port == 0 )
  {
    //from client
    click_ether *ether = (click_ether *)packet->data();
    click_ip *ip_header = (click_ip *)&packet->data()[14];

//    toEther = new EtherAddress(&p_data[0]);
    fromEther = new EtherAddress(&p_data[6]);

    headerLen = ip_header->ip_hl;

    toIP = new IPAddress(ip_header->ip_dst);
    fromIP = new IPAddress(ip_header->ip_src);

    click_udp *udp_header = (click_udp*) &packet->data()[sizeof(click_ether) + (headerLen * 4 )];
    srcPort = ntohs(udp_header->uh_sport);
    dstPort = ntohs(udp_header->uh_dport);

//    click_chatter("Ether: %s src: %s:%d dst: %s:%d",fromEther->unparse().c_str(),fromIP->unparse().c_str(),srcPort,toIP->unparse().c_str(),dstPort);

    redirectNode = getNodeForFlow(fromIP, toIP, srcPort, dstPort);

    if ( redirectNode == NULL )
    {
      redirectNode = getBestNodeForFlow(fromEther, fromIP, toIP, srcPort, dstPort);
      addFlow(fromEther,redirectNode, fromIP, toIP, srcPort, dstPort);
    }

    if ( *redirectNode == _me )
    {
      //do it local
      output(1).push(packet);
    }
    else
    {
      memcpy(&p_data[0] , redirectNode->data(), 6);
      memcpy(&p_data[6] , _me.data(), 6 );
      memcpy(ether->ether_dhost,redirectNode->data(),6);
      memcpy(ether->ether_shost, _me.data(),6);
      ether->ether_type = 0x8780; /*0*/
      packet->set_ether_header(ether);

      //sent to neighbout
      output(2).push(packet);
    }
	}
	else
	{
//    click_chatter("Now back to client");

    //click_ether *ether = (click_ether *)packet->data();
    click_ip *ip_header = (click_ip *)&packet->data()[14];

//    toEther = new EtherAddress(&p_data[0]);
    fromEther = new EtherAddress(&p_data[6]);

    headerLen = ip_header->ip_hl;

    toIP = new IPAddress(ip_header->ip_dst);
    fromIP = new IPAddress(ip_header->ip_src);

    click_udp *udp_header = (click_udp*) &packet->data()[sizeof(click_ether) + (headerLen * 4 )];
    srcPort = ntohs(udp_header->uh_sport);
    dstPort = ntohs(udp_header->uh_dport);

//    click_chatter("Ether: %s src: %s:%d dst: %s:%d",fromEther->unparse().c_str(),fromIP->unparse().c_str(),srcPort,toIP->unparse().c_str(),dstPort);

    EtherAddress *srcNode = getSrcForFlow(toIP, fromIP, dstPort, srcPort);

    if ( srcNode != NULL )
    {
//      click_chatter("Got Flow: src is %s",srcNode->unparse().c_str());
      click_ether *ether = (click_ether *)packet->data();
      memcpy(&p_data[0] , srcNode->data(), 6);
      memcpy(&p_data[6] , _me.data(), 6 );
      memcpy(ether->ether_dhost, srcNode->data(),6);
      memcpy(ether->ether_shost, _me.data(),6);
      ether->ether_type = htons(0x0800);
      packet->set_ether_header(ether);
      output(0).push(packet);
    }
    else
    {
      click_chatter("no flow");
      packet->kill();
    }

    return;
	}
}


void LoadBalancerRedirect::addFlow(EtherAddress *srcEtherAddress,EtherAddress *redirectAP, IPAddress *srcIP, IPAddress *dstIP, uint16_t srcPort, uint16_t dstPort)
{
  FlowInfo *newFlow;

  newFlow = new FlowInfo(srcEtherAddress, redirectAP, srcIP, dstIP, srcPort, dstPort);
  redirectFlows.push_back(newFlow);
}

void LoadBalancerRedirect::removeFlow(EtherAddress *srcEtherAddress, IPAddress *srcIP, IPAddress *dstIP, uint16_t srcPort, uint16_t dstPort)
{
  int i;
  FlowInfo *acFlow;

  for ( i = 0; i < redirectFlows.size(); i++ ) { 
    acFlow = redirectFlows[i];

    if ( ( *(acFlow->_src_address) == *srcEtherAddress ) && ( *(acFlow->_src_ip) == *srcIP ) && (*(acFlow->_dst_ip) == *dstIP ) && ( (acFlow->_src_port) == srcPort ) && ( (acFlow->_dst_port) = dstPort ) )
    {
      redirectFlows.erase( redirectFlows.begin() + i );
      return;
    }
  }
}

EtherAddress *LoadBalancerRedirect::getNodeForFlow( IPAddress *srcIP, IPAddress *dstIP, uint16_t srcPort, uint16_t dstPort)
{
  int i;
  FlowInfo *acFlow;

  for ( i = 0; i < redirectFlows.size(); i++ ) {
    acFlow = redirectFlows[i];

    if ( ( *(acFlow->_src_ip) == *srcIP ) && (*(acFlow->_dst_ip) == *dstIP ) && ( (acFlow->_src_port) == srcPort ) && ( (acFlow->_dst_port) = dstPort ) )
    {
      return acFlow->_redirect_ap;
    }
  }

  return NULL;

}

EtherAddress *LoadBalancerRedirect::getSrcForFlow( IPAddress *srcIP, IPAddress *dstIP, uint16_t srcPort, uint16_t dstPort)
{
  int i;
  FlowInfo *acFlow;

  for ( i = 0; i < redirectFlows.size(); i++ ) {
    acFlow = redirectFlows[i];

    if ( ( *(acFlow->_src_ip) == *srcIP ) && (*(acFlow->_dst_ip) == *dstIP ) && ( (acFlow->_src_port) == srcPort ) && ( (acFlow->_dst_port) = dstPort ) )
    {
//      click_chatter("ME. %s Redirected AP: %s  Client: %s",_me.unparse().c_str(),acFlow->_redirect_ap->unparse().c_str(),acFlow->_src_address->unparse().c_str());

      return acFlow->_src_address;
    }
  }

  return NULL;

}
EtherAddress* LoadBalancerRedirect::getBestNodeForFlow(EtherAddress *srcEtherAddress, IPAddress *srcIP, IPAddress *dstIP,uint16_t srcPort, uint16_t dstPort)
{
  Vector<EtherAddress> neighbors;
  EtherAddress *bestNode;

  click_chatter("bla");
  _rerouting->getBestNodeForFlow();
  click_chatter("bla");

  if ( ( srcEtherAddress == NULL ) || ( srcIP == NULL ) || ( dstIP == NULL ) || ( srcPort = 0 ) || ( dstPort == 0 ) ) return NULL;
  bestNode = NULL;

  if ( _linkstat != NULL )
  {
    _linkstat->get_neighbors(&neighbors);

    if ( neighbors.size() > 0 )
    {
      int num = click_random() % neighbors.size();
      //num =  neighbors.size();
      if ( num == neighbors.size() )
        bestNode = &_me;
      else
        bestNode = new EtherAddress(neighbors[num].data());
    }
    else
      bestNode = &_me;
  }

  return bestNode;

}

void LoadBalancerRedirect::nodeDetection()
{
  if ( _linkstat == NULL ) return;

  _linkstat->get_neighbors(&_neighbors);
}

void LoadBalancerRedirect::add_handlers()
{
}

#include <click/vector.cc>

template class Vector<LoadBalancerRedirect::FlowInfo*>;

CLICK_ENDDECLS
EXPORT_ELEMENT(LoadBalancerRedirect)
