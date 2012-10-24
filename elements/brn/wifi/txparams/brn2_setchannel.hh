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

#ifndef __BRN2SETCHANNEL_HH__
#define __BRN2SETCHANNEL_HH__

#include <click/element.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/identity/brn2_device.hh"

CLICK_DECLS

class BRN2SetChannel : public BRNElement {
 public:
  BRN2SetChannel();

  const char *class_name() const  { return "BRN2SetChannel"; }
  const char *port_count() const  { return "1/1"; }
  const char *processing() const  { return AGNOSTIC; }
  bool can_live_reconfigure() const     { return false; }

  int configure(Vector<String> &, ErrorHandler *);

  Packet *simple_action(Packet *);

  void add_handlers();

  String get_info();

 private:
  BRN2Device *_device;

  int _channel;

 public:
   int set_channel_iwconfig(const String &devname, int channel, ErrorHandler *errh);

  int get_channel() { return _channel; }

  void set_channel(int channel) { _channel = channel; }

};


CLICK_ENDDECLS
#endif //__BRN2SETCHANNEL_HH__
