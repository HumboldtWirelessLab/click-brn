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
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <click/router.hh>

#include "ncdecoder.hh"
#include <elements/brn/brn.h>
#define ETHERTYPE_BRN          0x8086
CLICK_DECLS

int NetcodingDecoder::_debug = BrnLogger::DEFAULT;

int NetcodingDecoder::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int usecTimeout = 0;
  tracer = NULL;
  if (cp_va_parse(conf, this, errh,
                  cpKeywords, "NC_CACHE", cpElement, "netcoding cache", &cache,
                  "HARD_TIMEOUT", cpInteger, "hard timeout", &usecTimeout,
                  "TRACE_COLLECTOR", cpElement, "trace collector", &tracer,
                  cpEnd) < 0)
    return -1;
  if (usecTimeout != 0)
  	hardTimeout = Timestamp::make_usec(usecTimeout);
  else
  	hardTimeout = Timestamp::make_usec(10000000);
  if (!cache || !cache->cast("NetcodingCache"))
    return -1;
  return 0;
}

void NetcodingDecoder::trace(const click_brn_dsr * dsrHeader, char pos, 
	bool noninnovative) {
	if (tracer == NULL) 
		return;
	else if (noninnovative)
		tracer->duplicate(dsrHeader, pos);
	else 
		tracer->forwarded(dsrHeader, pos);
}




void NetcodingDecoder::push(int port, Packet * p)
{
	while(batches.size()) {
	   	NCDecodingBatch * back = batches.back();
    	if (back->getAge() > hardTimeout &&
    		back->finished() && 
    		!cache->isEncoding(back)) {
    		cache->decodingDone(back);
    		batches.pop_back();
    	} else {
	   		break;
    	}
  	}
  	
	if (port == NCDECODER_INPUT_FEEDBACK) {
		handleStopFeedback(p);
	} else {
		char routePosition = PAINT_ANNO(p);
		
  		click_brn brnHeader = *((click_brn *)p->data());
  		click_brn_dsr dsrHeader = *(click_brn_dsr *)(p->data() + sizeof(click_brn));
  		p->pull(sizeof(click_brn));
  
  		brnHeader.src_port = BRN_PORT_DSR;
  		brnHeader.dst_port = BRN_PORT_DSR;
  		
  		NCDecodingBatch * batch = cache->putEncoded(p);
  		
  		if (batch) {
			
  			trace(&dsrHeader, routePosition, batch->isAckable());
  			
  			
  			if (batches.size() == 0 || batch->getAge() < batches.front()->getAge()) {
	 	 		batches.push_front(batch);
  			}
  			while(Packet * out = batch->getPlain())
  			{
	    		out = out->push(sizeof(click_brn));
    			*((click_brn *)out->data()) = brnHeader;
    			output(NCDECODER_OUTPUT_DATA).push(out);
  			}
  			if ((batch->finished()
  				|| (batch->isAckable() && batch->isSelfContained() && routePosition == 0))
  				&& !batch->getStopSent())
  			{
	    		Packet * stop = assembleStopPacket(batch, 0xff);
    			output(NCDECODER_OUTPUT_STOP).push(stop);
  			} 
		} else {
			trace(&dsrHeader, routePosition, true);
  	}
		
	}
}


Packet * NetcodingDecoder::assembleStopPacket(NCDecodingBatch * batch, uint8_t numPackets)
{
  WritablePacket * stop = Packet::make(sizeof(click_ether) + sizeof(click_brn) + 8);
  click_brn * brnHeader = (click_brn *)(stop->data() + sizeof(click_ether));
  brnHeader->src_port = BRN_PORT_NETCODING_STOP;
  brnHeader->dst_port = BRN_PORT_NETCODING_STOP;
  brnHeader->body_length = sizeof(click_brn) + sizeof(click_brn_netcoding);
  brnHeader->ttl = 255;
  brnHeader->tos = BRN_TOS_HP;
  const click_brn_dsr & route = batch->getIncomingRoute();
  
  uint32_t * batchId = (uint32_t *)(stop->data() + sizeof(click_ether) + sizeof(click_brn));
  uint8_t * innovation = (uint8_t *)(stop->data() + sizeof(click_ether) + sizeof(click_brn) + 4);
  uint8_t * startNext = (uint8_t *)(stop->data() + sizeof(click_ether) + sizeof(click_brn) + 5);
  *startNext = 0;
  *batchId = batch->getId();
  click_ether * ether = (click_ether *)stop->data();
  stop->set_ether_header(ether);
  int pos = route.dsr_hop_count - route.dsr_segsleft - 1;
  hwaddr shost;
  if (pos < 0)
    shost = route.dsr_src;
  else 
    shost = route.addr[pos].hw;
  for (int i = 0; i < 6; ++i)
    ether->ether_dhost[i] = shost.data[i];
  // ether_shost is filled in at FragmentReceiver as we know the NodeIdentity there
  ether->ether_type = htons(ETHERTYPE_BRN);
//BRNNEW  stop->set_tos_anno(1);
  
  *innovation = numPackets;
  batch->setStopSent();
  batch->setStartSent();
  return stop;
}

void NetcodingDecoder::handleStopFeedback(Packet * p) {
	BRN_WARN ("start or stop failed");
	click_ether ether = *(p->ether_header());
	p->push_mac_header(14);
	click_ether * pEther = (click_ether *)p->data();
	*(pEther) = ether;
	output(NCDECODER_OUTPUT_STOP).push(p);
}

#include <click/vector.cc>
#include <click/dequeue.cc>
template class DEQueue<NCDecodingBatch *>;
template class Vector<Packet *>;
template class Vector<click_brn_netcoding>;

CLICK_ENDDECLS
EXPORT_ELEMENT(NetcodingDecoder)
