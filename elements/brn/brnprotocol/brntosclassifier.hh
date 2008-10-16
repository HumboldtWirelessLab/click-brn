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

#ifndef BRNTOSCLASSIFIER_HH_
#define BRNTOSCLASSIFIER_HH_

#include <click/etheraddress.hh>
#include <click/element.hh>

CLICK_DECLS

/*
 * =c
 * BrnTosClassifier()
 * =s debugging
 * classifies packets according to their brn tos field
 * =d
 */
class BrnTosClassifier : public Element
{
//------------------------------------------------------------------------------
// Construction
//------------------------------------------------------------------------------
public:
	BrnTosClassifier();
	virtual ~BrnTosClassifier();

//------------------------------------------------------------------------------
// Overrides
//------------------------------------------------------------------------------
public:
  const char *class_name() const  { return "BrnTosClassifier"; }
  const char *port_count() const  { return "1/1-"; }
  const char *processing() const  { return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *errh);
  bool can_live_reconfigure() const { return false; }
  void add_handlers();

  void push(int, Packet *);

//------------------------------------------------------------------------------
// Data
//------------------------------------------------------------------------------
public:
  int         _debug;
};

CLICK_ENDDECLS
#endif /*BRNTOSCLASSIFIER_HH_*/
