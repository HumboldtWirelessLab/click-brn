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
#include <click/string.hh>

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
  _update_dht_interval(DEFAULT_UPDATE_DHT_INTERVAL)
{
}

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
                  "UPDATE_GATEWAYS_INTERVAL", cpkP+cpkM, cpSeconds, &_update_gateways_interval,
                  "UPDATE_DHT_INTERVAL", cpkP+cpkM, cpSeconds, &_update_dht_interval,
                  "DHTSTORAGE", cpkP+cpkM, cpElement, &_dht_storage,
                  "SETGATEWAYONFLOW", cpkP, cpElement, &_flows,
                  "DEBUG", cpkP, cpInteger, &_debug,
                  cpEnd) < 0)
    return -1;

  if (_update_gateways_interval == 0)
    errh->error("UPDATE_GATEWAYS_INTERVAL is zero");

  if (_update_dht_interval == 0)
    errh->error("UPDATE_GATEWAYS_INTERVAL is zero");

/*  if (_flows->cast("BRNSetGatewayOnFlow") == 0) {
    return errh->error("No element of type BRNSetGatewayOnFlow specified.");
  }*/

  return 0;
}

int
BRNGateway::initialize(ErrorHandler *errh) {
  click_srandom(_my_eth_addr.hashcode());
  (void) (errh);

  _timer_update_dht.initialize(this);
  _timer_update_dht.reschedule_after_sec(_update_dht_interval);

  _timer_refresh_known_gateways.initialize(this);
  _timer_refresh_known_gateways.reschedule_after_sec(_update_gateways_interval);

  pending_remove = false;
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

/********
 *
 * Timers
 *
 *
 *///////
void
BRNGateway::timer_refresh_known_gateways_hook(Timer *t) {
  BRN_INFO("Timer executed to get update list of gateways");
  get_getways_from_dht(NULL,NULL);                     // request ...
  t->reschedule_after_sec(_update_gateways_interval);  // ... and reschdule timer
}

void
BRNGateway::timer_update_dht_hook(Timer */*t*/) {
  BRN_INFO("Timer executed to get update this gateway");
  // is this node a gateway
  const BRNGatewayEntry *gwe = get_gateway();

  if ( pending_remove )
    remove_gateway_from_dht(NULL,NULL);
  else
    if (gwe != NULL) update_gateway_in_dht(NULL,NULL,*gwe); // output packet, if this node is a gateway

  // ... and reschdule timer
//  t->reschedule_after_sec(_update_dht_interval);
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
    sa << "Gateway list (" << gw->_known_gateways.size() <<") (next update in " << gw->_timer_refresh_known_gateways.expiry() - Timestamp::now() << "s)\n"
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

    Vector<String> args;
    cp_spacevec(s, args);
    cp_integer(args[0], &metric);
    cp_ip_address(args[1],&ip_addr);
    cp_bool(args[2],&nated);

    /*if (cp_va_kparse(data, gw, errh,
        "GATEWAYMETRIC", cpkP, cpInteger, &metric,
        "GATEWAYIPADDRESS", cpkP, cpIPAddress, &ip_addr,
        "GATEWAYBEHINDNAT", cpkP, cpBool, &nated,
        cpEnd) < 0)
      return errh->error("wrong arguments to handler 'add_gateway'");
    */
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
  } else {
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
    remove_gateway_from_dht(NULL,NULL);
  } else {
    BRN_WARN("Gateway %s was already removed. Did not find it.", _my_eth_addr.unparse().c_str());
  }

  return 0;
}

int
BRNGateway::remove_gateway(EtherAddress eth) {
  if ( _flows != NULL ) {
    // remove all pending flows
    _flows->remove_flows_with_gw(eth);
  }

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

void
BRNGateway::dht_callback_func(void *e, DHTOperation *op)
{
  BRNGateway *s = (BRNGateway *)e;
  BRNGateway::RequestInfo *request_info = s->get_request_by_dht_id(op->get_id());

  if ( request_info != NULL ) {
    s->handle_dht_reply(request_info,op);
  } else {
    click_chatter("BRN-Gateway: No request info for DHT-ID");
    delete op;
  }
}

void
BRNGateway::dht_request(RequestInfo *request_info, DHTOperation *op)
{
  BRN_DEBUG("New request: %d",request_list.size());
  BRN_DEBUG("NEW VALUELEN: %d",op->header.valuelen);
  uint32_t result = _dht_storage->dht_request(op, dht_callback_func, (void*)this );

  request_info->_id = result;
  request_list.push_back(request_info);

  if ( result == 0 )
  {
    BRN_INFO("Got direct-reply (local)");
    handle_dht_reply(request_info, op);
  }
}

void
BRNGateway::handle_dht_reply(RequestInfo *request_info, DHTOperation *op)
{
  //int result;
  remove_request(request_info);
  
  BRN_DEBUG("BRNGateway: Handle DHT-Answer: %d",request_info->_mode);

  switch ( request_info->_mode ) {
    case GRM_READ:
      get_getways_from_dht(request_info, op);
      break;
    case GRM_READ_BEFORE_UPDATE:
    case GRM_UPDATE:
      BRN_DEBUG("Handle Update");
      update_gateway_in_dht(request_info, op, request_info->_gw_entry);
      break;
    case GRM_READ_BEFORE_REMOVE:
    case GRM_REMOVE:
      remove_gateway_from_dht(request_info, op);
      break;
    default:
      BRN_WARN("UNKNOWN mode");
      break;
  }

  delete op;
  delete request_info;
}

BRNGateway::RequestInfo *
BRNGateway::get_request_by_dht_id(uint32_t id)
{
  BRN_DEBUG("RequestList Size: %d",request_list.size());
  for ( int i = 0; i < request_list.size(); i++ )
    if ( request_list[i]->_id == id ) return( request_list[i] );

  return( NULL );

}

int
BRNGateway::remove_request(RequestInfo *request_info)
{
  for ( int i = request_list.size()-1; i >=0 ; i-- )
    if ( request_list[i]->_id == request_info->_id ) {
      request_list.erase(request_list.begin() + i);
      BRN_DEBUG("Remove request: %d",request_list.size());
    }

  return(0);
}


struct brn_gateway_dht_entry*
BRNGateway::get_gwe_from_value(uint8_t *value, int valuelen, BRNGatewayEntry */*gwe*/)
{
  struct brn_gateway_dht_entry* gwentries = (struct brn_gateway_dht_entry*)value;
  int count_entries = valuelen / sizeof(struct brn_gateway_dht_entry);

  for(int i = 0; i < count_entries; i++ ) {
    if ( memcmp(gwentries[i].etheraddress, _my_eth_addr.data(),6) == 0 ) return (struct brn_gateway_dht_entry*)&(gwentries[i]);
  }

  return NULL;
}
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
void
BRNGateway::update_gateway_in_dht(RequestInfo *request_info, DHTOperation *req, BRNGatewayEntry gwe) {


  if ( request_info == NULL ) {
    BRN_INFO("Sending read packet");
    DHTOperation *new_req = new DHTOperation();
    RequestInfo *new_request_info = new RequestInfo(GRM_READ_BEFORE_UPDATE);

    new_req->read((uint8_t*)DHT_KEY_GATEWAY, strlen(DHT_KEY_GATEWAY));
    new_req->max_retries = 2;
    new_req->set_replica(0);
    new_req->set_lock(true);

    new_request_info->_gw_entry = gwe;

    dht_request(new_request_info, new_req);
  } else {
    if ( request_info->_mode == GRM_READ_BEFORE_UPDATE ) {
      if ( req->header.status == DHT_STATUS_OK ) {
        BRN_DEBUG("UPDATE GATEWAY");
        struct brn_gateway_dht_entry* gwentries = get_gwe_from_value(req->value, req->header.valuelen, &gwe);

        DHTOperation *new_req = new DHTOperation();
        RequestInfo *new_request_info = new RequestInfo(GRM_UPDATE);

        if ( gwentries == NULL ) {
          BRN_DEBUG("My Gateway (%s) is new",gwe.s().c_str());
          uint8_t new_value[req->header.valuelen + sizeof(struct brn_gateway_dht_entry)];
          memcpy(new_value, req->value, req->header.valuelen);
          gwentries = (struct brn_gateway_dht_entry*)&(new_value[req->header.valuelen]);
          memcpy(gwentries->etheraddress,_my_eth_addr.data(),6);
          gwentries->ipv4 = htonl(gwe._ip_addr);
          gwentries->bandwidth = 0;
          gwentries->metric = gwe._metric;
          if ( gwe._nated ) gwentries->flags = GATEWAY_FLAG_NATED;
          else gwentries->flags = 0;
          BRN_DEBUG("NEW VALUE: %d",(req->header.valuelen + sizeof(struct brn_gateway_dht_entry)));
          new_req->write((uint8_t*)DHT_KEY_GATEWAY, strlen(DHT_KEY_GATEWAY),
                          new_value, (req->header.valuelen + sizeof(struct brn_gateway_dht_entry)), false);

          BRN_DEBUG("NEW VALUE2: %d",new_req->header.valuelen);
        } else {
          BRN_DEBUG("Update Gateway");
          gwentries->metric = gwe._metric;
          if ( gwe._nated ) gwentries->flags = GATEWAY_FLAG_NATED;
          else gwentries->flags = 0;
          new_req->write((uint8_t*)DHT_KEY_GATEWAY, strlen(DHT_KEY_GATEWAY), req->value, req->header.valuelen, false);

        }

        new_req->max_retries = 2;
        new_req->set_replica(0);
        new_req->set_lock(false);
        dht_request(new_request_info, new_req);

      } else if ( req->header.status == DHT_STATUS_KEY_NOT_FOUND ) {
        BRN_DEBUG("First Gateway");

        DHTOperation *new_req = new DHTOperation();
        RequestInfo *new_request_info = new RequestInfo(GRM_UPDATE);

        struct brn_gateway_dht_entry gwentries;
        memcpy(gwentries.etheraddress,_my_eth_addr.data(),6);
        gwentries.ipv4 = htonl(gwe._ip_addr);
        gwentries.bandwidth = 0;
        gwentries.metric = gwe._metric;
        if ( gwe._nated ) gwentries.flags = GATEWAY_FLAG_NATED;
        else gwentries.flags = 0;
        new_req->write((uint8_t*)DHT_KEY_GATEWAY, (uint16_t)strlen(DHT_KEY_GATEWAY), (uint8_t*)&gwentries, (uint16_t)sizeof(gwentries), true);

        new_req->max_retries = 2;
        new_req->set_replica(0);
        new_req->set_lock(false);
        dht_request(new_request_info, new_req);

      } else { //TODO: restart request ???
        BRN_DEBUG("Gateway: send request again");
        _timer_update_dht.reschedule_after_sec(click_random() % _update_dht_interval);
      }
    } else {
      BRN_INFO("Gateway update finished. Status: %d",req->header.status);
    }
  }
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
void
BRNGateway::remove_gateway_from_dht(RequestInfo *request_info, DHTOperation *req ) {

  if ( request_info == NULL ) {
    DHTOperation *new_req = new DHTOperation();
    RequestInfo *new_request_info = new RequestInfo(GRM_READ_BEFORE_REMOVE);

    new_req->read((uint8_t*)DHT_KEY_GATEWAY, strlen(DHT_KEY_GATEWAY));
    new_req->max_retries = 2;
    new_req->set_replica(0);
    new_req->set_lock(true);
    dht_request(new_request_info,new_req);
    pending_remove = false;
  } else {
    if ( request_info->_mode == GRM_REMOVE ) {
      pending_remove = false;
      return;
    }

    switch (req->header.status) {
      case DHT_STATUS_KEY_IS_LOCKED:
        pending_remove = true;
        _timer_update_dht.reschedule_after_sec(click_random() % _update_dht_interval);
        break;
      case DHT_STATUS_OK:
        struct brn_gateway_dht_entry* gwentries = (struct brn_gateway_dht_entry*)req->value;
        int count_entries = req->header.valuelen / sizeof(struct brn_gateway_dht_entry);

        for(int i = 0; i < count_entries; i++ ) {
          if ( memcmp(gwentries[i].etheraddress, _my_eth_addr.data(),6) == 0 ) {
            if ( i < ( count_entries - 1 ) )
              memcpy(&(req->value[i*sizeof(struct brn_gateway_dht_entry)]),
                     &(req->value[(i+1)*sizeof(struct brn_gateway_dht_entry)]),
                       sizeof(struct brn_gateway_dht_entry)*(count_entries-1-i));

            req->header.valuelen -= sizeof(struct brn_gateway_dht_entry);
            break;
          }
        }

        DHTOperation *new_req = new DHTOperation();
        RequestInfo *new_request_info = new RequestInfo(GRM_REMOVE);
        new_req->write((uint8_t*)DHT_KEY_GATEWAY, (uint16_t)strlen(DHT_KEY_GATEWAY), req->value, req->header.valuelen, false);
        new_req->max_retries = 2;
        new_req->set_replica(0);
        new_req->set_lock(false);
        dht_request(new_request_info, new_req);
        break;
    }
  }
}

/* handle dht reponse */

// returns a packet to get the list of available gateways from DHT (READ)
/**
 * Returns a packet to the list of gateways from the DHT.
 *
 * @return 
 *
 */
void
BRNGateway::get_getways_from_dht(RequestInfo *request_info, DHTOperation *req)
{

  BRN_INFO("Sending dht get packet");
  if ( request_info == NULL ) {
    DHTOperation *req = new DHTOperation();
    RequestInfo *request_info = new RequestInfo(GRM_READ);

    req->read((uint8_t*)DHT_KEY_GATEWAY, strlen(DHT_KEY_GATEWAY));
    req->max_retries = 2;
    req->set_replica(0);
    dht_request(request_info,req);
  } else {
    BRN_INFO("GOT all bateways");
    switch ( req->header.status ) {
      case DHT_STATUS_OK:
        update_gateways_from_dht_response(req->value, req->header.valuelen);
        break;
      case DHT_STATUS_KEY_NOT_FOUND:
        BRN_DEBUG("GATEWAYKEY not found");
        break;
      case DHT_STATUS_TIMEOUT: 
        BRN_DEBUG("Timeout not found");
        break;
      default:
        BRN_DEBUG("Get gateway: other errors");
        break;
    }
  }
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
BRNGateway::update_gateways_from_dht_response(uint8_t *value, uint32_t valuelen) {

  BRN_INFO("Extracting gateways from DHT response: %d", valuelen);
  // TODO
  // clearing and inserting is expensive
  // should be done better
  bool lgw = false;
  BRNGatewayEntry copy_lgw;
  const BRNGatewayEntry* myself = get_gateway();
  if ( myself != NULL ) {
    copy_lgw = *myself;
    lgw = true;
  }

  _known_gateways.clear();

  struct brn_gateway_dht_entry* gwentries = (struct brn_gateway_dht_entry*)value;

  int count_entries = valuelen / sizeof(struct brn_gateway_dht_entry);

  for( int i = 0; i < count_entries; i++ ) {
    BRNGatewayEntry gwe = BRNGatewayEntry(IPAddress(ntohl(gwentries[i].ipv4)), gwentries[i].metric, ((gwentries[i].flags & GATEWAY_FLAG_NATED) == GATEWAY_FLAG_NATED ));
    BRN_DEBUG("NATEDFLAGS: %d",gwentries[i].flags);
    EtherAddress eth_addr = EtherAddress(gwentries[i].etheraddress);
    if ( memcmp(eth_addr.data(), _my_eth_addr.data(),6 ) != 0 ) {
      BRN_INFO("Found gateway %s in dht response with values %s", eth_addr.unparse().c_str(), gwe.s().c_str());
      _known_gateways.insert(eth_addr, gwe);
    }
  }

  if ( lgw ) {
    BRN_INFO("Found gateway %s in dht response with values %s", _my_eth_addr.unparse().c_str(), copy_lgw.s().c_str());
    _known_gateways.insert(_my_eth_addr, copy_lgw);
  }

  return true;
}

EXPORT_ELEMENT(BRNGateway)
CLICK_ENDDECLS
