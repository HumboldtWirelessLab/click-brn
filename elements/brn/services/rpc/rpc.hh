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

#ifndef CLICK_RPC_HH
#define CLICK_RPC_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/vector.hh>
#include <click/timer.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/dht/storage/dhtstorage.hh"
#include "elements/brn/services/system/tcc.hh"

CLICK_DECLS

struct rpc_dht_header {
  uint8_t flags;
  uint8_t result_type;
  uint8_t args_count;
  uint8_t compute_node_count;
  uint16_t code_size;
};

#define RPC_REQUEST_OR_REPLY (1 << 0)
#define RPC_CALLTYPE         (1 << 1)
#define RPC_REQUEST       0
#define RPC_REPLY         1
#define RPC_DATA_CALL     0
#define RPC_FUNCTION_CALL 2

#define RPC_TYPE             (RPC_REQUEST_OR_REPLY | RPC_CALLTYPE)
#define RPC_DATA_REQUEST     (RPC_REQUEST | RPC_DATA_CALL)
#define RPC_DATA_REPLY       (RPC_REPLY | RPC_DATA_CALL)
#define RPC_FUNCTION_REQUEST (RPC_REQUEST | RPC_FUNCTION_CALL)
#define RPC_FUNCTION_REPLY   (RPC_REPLY | RPC_FUNCTION_CALL)


struct rpc_function_call_header {
  uint8_t flags;        //request/reply,....
  uint8_t request_size; //size of request (function/handler name
  uint16_t result_size;
  uint16_t request_id;
};


#define RPC_TCC_DFLT_CHECK_TIMER_INTERVAL 5000

class RPC : public BRNElement {

  class RPCAddress {
   public:

    Timestamp _last_update;
    String _uri;                  //just handler/function
    Vector<EtherAddress> _node;
  };

  class RPCInfo {
   public:
    Timestamp _last_update;
    String _name;                 //handler or function name (full, including params)
    RPCAddress _addr;             //RPC Address
    Vector<EtherAddress> _rpc_src;
    Vector<RPCInfo*> _rpc_dep;
  };

 public:

  //
  //methods
  //
  RPC();
  ~RPC();

  const char *class_name() const	{ return "RPC"; }
  const char *processing() const	{ return PUSH; }

  const char *port_count() const  { return "1/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }
  void add_handlers();

  void run_timer(Timer *t);

  int initialize(ErrorHandler *);

  void push( int port, Packet *packet );

  int handle_dht_reply(DHTOperation *op);

  int call_function(String params);
  int call_remote_function(String params);
  String get_handler_value(String full_handler_name);

  void request_data(const EtherAddress ea, String handler);
  void handle_request_data(Packet *p);
  void handle_reply_data(Packet *p);

  void request_function(const EtherAddress ea, String handler);
  void handle_request_function(Packet *p);
  void send_reply_function(Packet *p);
  void handle_reply_function(Packet *p);

  String get_result();

 private:

  Timer _tcc_check_timer;
  uint32_t _tcc_check_interval;
  HashMap<String, Timestamp> _known_tcc_function;
  HashMap<String, RPCAddress*> _n_known_tcc_function;

  void dht_request(DHTOperation *op);

  uint8_t *tcc2dht(String fname, int * vdata_size);
  int dht2tcc(String fname, uint8_t *data, int data_size);

  TCC *_tcc;
  DHTStorage *_dht_storage;

  Vector<String> _pending_rpcs;

  HashMap<String, String> _pending_params;

  Vector<RPCInfo*> _n_pending_rpcs;

  HashMap<String, RPCInfo*> _n_pending_rpc_map;

  //int add_rpc(String object, String name, Vector<String> out, Vector<String> in, String config); //real function
};

CLICK_ENDDECLS
#endif
