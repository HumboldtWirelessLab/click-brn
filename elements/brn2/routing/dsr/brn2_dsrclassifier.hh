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

#ifndef BRN2DSRCLASSIFIER_HH
#define BRN2DSRCLASSIFIER_HH

#include <click/element.hh>
#include <click/bighashmap.hh>

CLICK_DECLS

/*
 * =c 
 * BRN2DSRClassifier()
 * =s
 * Classifies dsr src packets according to their payload type.
 *
 * =d 
 */
class BRN2DSRClassifier : public Element {

public:
  BRN2DSRClassifier();
  ~BRN2DSRClassifier();

  const char *class_name() const	{ return "BRN2DSRClassifier"; }
  const char *port_count() const	{ return "1/1-4"; }
  const char *processing() const	{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  virtual void push(int port, Packet *p);

  static String getNextArg(const String &s);

private:
  int _debug;
  HashMap<uint32_t, int> _msg_to_outport_map;
};

CLICK_ENDDECLS
#endif