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

#ifndef DART_IDSTORE_HH
#define DART_IDSTORE_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/timer.hh>

#include "elements/brn/routing/identity/brn2_nodeidentity.hh"

#include "elements/brn/dht/storage/dhtstorage.hh"
#include "elements/brn/dht/routing/dart/dart_routingtable.hh"

CLICK_DECLS

class DartIDStore : public Element
{
 public:

  //---------------------------------------------------------------------------
  // public methods
  //---------------------------------------------------------------------------

  DartIDStore();
  ~DartIDStore();

  const char *class_name() const { return "DartIDStore"; }
  const char *port_count() const { return "0/0"; }
  const char *processing() const { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);

  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();

  static void routingtable_callback_func(void *e, int status);
  void store_nodeid();

  static void callback_func(void *e, DHTOperation *op);
  void callback(DHTOperation *op);

 private:

  //---------------------------------------------------------------------------
  // private fields
  //---------------------------------------------------------------------------
  BRN2NodeIdentity *_me;

  DHTStorage *_dht_storage;
  DartRoutingTable *_drt;

 public:
  int _debug;

};

CLICK_ENDDECLS
#endif
