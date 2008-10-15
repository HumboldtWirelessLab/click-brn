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

#ifndef CLICK_READHANDLERCLASSIFIER_HH
#define CLICK_READHANDLERCLASSIFIER_HH
#include <click/element.hh>
CLICK_DECLS

class HandlerCall;
class NotifierSignal;

/*
=c
ReadHandlerFilter()

Pushes outermost BRN header

=d

Pushes the outermost BRN header from this packet.

=a CheckBRNHeader

*/

class ReadHandlerFilter : public Element {

 public:

  ReadHandlerFilter();
  ~ReadHandlerFilter();

  const char *class_name() const	{ return "ReadHandlerFilter"; }
  const char *port_count() const  { return "1-2/1-2"; }
  const char *processing() const  { return "l/lh"; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *errh);
  void cleanup(CleanupStage);
  bool can_live_reconfigure() const { return false; }
  void add_handlers();

  //void push(int, Packet *);
  Packet *pull(int);

public:
  int             _debug;
  HandlerCall*    _handler;
  NotifierSignal* _signals;
  String          _pattern;
};

CLICK_ENDDECLS
#endif //CLICK_READHANDLERCLASSIFIER_HH
