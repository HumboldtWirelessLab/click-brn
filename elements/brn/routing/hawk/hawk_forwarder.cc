/* Copyright (C) 2005 BerlinRoofNet Lab
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA. 
 *
 * For additional licensing options, consult http://www.BerlinRoofNet.de 
 * or contact brn@informatik.hu-berlin.de. 
 */

/*
 * srcforwarder.{cc,hh} -- forwards dsr source routed packets
 * A. Zubow
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "hawk_protocol.hh"
#include "hawk_forwarder.hh"


CLICK_DECLS

HawkForwarder::HawkForwarder()
  : _debug(BrnLogger::DEFAULT),
    _opt_first_dst(false),
    _opt_better_finger(false),
    _opt_successor_forward(false),
    _me(), _falconrouting(NULL),_rt(NULL),_frt(NULL)
{

}

HawkForwarder::~HawkForwarder()
{
}

int
HawkForwarder::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "ROUTINGTABLE", cpkP+cpkM, cpElement, &_rt,
      "FRT", cpkP+cpkM, cpElement, &_frt,
      "FALCONROUTING", cpkP+cpkM, cpElement, &_falconrouting,
      "OPTFIRSTDST", cpkP, cpBool, &_opt_first_dst,
      "OPTBETTERFINGER", cpkP, cpBool, &_opt_better_finger,
      "OPTSUCCESSORFORWARD", cpkP, cpBool, &_opt_successor_forward,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("BRN2NodeIdentity")) 
    return errh->error("NodeIdentity not specified");

  return 0;
}

int
HawkForwarder::initialize(ErrorHandler *)
{
  return 0;
}

void
HawkForwarder::uninitialize()
{
  //cleanup
}

void
HawkForwarder::push(int port, Packet *p_in)
{

  uint32_t metric = 0; //TODO: better Invalid_lt_metric 
  uint32_t last_metric ; 
  BRN_DEBUG("push(): %d",p_in->length());

  struct hawk_routing_header *header = HawkProtocol::get_route_header(p_in);
  click_ether *ether = HawkProtocol::get_ether_header(p_in);

  EtherAddress dst_addr(ether->ether_dhost);
  EtherAddress src_addr(ether->ether_shost);

  EtherAddress next(header->_next_etheraddress);

  const click_ether *annotated_ether = reinterpret_cast<const click_ether *>(p_in->ether_header());
  EtherAddress last_addr(annotated_ether->ether_shost);
  BRN_DEBUG("Me: %s src: %s dst: %s last: %s next: %s",_me->getNodeName().c_str(),
                                                       src_addr.unparse().c_str(),
                                                       dst_addr.unparse().c_str(),
                                                       last_addr.unparse().c_str(),
                                                       next.unparse().c_str());

//if next is initial i am the sender and dont have to inc the metric
if (memcmp(next.data(), EtherAddress().data(), 6) != 0){ 
     last_metric =  1 /*_rt->_link_table->get_host_metric_to_me(last_addr)*/;
     HawkProtocol::add_metric(p_in,last_metric);
     metric = header->_metric;
}
BRN_DEBUG("got packet with metric %d", header->_metric);
 if ( _me->isIdentical(&src_addr)) {header->_metric = 0; BRN_DEBUG("cleared metric");}  
 

  /*
  
  for(int i = 0; i < _rt->_rt.size(); i++) {
    entry = _rt->_rt[i];

    BRN_DEBUG("entry node=%s, next_hop=%s", entry->_dst.unparse().c_str(),entry->_next_phy_hop.unparse().c_str());
    BRN_DEBUG(" next_overlay=%s",entry->_next_hop.unparse().c_str());
  }*/

  /*
   * Here we are sure than the packet comes from the source over last hop,
   * so we add the entry (last hop as next hop to src
   */
  HawkRoutingtable::RTEntry *entry = _rt->getEntry(&src_addr);
  if ( memcmp(src_addr.data(), _falconrouting->_me->_ether_addr.data(), 6) != 0 
	&& memcmp(next.data(), EtherAddress().data(), 6) != 0 && 
     (entry == NULL || !_rt->_use_metric || ( entry != NULL && ( header->_metric <= entry->_metric || memcmp(entry->_next_phy_hop.data(),last_addr.data(),6) == 0 )))) {
    BRN_DEBUG("Node on route, so add info for backward route");
    _rt->addEntry(&(src_addr), header->_src_nodeid, 16, &(last_addr),metric);
    //and do it also with last hop cause he must be a neighbour
    _rt->addEntry(&(last_addr),header->_src_nodeid,16,&(last_addr),last_metric);
  }

  uint8_t ttl = BRNPacketAnno::ttl_anno(p_in);

  if (port == 0) ttl--;

        //check i have a successor request  then i have to save the route to the initialiser of the packet
        if(_opt_successor_forward && !_me->isIdentical(&src_addr) && ether->ether_type == htons(ETHERTYPE_BRN) ){
         	struct click_brn* brn_p = (click_brn*)&(p_in->data()[sizeof(struct hawk_routing_header) + sizeof(click_ether)]);
         	if (brn_p->dst_port == BRN_PORT_DHTROUTING){
        	  struct dht_packet_header *dht_header = (dht_packet_header*)&(p_in->data()[sizeof(struct hawk_routing_header) + sizeof(struct click_brn) + sizeof(click_ether)]);
    		  //is not for me so check, if the packet is a successor request for somebody
        	  if(dht_header->minor_type == FALCON_MINOR_REQUEST_SUCCESSOR || dht_header->minor_type == FALCON_MINOR_UPDATE_SUCCESSOR){
               	 BRN_DEBUG("I got a Successor-Request/Reply");

			 if (memcmp(next.data(), EtherAddress().data(), 6) == 0){
 			 struct falcon_routing_packet *request = (struct falcon_routing_packet*)&(p_in->data()[sizeof(struct hawk_routing_header) + sizeof(click_ether) 
															+ sizeof(struct click_brn) + sizeof(dht_packet_header)]);

			//get the falcon metric and put it into hawk header
					header->_metric = request->metric ;
			}else{//Korrektur der Backward-Metrik
				 HawkRoutingtable::RTEntry *e = _rt->getEntry(&src_addr);
				if(_rt->_use_metric && e != NULL && header->_metric > e->_metric && memcmp(e->_next_phy_hop.data(),last_addr.data(),6) != 0 )
				{
					BRN_DEBUG("metric repair from %d to %d",header->_metric,e->_metric);
					 header->_metric = e->_metric;
				}
			     }

		}
	    }
	}


  if ( _me->isIdentical(&dst_addr) ) {
    BRN_DEBUG("Is for me");

    HawkProtocol::strip_route_header(p_in);

    if ( BRNProtocol::is_brn_etherframe(p_in) )
      BRNProtocol::get_brnheader_in_etherframe(p_in)->ttl = ttl;

    output(1).push(p_in);
  } else {
    if ( _me->isIdentical(&next) ) { 
	//i'm next overlay hop
      HawkProtocol::clear_next_hop(p_in);
    }

    HawkProtocol::set_rew_metric(p_in,header->_rew_metric - 1);
    //check first if i have a direct route in my routingtable and take this
    // before checking next link to next overlay
	EtherAddress *next_phy_hop = NULL;

    if(!HawkProtocol::has_next_hop(p_in)) next_phy_hop = _rt->getNextHop(&dst_addr);
    if(_rt->_use_metric && next_phy_hop != NULL) HawkProtocol::set_rew_metric(p_in,(_rt->getEntry(&dst_addr))->_metric);
    if(next_phy_hop != NULL) header->_flags = 1;//mark direct route
    if (next_phy_hop == NULL && HawkProtocol::has_next_hop(p_in)){
        if(_opt_first_dst){
          next_phy_hop = _rt->getNextHop(&dst_addr);
         if (next_phy_hop != NULL){
 	   if(_rt->_use_metric)
	   {
	    uint8_t rew = (_rt->getEntry(&dst_addr))->_metric;
 	    BRN_DEBUG("my rew: %d, received rew:%d",rew,header->_rew_metric);
	    if( header->_rew_metric != 0 && rew >= header->_rew_metric)next_phy_hop = NULL; //bei metrik nur ersetzen wenn k�rzerer Weg zu erwarten 
	    else HawkProtocol::set_rew_metric(p_in,rew);
          }
	   else if(header->_flags == 1)next_phy_hop = NULL;
          }
	   if(next_phy_hop != NULL) { HawkProtocol::clear_next_hop(p_in); header->_flags = 1;}//mark direct route
        }
       if(next_phy_hop == NULL && _opt_better_finger && header->_flags != 1
         /* && ether->ether_type == htons(ETHERTYPE_BRN)*/) {
         //struct click_brn* brn_p = (click_brn*)&(p_in->data()[sizeof(struct hawk_routing_header) + sizeof(click_ether)]);
         //if (brn_p->dst_port == BRN_PORT_FLOW){
        	
	   
          //when i have a better finger i should prefer it
          DHTnode* better = NULL;
	   better = _falconrouting->get_responsibly_node_for_key(header->_dst_nodeid, &(_rt->_known_hosts));
          if(better != NULL && !better->equalsEtherAddress(_falconrouting->_me) && 
		FalconFunctions::is_in_between(header->_next_nodeid,header->_dst_nodeid,better->_md5_digest)
		&& _falconrouting->_frt->isFixSuccessor() == true ){
          
	  	//Since next hop in the overlay is not necessarily my neighbour
        	//i use the hawk table to get the real next hop
       	next_phy_hop = _rt->getNextHop(&(better->_ether_addr));
	       BRN_DEBUG("better: %s",better->_ether_addr.unparse().c_str());
 		char digest[16*2 + 1];
		MD5::printDigest(header->_next_nodeid, digest);
		BRN_DEBUG("old finger ID: %s", digest);
		char digest2[16*2 + 1];
		MD5::printDigest(header->_dst_nodeid, digest2);
		BRN_DEBUG("old finger ID: %s", digest2);

		// important to do that:
		    if (_rt->isNeighbour(next_phy_hop))
 		          next_phy_hop = &(better->_ether_addr);

         	BRN_DEBUG("I have a better finger: %s , next hop: %s",better->_ether_addr.unparse().c_str(),next_phy_hop->unparse().c_str());
        	//der alte overlay-hop muss gelöscht werden damit der neue gesetzt werden kann.
        	HawkProtocol::clear_next_hop(p_in);
        	if ( next_phy_hop == NULL ) {
          		BRN_ERROR("No next hop for ovelay dst. Kill packet.");
          		BRN_ERROR("RT: %s",_rt->routingtable().c_str());
          		p_in->kill();
          		return;
		}
         } 
        else next_phy_hop = NULL;
      }
     //}
      //check wether i have direct link/path to next overlay
      if(next_phy_hop == NULL)next_phy_hop = _rt->getNextHop(&next);
    }
    if ( next_phy_hop == NULL ) {
    	BRN_DEBUG("No next physical hop known. Search for next overlay.");
    	//no, so i check for next hop in the overlay
	if(_rt->_use_metric)HawkProtocol::set_rew_metric(p_in,0);
	header->_flags = 0;//demark direct route 
    	DHTnode *n = NULL;
       n = _falconrouting->get_responsibly_node_for_key(header->_dst_nodeid, &(_rt->_known_hosts));
     	
 	BRN_DEBUG("fingertable size=%d" , _falconrouting->_frt->_fingertable.size() );
  	/*for( int i = 0; i < _falconrouting->_frt->_fingertable.size(); i++ )
  	{
    	 	DHTnode* node = _falconrouting->_frt->_fingertable.get_dhtnode(i);
		BRN_DEBUG("addr=%s",node->_ether_addr.unparse().c_str());
	}*/	
      BRN_DEBUG("Responsible is %s",n->_ether_addr.unparse().c_str());

      if ( n->equalsEtherAddress(_falconrouting->_me) ) { //for clients, which have the
                                                          //same id but different
                                                          // ethereaddress
        //n = _falconrouting->get_responsibly_node_for_key(header->_dst_nodeid, &(_rt->_known_hosts));
        HawkProtocol::strip_route_header(p_in);           //TODO: use other output port
                                                          //      for client
        if ( BRNProtocol::is_brn_etherframe(p_in) )
          BRNProtocol::get_brnheader_in_etherframe(p_in)->ttl = ttl;

        output(1).push(p_in);
        return;
      } else {
        //Since next hop in the overlay is not necessarily my neighbour
        //i use the hawk table to get the real next hop
        next_phy_hop = _rt->getNextHop(&(n->_ether_addr));
    	
	 //testen ob das besser ist
	     if (_rt->isNeighbour(next_phy_hop))
	           next_phy_hop = &(n->_ether_addr);

        //der alte overlay-hop muss gelöscht werden damit der neue gesetzt werden kann.
        HawkProtocol::clear_next_hop(p_in);
        if ( next_phy_hop == NULL ) {
          BRN_ERROR("No next hop for ovelay dst. Kill packet.");
          BRN_ERROR("RT: %s",_rt->routingtable().c_str());
          p_in->kill();
          return;
        }

     }
   }//if next_phy_hop == NULL
    
   BRN_DEBUG("Next hop: %s", next_phy_hop->unparse().c_str());
   DHTnode phy_hop_node = DHTnode(*next_phy_hop);
  //es muss der next-Knoten beibehalten werden bis dieser erreicht wurde, denn eventuell weiß nur der overlayhop wie es weitergeht,
  //und die Zwischenknoten dorthin kennen keinen Weg zum Endziel 
  if ( _rt->isNeighbour(next_phy_hop) && ! HawkProtocol::has_next_hop(p_in) ) {
      BRN_INFO("Nexthop is a neighbour");
      HawkProtocol::set_next_hop(p_in,next_phy_hop,phy_hop_node._md5_digest);
  } else {
      BRN_INFO("Nexthop is not a neighbour");

      if ( ! HawkProtocol::has_next_hop(p_in) ) HawkProtocol::set_next_hop(p_in,next_phy_hop,phy_hop_node._md5_digest);

      int loop_counter = 0;
      while ( ( next_phy_hop != NULL ) && (! _rt->isNeighbour(next_phy_hop)) ) {
        next_phy_hop = _rt->getNextHop(next_phy_hop);
        loop_counter++;
        if ( loop_counter > 10 ) next_phy_hop = NULL; //TODO: why 10??
      }

      if ( next_phy_hop == NULL ) {
        BRN_ERROR("No valid next hop found. Discard packet.");

  	 for(int i = 0; i < _rt->_rt.size(); i++) {
    		HawkRoutingtable::RTEntry *entry = _rt->_rt[i];
	  	BRN_DEBUG("entry node=%s, next_hop=%s", entry->_dst.unparse().c_str(),entry->_next_phy_hop.unparse().c_str());
    		BRN_DEBUG(" next_overlay=%s",entry->_next_hop.unparse().c_str());
  	 }
        p_in->kill();
        return;
      }
  }

  Packet *brn_p = BRNProtocol::add_brn_header(p_in, BRN_PORT_HAWK, BRN_PORT_HAWK, ttl, BRNPacketAnno::tos_anno(p_in));

  BRNPacketAnno::set_src_ether_anno(brn_p,_falconrouting->_me->_ether_addr);  //TODO: take address from anywhere else
  BRNPacketAnno::set_dst_ether_anno(brn_p,*next_phy_hop);
  BRNPacketAnno::set_ethertype_anno(brn_p,ETHERTYPE_BRN);
  BRN_DEBUG("Send Packet to %s with link metric %d",next_phy_hop->unparse().c_str(),  _rt->_link_table->get_host_metric_to_me(*next_phy_hop));
  output(0).push(brn_p);
   
 }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  HawkForwarder *sf = reinterpret_cast<HawkForwarder *>(e);
  return String(sf->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  HawkForwarder *sf = reinterpret_cast<HawkForwarder *>(e);
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  sf->_debug = debug;
  return 0;
}

void
HawkForwarder::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(HawkForwarder)
