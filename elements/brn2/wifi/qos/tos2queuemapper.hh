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

#ifndef TOS2QUEUEMAPPERELEMENT_HH
#define TOS2QUEUEMAPPERELEMENT_HH
#include <click/element.hh>
#include <elements/brn2/brnelement.hh>
#include <elements/brn2/wifi/channelstats.hh>

CLICK_DECLS

/*
=c
()

=d

*/

class Tos2QueueMapper : public BRNElement {

 public:

  Tos2QueueMapper();
  ~Tos2QueueMapper();

  const char *class_name() const  { return "Tos2QueueMapper"; }
  const char *port_count() const  { return "1/1"; }

  int configure(Vector<String> &, ErrorHandler *);

  Packet *simple_action(Packet *);

  int get_cwmin(int busy, int nodes);
  int find_queue(int cwmin);

  ChannelStats *_cst;

  uint8_t no_queues;
  uint16_t *_cwmin;
  uint16_t *_cwmax;
  uint16_t *_aifs;
};

CLICK_ENDDECLS
#endif