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

CLICK_DECLS

class BRN2SetChannel : public Element {
public:
  BRN2SetChannel();

  const char *class_name() const	{ return "BRN2SetChannel"; }
  const char *port_count() const  { return "1/1"; }
  const char *processing() const  { return PULL; }
  bool can_live_reconfigure() const     { return false; }

  int configure(Vector<String> &, ErrorHandler *);

  Packet *pull(int);

  void add_handlers();

  int get_channel();
  void set_channel(int channel);
  void switch_to_channel(int new_channel);

  int get_freq();
  void set_freq(int freq);

private:
  String  _dev_name;
  bool    _rotate;
  int     _channel;

  bool    _switch_soon;
  bool    _switch_on_next;
};


CLICK_ENDDECLS
#endif //__BRN2SETCHANNEL_HH__