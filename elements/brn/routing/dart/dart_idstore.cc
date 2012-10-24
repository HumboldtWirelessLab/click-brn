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
#include <click/error.hh>
#include <click/confparse.hh>

#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "elements/brn/dht/standard/dhtnode.hh"

#include "elements/brn/dht/storage/dhtoperation.hh"
#include "elements/brn/dht/storage/dhtstorage.hh"
#include "elements/brn/dht/routing/dart/dart_routingtable.hh"

#include "dart_protocol.hh"
#include "dart_idstore.hh"

CLICK_DECLS

/* constructor initalizes timer, ... */
DartIDStore::DartIDStore()
  : _me(NULL),
    _dht_storage(NULL),
    _drt(NULL),
    _debug(BrnLogger::DEFAULT)
{
}

/* destructor processes some cleanup */
DartIDStore::~DartIDStore()
{
}

/* called by click at configuration time */
int
DartIDStore::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "DHTSTORAGE", cpkP+cpkM, cpElement, &_dht_storage,
      "DRT", cpkP+cpkM, cpElement, &_drt,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("BRN2NodeIdentity")) 
    return errh->error("NodeIdentity not specified");

  return 0;
}

int
DartIDStore::initialize(ErrorHandler */*errh*/)
{
  _drt->add_update_callback(routingtable_callback_func, this);
  return 0;
}

void
DartIDStore::uninitialize()
{
}

/*************************************************************************************************/
/*************************** R O U T I N G - C A L L B A C K *************************************/
/*************************************************************************************************/

void
DartIDStore::routingtable_callback_func(void *e, int /*status*/)
{
  DartIDStore *s = (DartIDStore *)e;
  s->store_nodeid();
}

void
DartIDStore::store_nodeid()
{
  DHTOperation *dhtop;
  int result;

  struct dht_nodeid_entry id_entry;

  id_entry._id_length = htonl(_drt->_me->_digest_length);
  memcpy(id_entry._nodeid, _drt->_me->_md5_digest, MAX_NODEID_LENTGH);

  dhtop = new DHTOperation();

  BRN_DEBUG("Insert EtherAddress: %s",_me->getMasterAddress()->unparse().c_str());

  dhtop->write((uint8_t*)_me->getMasterAddress()->data(), 6/*Size of EtherAddress*/, (uint8_t*)&id_entry, sizeof(struct dht_nodeid_entry), true); //if exist: OVERWRITE !!
  dhtop->max_retries = 3;

  result = _dht_storage->dht_request(dhtop, callback_func, (void*)this );

  if ( result == 0 )
  {
    BRN_DEBUG("Got direct-reply (local)");
    callback_func((void*)this, dhtop);
  }
}

/*************************************************************************************************/
/******************************* D H T - C A L L B A C K *****************************************/
/*************************************************************************************************/

void
DartIDStore::callback_func(void *e, DHTOperation *op)
{
  DartIDStore *s = (DartIDStore *)e;
  s->callback(op);
}

void
DartIDStore::callback(DHTOperation *op)
{
  BRN_DEBUG("callback %s: Status %d",class_name(),op->header.operation);

  if ( op->is_reply() )
  {
    if ( op->header.status == DHT_STATUS_OK ) {
      BRN_DEBUG("Insert ID: OK.");
    } else {
      if ( op->header.status == DHT_STATUS_TIMEOUT ) {
        BRN_DEBUG("Insert ID: Timeout. Try again !");
        store_nodeid();
      } else {
        BRN_DEBUG("Insert ID: Unknown error.");
      }
    }
  }

  delete op;
}

/*************************************************************************************************/
/***************************************** H A N D L E R *****************************************/
/*************************************************************************************************/

enum {H_DEBUG};

static String
read_handler(Element *e, void * vparam)
{
  DartIDStore *rq = (DartIDStore *)e;

  switch ((intptr_t)vparam) {
    case H_DEBUG: {
      return String(rq->_debug) + "\n";
    }
  }
  return String("n/a\n");
}

static int 
write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  DartIDStore *rq = (DartIDStore *)e;
  String s = cp_uncomment(in_s);

  switch ((intptr_t)vparam) {
    case H_DEBUG: {
      int debug;
      if (!cp_integer(s, &debug)) 
        return errh->error("debug parameter must be an integer value between 0 and 4");
      rq->_debug = debug;
      break;
    }
  }
  return 0;
}

void
DartIDStore::add_handlers()
{
  add_read_handler("debug", read_handler, (void*) H_DEBUG);

  add_write_handler("debug", write_handler, (void*) H_DEBUG);
}

EXPORT_ELEMENT(DartIDStore)

CLICK_ENDDECLS
