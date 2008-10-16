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

#ifndef NCCACHE_HH
#define NCCACHE_HH

#include <click/hashmap.hh>
#include <click/element.hh>
#include <click/timestamp.hh>
#include <elements/brn/brn.h>
#include "netcoding.h"
#include <elements/brn/nodeidentity.hh>
#include "ncdecodingbatch.hh"
#include "ncencodingbatch.hh"
#include "ncsourcebatch.hh"
#include "ncforwardbatch.hh"
#include <elements/brn/wifi/ap/assoclist.hh>

CLICK_DECLS


typedef HashMap<EtherAddress, NCSourceBatch *> MacSrcBatchMap;
typedef HashMap<NCBatch *, NCEncodingBatch *> EncBatchSet;
typedef HashMap<NCBatch *, NCDecodingBatch *> DecBatchSet;
typedef HashMap<uint32_t, NCBatch *> IdBatchMap;

/**
 * The Cache is used to identify packets, fill batches and manage the batches' 
 * life cycles. It
 * - always takes packets with all headers but BRN
 * - hands out batches on putEncoded and putPlain
 *	=> each batch will be handed out at least once
 * - keeps maps of source/destination/batchId to batch
 * - keeps a map of destinations to batches it has initiated itself
 * - knows if a Batch is in use by Encoder or Decoder (by type of batch)
 * - reacts on doneEncoding(Batch *) and doneDecoding(Batch *)
 * 	If a batch isn't needed by either encoder or decoder anymore it is deleted
 */
class NetcodingCache : public Element
{

	public:
		NetcodingCache() : me(0), associated(0), numUnfinishedDecoders(0), updateRoute(true) {}
		virtual ~NetcodingCache();

		
		NCEncodingBatch * putPlain ( Packet * p );
		NCDecodingBatch * putEncoded ( Packet * p );

		NCEncodingBatch * getEncodingBatch ( uint32_t id );
		void decodingDone ( NCDecodingBatch * b );
		NCEncodingBatch * encodingDone ( NCEncodingBatch * b);
		NCDecodingBatch * isDecoding ( NCBatch * b );
		NCEncodingBatch * isEncoding ( NCBatch * b );
		bool batchExists (uint32_t id) {
			IdBatchMap::Pair * pair = batches.find_pair(id);
			return pair;			
		}

		NCForwardBatch * createTmpFWBatch(uint32_t id);

		unsigned getNumUnfinishedDecoders() {return numUnfinishedDecoders;}

		const char *class_name() const {return "NetcodingCache";}
		int configure ( Vector<String> &conf, ErrorHandler *errh );

	private:
		NCEncodingBatch * forwardBatchDone(NCEncodingBatch * b);
		IdBatchMap batches;
		EncBatchSet encodingBatches;
		DecBatchSet decodingBatches;
		MacSrcBatchMap ownBatches;
		NodeIdentity * me;
		AssocList * associated;
		unsigned numUnfinishedDecoders;
		bool updateRoute;
		unsigned bitsInMultiplier;
		unsigned fragmentsInBatch;
		static int _debug;
		
		uint32_t findId();
		NCSourceBatch * createBatch ( const click_brn_dsr & route );
};

CLICK_ENDDECLS

#endif
