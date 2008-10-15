/* Copyright (C) 2008 Ulf Hermann
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
 */

#ifndef FRAGMENTSENDER_HH
#define FRAGMENTSENDER_HH

#include <click/config.h>
#include <click/element.hh>
#include <click/dequeue.hh>
#include "netcoding.h"
#include "headerpacker.hh"

CLICK_DECLS

typedef DEQueue<Packet *> PacketList;

/**
 * Assembles encoded packets from encoded fragments. The fragments are expected
 * to come with click_brn, click_brn_dsr and click_brn_netcoding in that order.
 * The output has click_brn, click_brn_dsr and click_brn_netcoding_packet in 
 * front of the packet plus click_brn_netcoding_fragment in front of each 
 * fragment
 */
class FragmentSender : public Element {
  public:
    FragmentSender(){}
    ~FragmentSender() {}
    int configure(Vector<String> &conf, ErrorHandler *errh);
    const char* class_name() const { return "FragmentSender"; }
    const char *port_count() const  { return "1/1"; }
    const char *processing() const { return "PUSH"; }
    void push(int, Packet *);

  private:
    PacketList receivedPackets;
    HeaderPacker * packer;
};

CLICK_ENDDECLS

#endif
