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

#ifndef BRNIAPPSTATIONTRACKER_HH_
#define BRNIAPPSTATIONTRACKER_HH_

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/timer.hh>

CLICK_DECLS

class BRN2NodeIdentity;
class BRN2AssocList;
class Brn2LinkTable; 
class BrnIappEncap;
class BrnIappNotifyHandler;
class BrnIappDataHandler;
class BRN2AssocResponder;
class Signal;


/*
 * =c
 * BrnIappStationTracker()
 * =s debugging
 * keeps track of associated stations
 * =d
 */
class BrnIappStationTracker : public Element
{
//------------------------------------------------------------------------------
// Construction
//------------------------------------------------------------------------------
public:
	BrnIappStationTracker();
	virtual ~BrnIappStationTracker();

//------------------------------------------------------------------------------
// Overrides
//------------------------------------------------------------------------------
public:
  const char *class_name() const  { return "BrnIappStationTracker"; }
  const char *port_count() const  { return "1/0"; }
  const char *processing() const  { return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *errh);
  bool can_live_reconfigure() const { return false; }
  void add_handlers();

  void push(int, Packet* p);

//------------------------------------------------------------------------------
// Methods
//------------------------------------------------------------------------------
public:
  /**
   * handle the (re)association of a station
   * 
   * @param sta @a [in] The address of the station.
   * @param ap_new @a [in] Address of the new access point.
   * @param ap_old @a [in] Address of the old access point, if known.
   */
  void sta_associated(
    EtherAddress  sta, 
    EtherAddress  ap_new, 
    EtherAddress  ap_old,
    BRN2Device *device,
    const String& ssid);

  /**
   * station disassociated from access point.
   * 
   * @param sta @a [in] The address of the station.
   */
  void sta_disassociated(
    EtherAddress  sta);

  /**
   * handle the detection of a roaming of a station.

   * @param sta @a [in] The address of the station.
   * @param ap_new @a [in] Address of the new access point.
   * @param ap_old @a [in] Address of the old access point, if known.
   */  
  void sta_roamed(
    EtherAddress  sta, 
    EtherAddress  ap_new, 
    EtherAddress  ap_old);

  /**
   * Filter packets for clients from which we know the old ap. send these packets
   * to the old ap instead of delaying them.
   * 
   * @param p @a [in] The packet which should be buffered.
   * @return the packet if not filtered, NULL otherwise.
   */
  Packet* filter_buffered_packet(
    Packet *p);

  void disassoc_all(int reason);

  /**
   * Update the association of the station.
   * 
   * @þaram sta @a [in] the station to refresh.
   */  
  bool update(EtherAddress sta);

//------------------------------------------------------------------------------
// Internals
//------------------------------------------------------------------------------
protected:
  bool update_linktable(
    EtherAddress sta, 
    EtherAddress ap_new,
    EtherAddress ap_old = EtherAddress()); 

  void clear_stale();

  static void static_timer_clear_stale(Timer *, void *);

//------------------------------------------------------------------------------
// Data
//------------------------------------------------------------------------------
public:
  int                     _debug;
  bool                    _optimize;

  uint8_t                 _seq_no;

  Timer                   _timer;
  timeval                 _stale_timeout;

  // Elements
  BRN2NodeIdentity*       _id;
  BRN2AssocList*          _assoc_list;
  Brn2LinkTable*          _link_table;
  BrnIappNotifyHandler*   _notify_handler;
  BrnIappDataHandler*     _data_handler;
  BRN2AssocResponder*     _assoc_responder;
  //Signal*                _sig_assoc;
};

CLICK_ENDDECLS
#endif /*BRNIAPPSTATIONTRACKER_HH_*/
