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

#ifndef BRNCOMPOUNDHANDLER_HH_
#define BRNCOMPOUNDHANDLER_HH_

#include <click/element.hh>

CLICK_DECLS

class BrnCompoundHandler : public Element 
{
public:
  BrnCompoundHandler();
  virtual ~BrnCompoundHandler();

public:
  const char *class_name() const  { return "BrnCompoundHandler"; }
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void add_handlers();

  void set_value( const String& value, ErrorHandler *errh );

  String read_handlers();


public:
  int _debug;

private:
  String _handler;
  String _items;
  String _value;

  Vector<Element*> _vec_elements;
  Vector<const Handler*> _vec_handlers;
};

CLICK_ENDDECLS
#endif /*BrnCompoundHANDLER_HH_*/
