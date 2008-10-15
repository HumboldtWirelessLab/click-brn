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

#ifndef SIGNAL_HH_
#define SIGNAL_HH_

#include <click/element.hh>

CLICK_DECLS

/*
 * =c
 * Signal()
 * =s debugging
 * Signal element, could be used to transport signals through the click graph
 * 
 * NOTE: if there is no receipiant, leave the field empty and set ACTIVE false
 * 
 * =d
 */
class Signal : public Element
{
//------------------------------------------------------------------------------
// Construction
//------------------------------------------------------------------------------
public:
	Signal();
	virtual ~Signal();

//------------------------------------------------------------------------------
// Overrides
//------------------------------------------------------------------------------
public:
  const char *class_name() const  { return "Signal"; }
  const char *port_count() const  { return "0/0"; }
  const char *processing() const  { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *errh);
  bool can_live_reconfigure() const { return false; }
  void add_handlers();

//------------------------------------------------------------------------------
// Methods
//------------------------------------------------------------------------------
public:
  /**
   * TODO think about return value?
   */
  void send_signal_action(const String& param = String(), ErrorHandler *errh = NULL);

  bool is_signal(const String& name) {
    return (_signal == name);
  }

  bool is_signal(const char* name) {
    return (_signal == name);
  }

//------------------------------------------------------------------------------
// Data
//------------------------------------------------------------------------------
public:
  int                        _debug;
  bool                       _active;

  String                    _signal;
  String                    _receptions;

  Vector<Element*>          _vec_elements;
  Vector<const Handler*>    _vec_handlers;
};

CLICK_ENDDECLS
#endif /*SIGNAL_HH_*/
