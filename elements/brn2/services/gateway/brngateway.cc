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

#include <click/config.h>

#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/string.hh>
#include <click/etheraddress.hh>
#include <click/router.hh>
// memcpy
#include <string.h>

// to generate dht packets
#include "elements/brn2/dht/storage/dhtoperation.hh"
#include "elements/brn2/dht/storage/dhtstorage.hh"

#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "brnsetgatewayonflow.hh"
#include "brngateway.hh"



CLICK_DECLS

BRNGateway::BRNGateway() :
  _flows(NULL),
  _timer_refresh_known_gateways(this),
  _update_gateways_interval(DEFAULT_UPDATE_GATEWAYS_INTERVAL),
  _timer_update_dht(this),
  _update_dht_interval(DEFAULT_UPDATE_DHT_INTERVAL),
  unique_id(0) {}

BRNGateway::~BRNGateway() {}

/*******************
 *
 * methods for click
 *
 *//////////////////
int
BRNGateway::configure(Vector<String> &conf, ErrorHandler *errh) {

  if (cp_va_kparse(conf, this, errh,
                  "ETHERADDRESS", cpkP+cpkM, cpEthernetAddress, &_my_eth_addr,
                  "SETGATEWAYONFLOW", cpkP+cpkM, cpElement, &_flows,
                  "UPDATE_GATEWAYS_INTERVAL", cpkP+cpkM, cpSeconds, &_update_gateways_interval,
                  "UPDATE_DHT_INTERVAL", cpkP+cpkM, cpSeconds, &_update_dht_interval,
                  cpEnd) < 0)
    return -1;

  if (_update_gateways_interval == 0)
    errh->error("UPDATE_GATEWAYS_INTERVAL is zero");

  if (_update_dht_interval == 0)
    errh->error("UPDATE_GATEWAYS_INTERVAL is zero");

  if (_flows->cast("BRNSetGatewayOnFlow") == 0) {
    return errh->error("No element of type BRNSetGatewayOnFlow specified.");
  }

  return 0;
}

int
BRNGateway::initialize(ErrorHandler *errh) {
  (void) (errh);

  _timer_update_dht.initialize(this);
  _timer_update_dht.schedule_now();

  _timer_refresh_known_gateways.initialize(this);
  _timer_refresh_known_gateways.schedule_now();

  return 0;
}

void BRNGateway::cleanup(CleanupStage stage) {
  // TODO fire packet to remove this node from gateway list
  // Does this work with DHT and so on
  // probably not

  if (stage == CLEANUP_ROUTER_INITIALIZED) {
    // generate packet to remove from dht
    // this maybe to late, since dht may have gone away already
    // and thus unable to handle this packet anymore
    BRN_INFO("Remove this node as gateway from DHT, if it was in the DHT");
  }
}

/*****************
 *
 * Packet handling
 *
 *////////////////
void
BRNGateway::push(int, Packet *p) {
  // TODO
  // check if this a brn gateway packet
  // what the preferred way?

  handle_dht_response(p);
  p->kill();
  return;
}

/********
 *
 * Timers
 *
 *
 *///////
void
BRNGateway::timer_refresh_known_gateways_hook(Timer *t) {
  //BRN_INFO(" \n*  Timer executed to get update list of gateways  \n* ");
  //click_chatter(" \n*  Timer executed to get update list of gateways  \n* ");

  // output packet
  output(0).push(get_getways_from_dht());

  // ... and reschdule timer
  t->reschedule_after_sec(_update_gateways_interval);
}

void
BRNGateway::timer_update_dht_hook(Timer *t) {
  //BRN_INFO(" \n*  Timer executed to get update this gateway  \n* ");
  //click_chatter(" \n*  Timer executed to get update this gateway  \n* ");

  // is this node a gateway
  const BRNGatewayEntry *gwe = get_gateway();

  // output packet, if this node is a gateway
  if (gwe != NULL)
    output(0).push(update_gateway_in_dht(*gwe));

  // ... and reschdule timer
  t->reschedule_after_sec(_update_dht_interval);
}

void
BRNGateway::run_timer(Timer *t) {
  if (t == &_timer_refresh_known_gateways) {
    timer_refresh_known_gateways_hook(t);
  }
  else if (t == &_timer_update_dht) {
    timer_update_dht_hook(t); 
  }
}

/**********
 *
 * Handlers
 *
 */////////

enum { HANDLER_GET_GATEWAY, HANDLER_KNOWN_GATEWAYS, HANDLER_ADD_GATEWAY, HANDLER_REMOVE_GATEWAY };

String
BRNGateway::read_handler(Element *e, void *thunk) {
  BRNGateway *gw = static_cast<BRNGateway *> (e);
  switch ((uintptr_t) thunk) {
  case HANDLER_GET_GATEWAY: {
    // return this nodes gateway metric, if listed in _known_gateways, else 0
    const BRNGatewayEntry* gwe = gw->get_gateway();
    if (gwe != NULL)
    	return gwe->s() + "\n";
    else
      return "No gateway\n";
  }
  case HANDLER_KNOWN_GATEWAYS: {
    // returns all known gateways (including this node)

    /**
     * 
     * @todo
     * Impove printing output
     * 
     */

    StringAccum sa;
    sa << "Gateway list (next update in " << gw->_timer_refresh_known_gateways.expiry() - Timestamp::now() << "s)\n"
       << "BRNGateway\t\tMetric\tIP address\tBehind NAT?\n";
    // iterate over all known gateways
    for (BRNGatewayList::const_iterator i = gw->_known_gateways.begin(); i.live(); i++) {
      BRNGatewayEntry gwe = i.value();
      sa << i.key().unparse().c_str() << "\t"
         << (uint32_t) gwe.get_metric() << "\t"
         << gwe.get_ip_address() << "\t"
         << gwe.is_behind_NAT() << "\n";
    }

    return sa.take_string();
  }
  default:
  	return "<error>\n";
  }
}

int
BRNGateway::write_handler(const String &data, Element *e, void *thunk, ErrorHandler *errh) {
  BRNGateway *gw = static_cast<BRNGateway *> (e);
  String s = cp_uncomment(data);
  switch ((intptr_t)thunk) {
  case HANDLER_ADD_GATEWAY: {

  	IPAddress ip_addr;
    int metric;
    bool nated;

    if (cp_va_space_parse(data, gw, errh,
                          cpInteger, "Gateway's metric", &metric,
                          cpIPAddress, "Gateway's IP address", &ip_addr,
                          cpBool, "Gateway behind NAT?", &nated,
                          cpEnd) < 0)
    	return errh->error("wrong arguments to handler 'add_gateway'");

    BRNGatewayEntry gwe = BRNGatewayEntry(ip_addr, metric, nated);
    return gw->add_gateway(gwe);
  }
  case HANDLER_REMOVE_GATEWAY: {
  	return gw->remove_gateway();
  }
  default:
  	return errh->error("internal error");
  }
}

void
BRNGateway::add_handlers() {
//  BRNElement::add_handlers();

  // handler to get all known gateways
  add_read_handler("known_gateways", read_handler, (void *) HANDLER_KNOWN_GATEWAYS);

  // handlers used to get and set information about this node as a gateway
  add_read_handler("get_gateway", read_handler, (void *) HANDLER_GET_GATEWAY);
  add_write_handler("add_gateway", write_handler, (void *) HANDLER_ADD_GATEWAY);
  add_write_handler("remove_gateway", write_handler, (void *) HANDLER_REMOVE_GATEWAY);
}

/***********************************************
 * 
 * methods to access the gateways data structure
 * 
 *//////////////////////////////////////////////

int
BRNGateway::add_gateway(BRNGatewayEntry gwe) {
	// TODO
	// find the gateway and update its values
	// if not found insert it
	
	BRNGatewayEntry *old_gw;
	
	if ((old_gw = _known_gateways.findp(_my_eth_addr)) != NULL)	{
		// update values since entry is known	
		
		// a metric of 0 indicates no connection
    // insert only, if metric bigger than 0
		if (gwe.get_metric() > 0) {
    	// TODO
     	// check if metric change is significant
     	// or if other data has changed
    	// generate only in this case the dht packet, else just save it locally
     	// use code from ping source
    	// add exponential averaging here


			// update values			
			*old_gw = gwe;	
	   	
  		// update dht
    	_timer_update_dht.schedule_now();
  	}
	   	
		return 0;
	}
	else {
		// and insert new entry
    if (!_known_gateways.insert(_my_eth_addr, gwe)) {
    	BRN_WARN("Could not insert gateway %s with values %s.", _my_eth_addr.unparse().c_str(), gwe.s().c_str());
      return -1;
    }
		
		// update dht
    _timer_update_dht.schedule_now();
	
    return 0;
	}
	
	return -1;
}

int
BRNGateway::remove_gateway() {

  if (remove_gateway(_my_eth_addr)) {	
    BRN_INFO("Removed gateway %s from list", _my_eth_addr.unparse().c_str());

    // delete gateway from dht
    output(0).push(remove_gateway_from_dht());
  }
  else {
    BRN_WARN("Gateway %s was already removed. Did not find it.", _my_eth_addr.unparse().c_str());
  }
    
  return 0;
}

int
BRNGateway::remove_gateway(EtherAddress eth) {
  // remove all pending flows
  _flows->remove_flows_with_gw(eth);

  // this gateway should not be updated via this method
  return _known_gateways.remove(eth);
}


const BRNGatewayEntry*
BRNGateway::get_gateway() {
  return _known_gateways.findp(_my_eth_addr);
}

const BRNGatewayList *
BRNGateway::get_gateways() {
  // return updated list
  return &_known_gateways;
}

/*******************************
 * 
 * methods for dht communication
 * 
 *//////////////////////////////

/* returns a packet for the dht to update this gateway in DHT (INSERT|WRITE) */

/**
 * Returns a DHT packet, which updates the BRNGatewayEntry for the
 * node with ether address <_my_eth_addr> (key)
 *
 * @param gwe stores the information about the gateway
 *
 * @return
 *
 */
Packet* 
BRNGateway::update_gateway_in_dht(BRNGatewayEntry gwe) {

	BRN_INFO("Sending dht update packet");
/*  WritablePacket *dht_p_out = DHTPacketUtil::new_dht_packet();
    
    
      
  //unique_id = ( ( unique_id - ( unique_id % 3 ) ) + 3 ) % 255;
	unique_id = ( ( unique_id - ( unique_id % 3 ) ) + 3 ) % 255;
	
	BRN_CHECK_EXPR_RETURN(unique_id % 3 != 0,
    	("Unique ID %u mod 3 != 0; But sould be euqal 0", unique_id), return NULL;);
	
	   
  DHTPacketUtil::set_header(dht_p_out, GATEWAY, DHT, 0, WRITE, unique_id );
  dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_STRING, strlen(DHT_KEY_GATEWAY), (uint8_t *) DHT_KEY_GATEWAY);
  dht_p_out = DHTPacketUtil::add_sub_list(dht_p_out, 1);
  dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_MAC, 6, (uint8_t *) _my_eth_addr.data() );
  dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 1, TYPE_UNKNOWN, sizeof(gwe), (uint8_t *) &gwe );
    
  return dht_p_out;*/
  return NULL;
}

/* returns a packet to remove this gateway from the DHT (REMOVE) */

/**
 * Returns a DHT packet, which removes the entry for the node
 * with ether address <_my__eth_addr> from DHT
 *
 *
 * @return
 *
 */
Packet* 
BRNGateway::remove_gateway_from_dht() {
/*
  WritablePacket *dht_p_out = DHTPacketUtil::new_dht_packet();
      
  unique_id = ( ( unique_id - ( unique_id % 3 ) ) + 4 ) % 255;
    
  BRN_CHECK_EXPR_RETURN(unique_id % 3 != 1,
    	("Unique ID %u mod 3 != 1; But sould be euqal 1", unique_id), return NULL;);
	      
  DHTPacketUtil::set_header(dht_p_out, GATEWAY, DHT, 0, REMOVE, unique_id );
  dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_STRING, strlen(DHT_KEY_GATEWAY), (uint8_t *) DHT_KEY_GATEWAY);
  dht_p_out = DHTPacketUtil::add_sub_list(dht_p_out, 1);
  dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_MAC, 6, (uint8_t *) _my_eth_addr.data() );
    
  return dht_p_out;*/
  return NULL;
}

/* handle dht reponse */

// returns a packet to get the list of available gateways from DHT (READ)
/**
 * Returns a packet to the list of gateways from the DHT.
 *
 * @return 
 *
 */
Packet* 
BRNGateway::get_getways_from_dht() {

	BRN_INFO("Sending dht get packet");

/*  WritablePacket *dht_p_out = DHTPacketUtil::new_dht_packet();
      
  unique_id = ( ( unique_id - ( unique_id % 3 ) ) + 5 ) % 255;
    
  BRN_CHECK_EXPR_RETURN(unique_id % 3 != 2,
    ("Unique ID %u mod 3 != 2; But should be equal 2", unique_id), return NULL;);
    
  DHTPacketUtil::set_header(dht_p_out, GATEWAY, DHT, 0, READ, unique_id );
  dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_STRING, strlen(DHT_KEY_GATEWAY), (uint8_t *) DHT_KEY_GATEWAY);

  return dht_p_out;*/
  return NULL;
}

/**
 * Get a response from DHT and decides whether this packet
 * is an response to an update or remove or the list of gateway,
 * which is handled by the method update_gateways_from_dht_response
 *get_gateway()
 */
void 
BRNGateway::handle_dht_response(Packet* p) {
/*
  struct dht_packet_header *dht_header = (struct dht_packet_header*)p->data();

  switch ( dht_header->id % 3 ) {
	  case 0: { //update
	    if ( dht_header->flags == 0 ) {
	     		BRN_INFO("DHT-Update OK");
	    }
	    else {
	     	BRN_WARN("DHT-Update FAILED");
	    }
	     
	    break;
	  }
    case 1: { //remove
	    if ( dht_header->flags == 0 ) {
	     		BRN_INFO("Gateway: DHT-Remove OK");
	    }
	    else {
	      BRN_ERROR("DHT-Remove FAILED");
	    }
	     
	    break;
	  }
    case 2: { //get
	    if ( dht_header->flags == 0 ) {
	      BRN_INFO("DHT-GET OK");
	      bool result = update_gateways_from_dht_response(p);
	      if (!result) {
	        BRN_ERROR("Could not update gateways from dht response");
	      }
	    }
	    else {
	    	BRN_WARN("DHT-GET FAILED; no gateway key known in DHT.");
	    }
	    break;
	  }
  	default: {
  		BRN_ERROR("Strange DHT ID %d. Fix me.", dht_header->id);
  	}
  }*/
}

// extracts the list of gateways from the dht response */

/**
 * Updates the current list of gateways from the DHT
 * response
 *
 *
 * @return true, if successful, else false
 *
 */
bool 
BRNGateway::update_gateways_from_dht_response(Packet* p) {

/*  BRN_INFO("Extracting gateways from DHT response");

  //BRNGatewayEntry new_gw;
 
  struct dht_packet_header *dht_header = (struct dht_packet_header*) p->data();

  struct dht_packet_payload *_dht_payload = DHTPacketUtil::get_payload_by_number( p, 1 ); //Sublist holen, hat nummer 1

  uint8_t *dht_data = (uint8_t*)_dht_payload;
  int sublist_len = (int)dht_data[2];
  int index = 0;
  
  EtherAddress eth_addr;
  BRNGatewayEntry gwe;

  if ( dht_header->flags != 0 ) {
    BRN_WARN("Update Gateways: DHT read failed");
    
    return false;
  }
  
  // TODO
  // clearing and inserting is expensive
  // should be done better
  _known_gateways.clear();

  for ( index = DHT_PAYLOAD_OVERHEAD; index < sublist_len; ) {
    if ( ( dht_data[index] == 0 ) && ( dht_data[index + 1] == TYPE_MAC ) ) { //found subkey
      eth_addr = EtherAddress(&dht_data[index + DHT_PAYLOAD_OVERHEAD]);
      
      index += DHT_PAYLOAD_OVERHEAD + dht_data[index + 2]; 
      
	    if (dht_data[index] == 1) {
		  	// BRNGatewayEntry
		  	// TODO handle endianess inside class BRNGatewayEntry with htons and so on
		  	
		  	// TODO can this be done in C++ style
		  	memcpy(&gwe, &dht_data[index + DHT_PAYLOAD_OVERHEAD], sizeof(gwe));
		  	
		  	//gwe = (BRNGatewayEntry) dht_data[index + DHT_PAYLOAD_OVERHEAD];
		  	BRN_INFO("Found gateway %s in dht response with values %s", eth_addr.unparse().c_str(), gwe.s().c_str());
	  	}
	  	else {
		  	BRN_ERROR("BRNGateway: Not supported");
		  	return false;
	  	}

    	// inefficient but right sematics; see TODO at beginning of method
    	if (!_known_gateways.insert(eth_addr, gwe))
    		return false;	
    	else {
      	index += DHT_PAYLOAD_OVERHEAD + dht_data[ index + 2 ]; // jump
    	}
  	}
  }*/
  return true;
}

EXPORT_ELEMENT(BRNGateway)
#include <click/bighashmap.cc>
CLICK_ENDDECLS
