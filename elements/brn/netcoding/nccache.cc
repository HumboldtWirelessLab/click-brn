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
#include "nccache.hh"
#include "packetarithmetics.hh"
#include "ncdestbatch.hh"
#include "ncencoder.hh"


CLICK_DECLS

int NetcodingCache::_debug = BrnLogger::DEFAULT;

NetcodingCache::~NetcodingCache() {
  IdBatchMap::iterator i = batches.begin();
  while(i.live()) {
    delete i.value();
    ++i;
  }
}


int NetcodingCache::configure(Vector<String> &conf, ErrorHandler *errh) {
	bitsInMultiplier = 0;
	fragmentsInBatch = 0;
  if (cp_va_parse(conf, this, errh,
      cpKeywords, "NODE_IDENTITY",
      cpElement, "node identity", &me,
      "ASSOC_LIST", cpElement, "association list", &associated,
      "UPDATE_ROUTE", cpBool, "update the DSR route", &updateRoute,
      "BITS_IN_MULTIPLIER", cpInteger, "bits in multiplier", &bitsInMultiplier,
      "FRAGMENTS_IN_BATCH", cpInteger, "max fragments in batch", &fragmentsInBatch,
      cpEnd) < 0)
    return -1;
  if (!me || !me->cast("NodeIdentity"))
    return -1;
  if (!associated || !associated->cast("AssocList"))
    return -1;
  if (bitsInMultiplier == 0) 
  	return -1;
  if (fragmentsInBatch == 0) 
    return -1;

  GaloisConfiguration::instance()->configure(bitsInMultiplier);
  return 0;
  
}

uint32_t NetcodingCache::findId() {
  uint32_t id = 0;
  do {
    while(id == 0 || id == NETCODING_INVALID)
      id = random();
  } while (batches.find_pair(id));
  return id;
}

NCSourceBatch * NetcodingCache::createBatch(const click_brn_dsr & route ) {
  uint32_t id = findId();
  NCSourceBatch * ret = new NCSourceBatch(id, route, fragmentsInBatch);
  batches.insert(id, ret);
  encodingBatches.insert(ret, ret);
  ownBatches.insert(EtherAddress(route.dsr_dst.data), ret);
  return ret;
}

/**
 * find the correct plain batch for the packet
 * strip the DSR header, strip the NC header and examine the netcoding header and dsr_dst
 * if (batch_id == NETCODING_INVALID) find the batch according to dest
 * or create new one 
 * else and look for the batch id
 */
NCEncodingBatch * NetcodingCache::putPlain(Packet * p) {
  NCEncodingBatch * ret = NULL;
  click_brn_dsr dsrHeader = *((click_brn_dsr *)p->data());
  p->pull(sizeof(click_brn_dsr));
  click_brn_netcoding ncHeader = *((click_brn_netcoding *)p->data());
  p->pull(sizeof(click_brn_netcoding));
  
  if (ncHeader.batch_id == NETCODING_INVALID) { // locally created  packet
    MacSrcBatchMap::Pair * pair = ownBatches.find_pair(EtherAddress(dsrHeader.dsr_dst.data));
    if (pair) {
    	ret = pair->value;
    }
    else {
      ret = createBatch(dsrHeader);  
    }
    if (updateRoute) {
    	ret->putPlain(dsrHeader, p);
    } else {
    	ret->putPlain(p);
    }
  } else { // forwarded packet
    // those double lookups assert that the batch is of the correct type; of course, casts would be faster
    ret = encodingBatches[batches[ncHeader.batch_id]];
    if (ret)
    	ret->putPlain(dsrHeader, p);
  }
  if (!ret) {
  	// forwarding batch has already timed out
  	p->kill();
  }

  return ret;
}

/**
 * strip DSR and NC header, find batch by id
 * if it's not there create new one
 * don't check for finished decoding; this is done by decoder
 */
NCDecodingBatch * NetcodingCache::putEncoded(Packet * p) {
  NCDecodingBatch * batch = NULL;
  click_brn_dsr dsrHeader = *((click_brn_dsr *)(p->data()));
  p->pull(sizeof(click_brn_dsr));
  click_brn_netcoding ncHeader = *((click_brn_netcoding *)(p->data()));
  p->pull(sizeof(click_brn_netcoding));
  uint32_t id = ncHeader.batch_id;
  IdBatchMap::Pair * pair = batches.find_pair(id);
  if (pair) {
    batch = decodingBatches[pair->value];
  } else {
    EtherAddress dst_addr(dsrHeader.dsr_dst.data);
    if (me->isIdentical(&dst_addr) || associated->is_associated(dst_addr)) {
      batch = new NCDestBatch(id);
    } else {
    	uint32_t oldId = NCBatch::prevId(id);
    	IdBatchMap::Pair * oldPair = batches.find_pair(oldId);
    	NCForwardBatch * fwBatch = NULL;
    	if (oldPair) {
    		NCForwardBatch * oldBatch = dynamic_cast<NCForwardBatch *>(oldPair->value);
    		fwBatch = new NCForwardBatch(oldBatch, id);
    	}
    	else {
      		fwBatch =  new NCForwardBatch(id);
    	}
      	encodingBatches.insert(fwBatch, fwBatch);
      	batch = fwBatch;
    }
    batches.insert(id, batch);
    decodingBatches.insert(batch, batch);
  }
  if (batch && !batch->finished()) {
  	batch->putEncoded(dsrHeader, ncHeader, p);
  	if (batch->getReceivedEncoded() == 1) {
  		numUnfinishedDecoders++;
  	}
  	if (batch->finished()) { 
  		numUnfinishedDecoders--;
  	}
  	return batch;
  } else {
  	if (batch) {
  		batch->incomingRoute = dsrHeader;
  		batch->ackable = true;
  	}
  	//assert(batch && batch->stopSent);
  	p->kill();
  	return batch;
  }
}

/**
 * check if the batch is still needed for encoding
 * otherwise delete it
 */
void NetcodingCache::decodingDone(NCDecodingBatch * b) {
  decodingBatches.remove(b);
  EncBatchSet::Pair * p = encodingBatches.find_pair(b);
  if (!p) {
    batches.remove(b->getId());
    if (!b->finished()) 
    	numUnfinishedDecoders--;
    delete b;
  }
}
  

/**
 * check if the batch is still needed for decoding
 * otherwise delete or advance it
 */
NCEncodingBatch * NetcodingCache::encodingDone(NCEncodingBatch * b) {
  encodingBatches.remove(b);
  EtherAddress dest = b->getDestination();
  MacSrcBatchMap::Pair * p = ownBatches.find_pair(dest);
  if (p != NULL) {
  	NCSourceBatch * oldBatch = p->value;
  	if(oldBatch == b) {
  	  batches.remove(oldBatch->getId());
  		NCSourceBatch * newBatch = new NCSourceBatch(oldBatch);
  		uint32_t id = newBatch->getId();
  		batches.insert(id, newBatch);
  		encodingBatches.insert(newBatch, newBatch);
  		assert(dest == newBatch->getDestination());
  		ownBatches.insert(dest, newBatch);
  		assert(ownBatches[dest] == newBatch);
  		delete oldBatch;
  		return newBatch;
  	} else {
  		return forwardBatchDone(b);
  	}
  } else {
  	return forwardBatchDone(b);
  }
}

NCEncodingBatch * NetcodingCache::forwardBatchDone(NCEncodingBatch * b) {
	numUnfinishedDecoders--;
	uint32_t newId = b->getId() + 1;
	IdBatchMap::Pair * pair = batches.find_pair(newId);
	NCEncodingBatch * next = NULL;
	if (pair) {
		next = encodingBatches[pair->value];
		if(next) {
			next->outgoingRoute = b->outgoingRoute;
			((NCForwardBatch *)next)->incomingRoute = ((NCForwardBatch *)b)->incomingRoute;
		} else {
			next = dynamic_cast<NCForwardBatch *>(pair->value);
		}
	} else {
    next =	new NCForwardBatch((NCForwardBatch *)b, newId);
    batches.insert(newId, next);
    decodingBatches.insert(next, (NCForwardBatch *)next);
    encodingBatches.insert(next, next);
	}
	
  DecBatchSet::Pair * pp = decodingBatches.find_pair(b);
  if (!pp) {
    batches.remove(b->getId());
    delete b;
  } 
  return next;
}

NCEncodingBatch * NetcodingCache::getEncodingBatch(uint32_t id) {
	IdBatchMap::Pair * p = batches.find_pair(id);
	if (p) return encodingBatches[p->value];
	else return NULL;
}

NCDecodingBatch * NetcodingCache::isDecoding ( NCBatch * b ) {
	DecBatchSet::Pair * p = decodingBatches.find_pair ( b );
	if (p) return p->value;
	else return NULL;
}
 
 NCEncodingBatch * NetcodingCache::isEncoding ( NCBatch * b ) {
	EncBatchSet::Pair * p = encodingBatches.find_pair ( b );
	if (p) return p->value;
	else return NULL;
}

NCForwardBatch * NetcodingCache::createTmpFWBatch(uint32_t id) {
	NCForwardBatch * ret = new NCForwardBatch(id);
	ret->startReceived = false;
	batches.insert(id, ret);
	decodingBatches.insert(ret, ret);
	encodingBatches.insert(ret, ret);
	return ret;
}



#include <click/bighashmap.cc>
template class HashMap<EtherAddress, NCSourceBatch *>;
template class HashMap<NCBatch *, NCEncodingBatch *>;
template class HashMap<NCBatch *, NCDecodingBatch *>;
template class HashMap<uint32_t, NCBatch *>;
#include <click/dequeue.cc>
template class DEQueue<NCEncodingBatch *>;


CLICK_ENDDECLS
EXPORT_ELEMENT(NetcodingCache)
