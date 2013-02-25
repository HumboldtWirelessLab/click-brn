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

/* Sender-/Receivernumbers */
/* field: sender,receiver */
#ifndef FALCON_LP_HANDLER_HH
#define FALCON_LP_HANDLER_HH

#include <click/element.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"

#include "elements/brn/routing/hawk/hawk_routingtable.hh"

#include "falcon_routingtable.hh"

CLICK_DECLS

#define FALCON_DEFAULT_NO_NODES_PER_LINKPROBE 5

class FalconLinkProbeHandler : public BRNElement
{

 public:
  FalconLinkProbeHandler();
  ~FalconLinkProbeHandler();

  const char *class_name() const  { return "FalconLinkProbeHandler"; }

  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  bool can_live_reconfigure() const  { return false; }

  void add_handlers();

  int32_t lpSendHandler(char *buffer, int32_t size);
  int32_t lpReceiveHandler(char *buffer, int32_t size, bool is_neighbour);

  int register_linkprobehandler();
  bool _register_handler;

 private:
  FalconRoutingTable *_frt;
  BRN2LinkStat *_linkstat;

  int _all_nodes_index;

  int _no_nodes_per_lp;

  HawkRoutingtable *_rfrt;

  Timestamp _start;

  bool _active;
  uint32_t _delay;
  bool _onlyfingertab;

 public:
  void setHawkRoutingTable(HawkRoutingtable *t) { _rfrt = t; }
};

CLICK_ENDDECLS
#endif
