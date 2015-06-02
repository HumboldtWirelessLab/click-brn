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
 * arp.{cc,hh}
 */

#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <click/router.hh>
#include <click/handlercall.hh>

#include "rpc.hh"

#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"

#include "elements/brn/dht/storage/dhtoperation.hh"
#include "elements/brn/dht/storage/dhtstorage.hh"

CLICK_DECLS

RPC::RPC()
: _tcc_check_timer(this),
  _tcc_check_interval(RPC_TCC_DFLT_CHECK_TIMER_INTERVAL),
  _dht_storage(NULL)
{
  _known_tcc_function.clear();
  _pending_rpcs.clear();
}

RPC::~RPC()
{
}

int
RPC::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
    "TCC", cpkP+cpkM, cpElement, &_tcc,
    "DHTSTORAGE", cpkP, cpElement, &_dht_storage,
    "DEBUG", cpkP, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  return 0;
}

int
RPC::initialize(ErrorHandler *)
{
  _tcc_check_timer.initialize(this);

  _tcc_check_timer.schedule_after_msec(_tcc_check_interval);

  return 0;
}

void
RPC::run_timer(Timer *t)
{
  //BRN_DEBUG("Run timer.");

  if ( t == NULL ) BRN_ERROR("Timer is NULL");

  _tcc_check_timer.schedule_after_msec(_tcc_check_interval);

  if ( _dht_storage == NULL ) return;

  Timestamp now = Timestamp::now();

  for (TCC::TccFunctionMapIter fm = _tcc->_func_map.begin(); fm.live(); ++fm) {
    String name = fm.key();
    if ( _known_tcc_function.findp(name) == NULL ) {
      BRN_INFO("Found new function: %s",name.c_str());

      _known_tcc_function.insert(name,now);

      DHTOperation *req = new DHTOperation();

      int dht_data_size;
      uint8_t *dht_data = tcc2dht(name, &dht_data_size);

      req->insert((uint8_t*)name.data(), name.length(), dht_data, dht_data_size);

      dht_request(req);

    }
  }
}

static void callback_func(void *e, DHTOperation *op)
{
  RPC *s = (RPC *)e;
  s->handle_dht_reply(op);
}

void
RPC::dht_request(DHTOperation *op)
{
  BRN_DEBUG("send dht request");
  uint32_t result = _dht_storage->dht_request(op, callback_func, (void*)this );

  if ( result == 0 )
  {
    BRN_DEBUG("Got local reply");
    handle_dht_reply(op);
  }
}


void
RPC::push( int port, Packet *packet )
{
  (void)port;
  (void)packet;

  BRN_DEBUG("got packet");
  struct rpc_dht_header *rpcdhth = (struct rpc_dht_header *)packet->data();

  if ( rpcdhth->flags & RPC_FUNCTION_CALL_REPLY ) {   //handle replies
    if ( rpcdhth->flags & RPC_FUNCTION_DATA_CALL ) {  //handle data reply
      BRN_DEBUG("Reply");
      handle_reply_data(packet);
    }
  } else {
    if ( rpcdhth->flags & RPC_FUNCTION_DATA_CALL ) {  //handle data reply
      handle_request_data(packet);
    }
  }
}

int
RPC::handle_dht_reply(DHTOperation *op)
{
  BRN_DEBUG("Handle DHT-Reply");
  /* Test: ip found ?*/	

  if ( op->header.status == DHT_STATUS_KEY_NOT_FOUND ) {
    BRN_DEBUG("DHT Error !!!");
  } else {
    if ( op->is_reply() ) {
      String key = String(op->key, op->header.keylen);

      if ( (op->header.operation & ( (uint8_t)~((uint8_t)OPERATION_REPLY))) == OPERATION_INSERT ) {
        BRN_DEBUG("Insert was successful. Function: %s", key.c_str());
      } else if ( (op->header.operation & ( (uint8_t)~((uint8_t)OPERATION_REPLY))) == OPERATION_READ ) {
        BRN_DEBUG("Read was successful. Function: %s", key.c_str());
        _known_tcc_function.insert(key,Timestamp::now());
        dht2tcc(key,op->value, op->header.valuelen);
        for( int i = _pending_rpcs.size()-1; i >= 0; i-- ) {
          Vector<String> args;
          cp_spacevec(_pending_rpcs[i], args);

          if ( args[0] == key ) {
            String pending_rpc = _pending_rpcs[i];
            _pending_rpcs.erase(_pending_rpcs.begin() + i);
            call_function(pending_rpc);
          }
        }
      }
    }
  }

  delete op;
  return(0);
}

uint8_t *
RPC::tcc2dht(String fname, int *data_size)
{
  TCC::TccFunction* tccf = _tcc->_func_map.find(fname);

  int size = sizeof(struct rpc_dht_header) + tccf->_args.size() + tccf->_c_code.length();
  uint8_t *tcc_func_buf = new uint8_t[size];

  struct rpc_dht_header *rpcdhth = (struct rpc_dht_header *)tcc_func_buf;

  rpcdhth->flags = 0;
  rpcdhth->result_type = tccf->_result.type;
  rpcdhth->args_count = tccf->_args.size();
  rpcdhth->compute_node_count = 0;
  rpcdhth->code_size = tccf->_c_code.length();

  uint8_t *result_type_list = (uint8_t*)&rpcdhth[1];
  for (int i = 0; i < tccf->_args.size(); i++) result_type_list[i] = (uint8_t)(tccf->_args[i].type);

  memcpy(&(result_type_list[tccf->_args.size()]), tccf->_c_code.data(), tccf->_c_code.length());

  *data_size = size;

  return tcc_func_buf;
}

/**
 * TODO: use data_size to test whether dht reply is valid
 **/
int
RPC::dht2tcc(String fname, uint8_t *data, int /*data_size*/)
{
  int res = 0;

  Vector<String> args;
  String result;
  String code;

  struct rpc_dht_header *rpcdhth = (struct rpc_dht_header *)data;
  result = TCC::datatype2string(rpcdhth->result_type);

  uint8_t *result_type_list = (uint8_t*)&rpcdhth[1];
  for (int i = 0; i < rpcdhth->args_count; i++) args.push_back(TCC::datatype2string(result_type_list[i]));

  code = String(&(result_type_list[rpcdhth->args_count]),rpcdhth->code_size);

  res = _tcc->add_function(fname, result, args);
  res = _tcc->add_code(fname, code);

  return res;
}

int
RPC::call_function(String params)
{
  int res = 0;

  Vector<String> args;
  cp_spacevec(params, args);

  String function = args[0];
  args.erase(args.begin());

  int pending_params = 0;

  if ( _tcc->_func_map.findp(function) == NULL ) {

    _pending_rpcs.push_back(params);

    DHTOperation *req = new DHTOperation();

    req->read((uint8_t*)function.data(), function.length());

    dht_request(req);

  } else { //TODO: send request for functions and params parallel
    Vector<String> finished_params;

    /* check each arg */
    for (int i = 0; i < args.size(); i++) {

      BRN_DEBUG("handler has : ? (with addr)");
      int last_colon = args[i].find_right(':',args[i].length());
      if ( last_colon > 0 ) {
        if ( _pending_params.findp(args[i]) != NULL ) {  // we already search for params (data request)
          String pp = _pending_params.find(args[i]);
          if (pp.length() > 0) {                         // and we got a result
            finished_params.push_back(args[i]);
            args[i] = pp;
          } else {                                       // we are waiting fpor some results
            pending_params++;
          }
        } else {
          BRN_DEBUG("with colon");
          String ea_string = args[i].substring(0,last_colon);
          EtherAddress ea;
          cp_ethernet_address(ea_string, &ea);
          String full_handler_name = args[i].substring(last_colon+1, args[i].length()-1);
          BRN_DEBUG("Ask %s (%s) for %s",ea.unparse().c_str(), ea_string.c_str(), full_handler_name.c_str());

          request_data(ea, full_handler_name);
          pending_params++;
          _pending_params.insert(args[i],String());
        }
      } else {
        args[i] = get_handler_value(args[i]);
        BRN_ERROR(">%s<",args[i].c_str());
      }

    }

    if ( pending_params == 0 ) {
      for( int i = finished_params.size()-1; i >= 0; i-- ) _pending_params.remove(finished_params[i]);
      finished_params.clear();

      res = _tcc->call_function(function,args);

    } else {
      _pending_rpcs.push_back(params);
      res = pending_params;
    }
  }

  return res;
}

String
RPC::get_handler_value(String full_handler_name)
{
  String result = full_handler_name;
  Vector<String> arg_nospace;

  /* find element prefix: e.g. foo/bar -> foo/ */
  String element_name = router()->ename(this->eindex());
  int last_slash = element_name.find_right('/',element_name.length());

  String element_class_prefix = String();
  if ( last_slash > 0 ) element_class_prefix = element_name.substring(0,last_slash+1);

  int handler_point = full_handler_name.find_right('.',full_handler_name.length());

  if ( handler_point > 0 ) {
    BRN_DEBUG("arg has point. handler?");
    String element_name = full_handler_name.substring(0,handler_point);
    String handler_name = full_handler_name.substring(handler_point+1,full_handler_name.length()-1);

    BRN_DEBUG("Check Handler: %s . %s", element_name.c_str(), handler_name.c_str());

    Element* s_element = router()->find(element_name);
    if (s_element != NULL) {
      result = HandlerCall::call_read(full_handler_name,
                                        router()->root_element(), ErrorHandler::default_handler());
    } else {
      s_element = router()->find(element_class_prefix + element_name);
      if (s_element != NULL)
        result = HandlerCall::call_read(element_class_prefix + full_handler_name,
                                        router()->root_element(), ErrorHandler::default_handler());
    }
    //remove linebreak
    cp_spacevec(result, arg_nospace);
    result = arg_nospace[0];
  }

  return result;
}

void
RPC::request_data(const EtherAddress ea, String handler)
{
  BRN_DEBUG("Request: %s:%s (%d)",ea.unparse().c_str(), handler.c_str(), handler.length());
  WritablePacket *p = WritablePacket::make( 256, NULL,
                                sizeof(struct rpc_function_call_header)+handler.length(), 32);

  struct rpc_function_call_header *fch = (struct rpc_function_call_header*)p->data();
  fch->flags = 0 | RPC_FUNCTION_DATA_CALL;
  fch->request_size = handler.length();
  fch->result_size = 0;
  fch->request_id = 0;

  memcpy(&(fch[1]), handler.data(),handler.length());

  WritablePacket *p_out = BRNProtocol::add_brn_header(p, BRN_PORT_RPC, BRN_PORT_RPC, DEFAULT_TTL, 0);
  BRN_ERROR("Add annos: %s %s",EtherAddress().unparse().c_str(),ea.unparse().c_str());
  BRNPacketAnno::set_ether_anno(p_out, EtherAddress(), ea, ETHERTYPE_BRN);

  output(0).push(p_out);
}

void
RPC::handle_request_data(Packet *p)
{
  EtherAddress src = BRNPacketAnno::src_ether_anno(p);
  EtherAddress dst = BRNPacketAnno::dst_ether_anno(p);
  struct rpc_function_call_header *fch = (struct rpc_function_call_header*)p->data();

  String handler = String((char*)&(fch[1]), fch->request_size);
  String result = get_handler_value(handler);

  WritablePacket *p_out = p->put(result.length());
  fch = (struct rpc_function_call_header*)p_out->data();
  fch->flags |= RPC_FUNCTION_CALL_REPLY;
  fch->result_size = htons(result.length());

  BRN_DEBUG("Handle request: %s (%d) -> %s (%d)",handler.c_str(), fch->request_size, result.c_str(), result.length());
  BRN_DEBUG("Int: %d", (int)(result.data()[0]));

  memcpy(((char*)&(fch[1])) + fch->request_size, &(result.data()[0]),result.length());

  BRNProtocol::add_brn_header(p_out, BRN_PORT_RPC, BRN_PORT_RPC, DEFAULT_TTL, 0);
  BRN_ERROR("Add annos: %s %s", dst.unparse().c_str(), src.unparse().c_str());
  BRNPacketAnno::set_ether_anno(p_out, dst, src , ETHERTYPE_BRN);

  output(0).push(p_out);
}

void
RPC::handle_reply_data(Packet *p)
{
  EtherAddress src = BRNPacketAnno::src_ether_anno(p);
  struct rpc_function_call_header *fch = (struct rpc_function_call_header*)p->data();

  String handler = String((char*)&(fch[1]), fch->request_size);
  String result = String(((char*)&(fch[1])) + fch->request_size, ntohs(fch->result_size));

  BRN_DEBUG("%s -> %s (%d)",(src.unparse() + ":" + handler).c_str(), result.c_str(), ntohs(fch->result_size));
  _pending_params.insert(src.unparse() + ":" + handler, result);

  for( int i = _pending_rpcs.size()-1; i >= 0; i-- ) {
    if ( call_function(_pending_rpcs[i]) != 0 ) {
       _pending_rpcs.erase(_pending_rpcs.begin() + i);
    }
  }
}

String
RPC::get_result()
{
  return _tcc->_last_result;
}

enum { H_STATS, H_CALL_PROCEDURE, H_RESULT };

static String RPC_read_param(Element *e, void *thunk) {
  StringAccum sa;
  RPC *rpc = (RPC *)e;

  switch((uintptr_t) thunk) {
    case H_RESULT: return rpc->get_result();
  }

  return sa.take_string();
}


static int RPC_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *) {

  RPC *rpc = (RPC *)e;
  String s = cp_uncomment(in_s);

  switch((intptr_t)vparam) {
    case H_CALL_PROCEDURE:
      rpc->call_function(s);
      break;
  }

  return 0;
}

void
RPC::add_handlers()
{
  BRNElement::add_handlers();

  add_write_handler("call", RPC_write_param, (void *)H_CALL_PROCEDURE);

  add_read_handler("result", RPC_read_param, (void *)H_RESULT);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RPC)
