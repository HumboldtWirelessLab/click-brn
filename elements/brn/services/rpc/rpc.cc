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
#include "rpc.hh"

#include "elements/brn/dht/storage/dhtoperation.hh"
#include "elements/brn/dht/storage/dhtstorage.hh"
#include "elements/brn/dht/storage/rangequery/ip_rangequery.hh"

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
            call_function(_pending_rpcs[i]);
            _pending_rpcs.erase(_pending_rpcs.begin() + i);
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
  result = datatype2string(rpcdhth->result_type);

  uint8_t *result_type_list = (uint8_t*)&rpcdhth[1];
  for (int i = 0; i < rpcdhth->args_count; i++) args.push_back(datatype2string(result_type_list[i]));

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

  if ( _tcc->_func_map.findp(function) == NULL ) {

    _pending_rpcs.push_back(params);

    DHTOperation *req = new DHTOperation();

    req->read((uint8_t*)function.data(), function.length());

    dht_request(req);

  } else {
    res = _tcc->call_function(function,args);
  }

  return res;
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
