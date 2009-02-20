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
#include "ncencoder.hh"
#include "ncforwardbatch.hh"
#include "galois.hh"
#include <elements/brn/brn.h>
#include "elements/brn/brnprotocol/brnpacketanno.hh"

#define ETHERTYPE_BRN          0x8086

CLICK_DECLS

int NetcodingEncoder::configure(Vector<String> &conf, ErrorHandler *errh) {
	int bitRate = 0;
	fragmentsInPacket = 0;
	if (cp_va_kparse(conf, this, errh,
      "NC_CACHE", cpkP+cpkM, cpElement, /*"netcoding cache",*/ &cache,
      "NODE_IDENTITY", cpkP+cpkM, cpElement, /*"node identity",*/ &me,
      "SEND_QUEUE", cpkP+cpkM, cpElement, /*"sending queue",*/ &sendQueue,
      "BITRATE", cpkP+cpkM, cpInteger, /*"bit rate",*/ &bitRate,
      "FRAGMENTS_IN_PACKET", cpkP+cpkM, cpInteger, /*"no. fragments for each coded packet",*/ &fragmentsInPacket,
       cpEnd) < 0)
		return -1;
	if ( !cache || !cache->cast("NetcodingCache") )
		return -1;
	if ( !sendQueue || !sendQueue->cast("SimpleQueue"))
		return -1;
	if (!me || !me->cast("NodeIdentity"))
		return -1;
	if (fragmentsInPacket == 0)
		return -1;
	if (bitRate) {
		bitRate = bitRate / 2; // conversion from click notation to real MBps
		shortWait = Timestamp::make_usec(500 / bitRate);
		longWait = shortWait * 10;
	}
	return 0;
}

int NetcodingEncoder::initialize(ErrorHandler *) {
	trigger.initialize(this);
	trigger.schedule_after_msec(1000/SEND_TRIGGER_RATE);
	return 0;
}

void * NetcodingEncoder::cast(const char * name) {
	const char *my_name = class_name();
	const char *parent_name = "NotificationClient";
	if (my_name && name && strcmp(my_name, name) == 0)
		return this;
	else if (parent_name && name && strcmp(parent_name, name) == 0)
		return this;
	else
		return 0;
}

void NetcodingEncoder::trySchedule(NCEncodingBatch * batch) {
	if (batch->lastFragmentSeen() && batch->getStartReceived()
			&& !batch->isScheduled() && !batch->getStopReceived()) {
		batch->setScheduled(); // make sure we don't schedule it twice
		schedule.push_back(batch);
		triggerSend();
	}
}

bool NetcodingEncoder::isUpstream(const NCEncodingBatch * b,
		const click_ether * ether) {
	const click_brn_dsr & dsrHeader = b->getRoute();
	EtherAddress dst(ether->ether_dhost);
	if (dsrHeader.dsr_src == *(me->getMyWirelessAddress())) {
		return false;
	} else if (dsrHeader.dsr_src == dst)
		return true;

	for (int i = 0; i < dsrHeader.dsr_hop_count; ++i) {
		if (dsrHeader.addr[i].hw == *(me->getMyWirelessAddress())) {
			return false;
		} else if (dsrHeader.addr[i].hw == dst) {
			return true;
		}
	}
	return true;
}

void NetcodingEncoder::handleStartStop(Packet * p) {
	uint32_t batchId = *((uint32_t *)p->data());
	p->pull(4);
	uint8_t innovation = *((uint8_t *)p->data());
	p->pull(1);
	uint8_t startNext = *((uint8_t *)p->data());
	bool forMe = (*me->getMyWirelessAddress() == p->ether_header()->ether_dhost);

	NCEncodingBatch * batch = cache->getEncodingBatch(batchId);
	if (batch != NULL) {
		if (innovation != 0) {
			// this is a real STOP
			if (forMe) {
				NCEncodingBatch * next = NULL;
				if (batch->getRoute().dsr_type == BRN_DSR_RERR) {
					batch->setStopReceived();
					propagateStop(batchId, startNext);
				} else {
					next = handleStop(batch);
					if (startNext) {
						handleStart(next);
					}
				}
			} else if (batch->isScheduled() && !isUpstream(batch, p->ether_header())) {
				// batch is scheduled - schedule it last
				// we can't stop it completely, because we don't know if other nodes
				// between stop-sender and here have received it
				for (int i = 0; i < schedule.size(); ++i) {
					if (schedule[i] == batch) {
						schedule[i] = NULL;
						break;
					}
				}
				schedule.push_back(batch);
				while (schedule.front() == NULL)
					schedule.pop_front();
			}
		} else if (forMe) {
			// this is a START for this node
			handleStart(batch);
		} else {
			// START packet for another node, wait until sending is expected to be
			// finished there
			//wait(LONG);
		}
	} else if (innovation == 0) {
		if (!forMe) {
			//wait(LONG);
		} else {
			// start packet for unknown batch ... happens when tx fails for STOP
			// remember and put to use later when batch is created
			BRN_WARN("start packet for unknown batch");
			unknownStarts.insert(batchId);
		}
	} else if (forMe) {
		// stop for unknown batch
		// create temporary batch to make sure handleStop is called later
		NCForwardBatch * fw = cache->createTmpFWBatch(batchId);
		unknownStarts.remove(batchId);
		fw->setStartReceived();
		fw->setStopReceived();
		propagateStop(batchId, startNext);
	}
	p->kill();
}

void NetcodingEncoder::propagateStop(uint32_t batchId, bool startNext) {
	NCEncodingBatch * next = NULL;
	if (!cache->batchExists(batchId + 1)) {
		// new batch has to be created after stop, no matter if it's needed for startNext
		next = cache->createTmpFWBatch(batchId + 1);
		checkUnknownStarts(next);
	} else {
		next = cache->getEncodingBatch(batchId + 1);
	}
	if (startNext)
		handleStart(next);
}

void NetcodingEncoder::handleStart(NCEncodingBatch * batch) {
	batch->setStartReceived();
	if (!batch->isScheduled()) {
		if (waitState != SENDING) {
			waitState = SENDING;
			trigger.unschedule();
			trigger.schedule_after_msec(1000/SEND_TRIGGER_RATE);
		}
		trySchedule(batch);
	}
}

/**
 * if port == stop, remove the indicated batch
 * else
 * strip brn header, put packet into cache,
 * find out the batch, update its rank,
 * reschedule it.
 *
 */
void NetcodingEncoder::push(int port, Packet * p) {
	click_brn brnHeader = *((click_brn *)p->data());
	p->pull(sizeof(click_brn));
	if (port == NCENCODER_INPUT_STOP) {
		handleStartStop(p);
	} else {
		NCEncodingBatch * batch = cache->putPlain(p);
		if (batch) {
			checkUnknownStarts(batch);
			if (!batch->isScheduled()) {
				if (batch->getStopReceived())
					// temporary batch created by STOP before data was available
					handleStop(batch);
				else
					trySchedule(batch);
			}
		}
	}
}

void NetcodingEncoder::wait(WaitState time) {
	Timestamp baseWait = sendQueue->size() * shortWait;
	trigger.unschedule();
	waitState = time;
	if (time == LONG) {
		trigger.schedule_after(longWait + baseWait);
	} else if (time == SHORT) {
		trigger.schedule_after(shortWait+ baseWait);
	} else {
		trigger.schedule_after_msec(1000/SEND_TRIGGER_RATE);
	}
}

void NetcodingEncoder::checkUnknownStarts(NCEncodingBatch * batch) {
	uint32_t newid = batch->getId();
	if (unknownStarts.contains(newid)) {
		batch->setStartReceived();
		unknownStarts.remove(newid);
	}
}

NCEncodingBatch * NetcodingEncoder::handleStop(NCEncodingBatch * batch) {
	NCDecodingBatch * fwold = cache->isDecoding(batch);
	Packet * stop = NULL;
	uint32_t oldId = 0;
	if (fwold && !fwold->getStopSent()) {
		oldId = fwold->getId();
		// combine START and STOP so that previous hop isn't confused if only one arrives
		stop = getStartOrStop(dynamic_cast<NCForwardBatch *>(fwold),
				0xff, 1);
	}

	purgeBatch(batch);
	batch->setStopReceived();
	NCEncodingBatch * successor = cache->encodingDone(batch);
	checkUnknownStarts(successor);
	NCDecodingBatch * fw = cache->isDecoding(successor);
	if (fw) {
		// forwarding batch
		if (fw->getStopSent()) {
			// someone has to send now, the previous node can't, so this node is the
			// best guess
			successor->setStartReceived();
			if (stop) {
				stop->kill();
				stop = NULL;
			}
		} else if (!stop) {
			sendStart(fw);
		} else {
			output(NCENCODER_OUTPUT_START).push(stop);
			fw->setStartSent();
			if (cache->batchExists(oldId)) {
				fwold->setStopSent();
			}
			//wait(LONG);
		}
	} else if (stop) {
		stop->kill();
		stop = NULL;
	}
	trySchedule(successor);
	return successor;
}

void NetcodingEncoder::run_timer(Timer * t) {
	if (waitState != SENDING && !sendQueue->empty()) {
		// we haven't actually waited, try again
		wait(waitState);
	} else {
		waitState = SENDING;
		trigger.schedule_after_msec(1000/SEND_TRIGGER_RATE);
		timerHits++;
		Timestamp difference = Timestamp::now() - t->expiry();
		if (difference > Timestamp::make_msec(1500/SEND_TRIGGER_RATE))
			BRN_WARN("severe timer deviation at %d th hit: %d ms", timerHits,
					difference.msec1());

		triggerSend();
	}
}

void NetcodingEncoder::notify() {
	if (waitState != SENDING)
		// postpone wait period as there has been a packet
		wait(waitState);
	else
		triggerSend();
}

void NetcodingEncoder::triggerSend() {
	if (schedule.size() > 0) {
		NCEncodingBatch * batch = schedule.front();
		if (batch == NULL) {
			schedule.pop_front();
			triggerSend();
			return;
		}
		while (waitState == SENDING && sendQueue->size() < MAX_QUEUE_LENGTH) {
			if (!directSend(batch)) {
				schedule.pop_front();
				schedule.push_back(batch);
				BRN_DEBUG("unable to produce packet");
				break;
			}
		}
	}
}

int NetcodingEncoder::directSend(NCEncodingBatch * batch) {
	click_brn brnHeader;
	brnHeader.dst_port = BRN_PORT_NETCODING;
	brnHeader.src_port = BRN_PORT_NETCODING;
	brnHeader.body_length = 0;
	brnHeader.ttl = 255;
	brnHeader.tos = 0;
	unsigned sent = 0;

	for (unsigned i = 0; i < fragmentsInPacket; ++i) {
		Packet * p1 = batch->getEncoded();
		if (p1) {
			sent++;
			WritablePacket * p = p1->push(sizeof(click_brn));
			memcpy(p->data(), &brnHeader, sizeof(click_brn));
			if (sent == fragmentsInPacket)
				SET_EXTRA_PACKETS_ANNO(p, sent);
			output(0).push(p);
		} else {
			break;
		}
	}
	unsigned allSent = batch->getDeliveredEncoded();
	unsigned allReceived = batch->getSize();
	if (!(allSent % allReceived))
		wait(SHORT);
	return sent;
}

void NetcodingEncoder::purgeBatch(NCEncodingBatch * batch) {
	Vector<Packet *> v;
	sendQueue->yank(BatchIdFilter(batch->getId()), v);
	for (int i = 0; i < v.size(); ++i)
		v[i]->kill();
	if (schedule.size() > 0 && schedule.front() == batch) {
		schedule.pop_front();
		while (schedule.size() > 0 && schedule.front() == NULL)
			schedule.pop_front();
	} else {
		for (int i = 1; i < schedule.size(); ++i) {
			// this should be rare.
			if (schedule[i] == batch) {
				schedule[i] = NULL;
				break;
			}
		}
	}
}

void NetcodingEncoder::replacePackets(NCEncodingBatch * batch) {
	Vector<Packet *> v;
	sendQueue->yank(BatchIdFilter(batch->getId()), v);
	for (int i = 0; i < v.size(); ++i) {
		directSend(batch);
		v[i]->kill();
	}
}

void NetcodingEncoder::sendStart(NCDecodingBatch * batch) {
	output(NCENCODER_OUTPUT_START).push(getStartOrStop(
			dynamic_cast<NCForwardBatch *>(batch), 0));
	batch->setStartSent();
	//wait(LONG);
}

void NetcodingEncoder::sendStop(NCDecodingBatch * batch) {
	output(NCENCODER_OUTPUT_START).push(getStartOrStop(
			dynamic_cast<NCForwardBatch *>(batch), 0xff));
	batch->setStopSent();
}

Packet * NetcodingEncoder::getStartOrStop(NCForwardBatch * batch,
		uint8_t numPackets, uint8_t startNext) {

	WritablePacket * start = Packet::make(sizeof(click_ether) + sizeof(click_brn)
			+ 8);
	click_brn * brnHeader = (click_brn *)(start->data() + sizeof(click_ether));
	brnHeader->src_port = BRN_PORT_NETCODING_STOP;
	brnHeader->dst_port = BRN_PORT_NETCODING_STOP;
	brnHeader->body_length = sizeof(click_brn) + sizeof(click_brn_netcoding);
	brnHeader->ttl = 255;
	brnHeader->tos = BRN_TOS_HP;
	const click_brn_dsr & route = batch->getIncomingRoute();

	uint32_t * batchId = (uint32_t *)(start->data() + sizeof(click_ether)
			+ sizeof(click_brn));
	uint8_t * mode = (uint8_t *)(start->data() + sizeof(click_ether)
			+ sizeof(click_brn) + 4);
	uint8_t * next = mode + 1;
	*batchId = batch->getId();
	click_ether * ether = (click_ether *)start->data();
	start->set_ether_header(ether);
	int pos = route.dsr_hop_count - route.dsr_segsleft - 1;
	hwaddr shost;
	if (pos < 0)
		shost = route.dsr_src;
	else
		shost = route.addr[pos].hw;
	for (int i = 0; i < 6; ++i)
		ether->ether_dhost[i] = shost.data[i];
	for (int i = 0; i < 6; ++i)
		ether->ether_shost[i] = me->getMyWirelessAddress()->data()[i];
	assert(ether->ether_dhost[5] != ether->ether_shost[5]);
	ether->ether_type = htons(ETHERTYPE_BRN);
  BRNPacketAnno::set_tos_anno(start,1);
	*mode = numPackets;
	*next = startNext;
	return start;
}

#include <click/hashmap.cc>
#include "hashset.cc"
#include <click/dequeue.cc>
template class DEQueue<NCEncodingBatch *>;
template class HashSet<uint32_t>;
template class HashMap<uint32_t, uint32_t>;
CLICK_ENDDECLS
EXPORT_ELEMENT ( NetcodingEncoder )

