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

#include <click/config.h>
#include <elements/brn/common.hh>
#include "netcoding.h"
#include "tracecollector.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <click/router.hh>

CLICK_DECLS

EtherAddress TraceCollector::getLastNode(const click_brn_dsr * dsrHeader, char pos) {
	pos = dsrHeader->dsr_hop_count - dsrHeader->dsr_segsleft + pos - 1;
  	const hwaddr * lastnode = 0;
  	if (pos == -1) 
	  	lastnode = &(dsrHeader->dsr_src);
  	else
	  	lastnode = &(dsrHeader->addr[(int)pos].hw);
	
	return EtherAddress(lastnode->data);
}

void TraceCollector::duplicate(const click_brn_dsr * dsrHeader, char pos) {
	dupe << EtherAddress(dsrHeader->dsr_src.data) << "-" << EtherAddress(dsrHeader->dsr_dst.data) 
	<< "-" << getLastNode(dsrHeader, pos) << ";";  
}

void TraceCollector::forwarded(const click_brn_dsr * dsrHeader, char pos) {
	forward << EtherAddress(dsrHeader->dsr_src.data) << "-" << EtherAddress(dsrHeader->dsr_dst.data) 
	<< "-" << getLastNode(dsrHeader, pos) << ";";
}

void TraceCollector::corrupt(const click_brn_dsr * dsrHeader, char pos) {
	drop << EtherAddress(dsrHeader->dsr_src.data) << "-" << EtherAddress(dsrHeader->dsr_dst.data) 
	<< "-" << getLastNode(dsrHeader, pos) << ";";
}

String TraceCollector::read_handler(Element *e, void *thunk)
{
    TraceCollector *t = static_cast<TraceCollector *>(e);
    int which = reinterpret_cast<intptr_t>(thunk);
    switch (which) {
      case 0:
		return t->forward.take_string();
      case 1:
		return t->dupe.take_string();
      case 2:
		return t->drop.take_string();
      default:
		return "";
    }
}

void TraceCollector::add_handlers()
{
    add_read_handler("forwarded", read_handler, (void *)0);
    add_read_handler("duplicate", read_handler, (void *)1);
    add_read_handler("dropped", read_handler, (void *)2);
}

/**
 * This whole idea is so bad it actually shouldn' be fixed. It's preserved in order
 * to allow you to study how not to trace packets. In order to do it correctly:
 * 
 * 1. Teach brn.sim the concept of routing on link layer. This concept is evil in itself
 * but we don't get around that. We can derive the IP address from MAC address in
 * this special case, because we know it's UDP traffic and we know the IP address is
 * the last four bytes of the MAC address in our simulation environment. Like this we
 * can pretend to be routing on net layer, but that's just ugly and incredibly fragile.
 * 
 * 2. Store all forwarding/dupe/corrupt counts locally in this class.
 * 
 * 3. Use brn.sim.handler.StatsClickHandler to fetch the data after the simulation, 
 * rather than pushing it
 * 
 * 4. Determine opportunistic reception already here. This would simplify the 
 * evaluation a lot.
 * 
 */
 /*
void TraceCollector::forwarded(const click_brn_dsr * dsrHeader, char pos, char mode) {
	
	 
	Router * thisNode = router();
	char message[13]; // 1 byte descriptor + 3*4 bytes ip
	message[0] = mode;
  
  	pos = dsrHeader->dsr_hop_count - dsrHeader->dsr_segsleft + pos - 1;
  	const hwaddr * lastnode = 0;
  	if (pos == -1) 
	  	lastnode = &(dsrHeader->dsr_src);
  	else
	  	lastnode = &(dsrHeader->addr[(int)pos].hw);
  	// use the implicit knowledge about mac and ip addresses to fool the brn gui
  	// forward graph. Also this should be done as a proper C struct ...
  	// So, this is a crude hack
  	assert(lastnode->data[2] == 0);
  	memcpy(&message[1], &(dsrHeader->dsr_src.data[2]), 4);
  	memcpy(&message[5], &(lastnode->data[2]), 4);
  	memcpy(&message[9], &(dsrHeader->dsr_dst.data[2]), 4);
  
  	thisNode->sim_trace(message, 13);
  	
}
*/





CLICK_ENDDECLS
EXPORT_ELEMENT(TraceCollector)
