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

#ifndef NCBATCH_HH
#define NCBATCH_HH

#include <elements/brn/brn.h>
#include <elements/brn/routing/linkstat/brnlinktable.hh>
#include <click/dequeue.hh>
#include <click/timestamp.hh>
#include "netcoding.h"

CLICK_DECLS

/**
 * The base class of all batches. Batches are managed by the cache. No one but
 * the cache may thus create or destroy them.
 */
class NCBatch {
public:

	uint32_t getId() const {return id;}
	int getSize() const {return numFragments;}
	Timestamp getAge() const {return Timestamp::now() - creation;}
	uint32_t nextId() const;
	static uint32_t prevId(uint32_t orig);

protected:
	NCBatch() {assert ( false );} // only there to silence virtually derived classes
	NCBatch ( uint32_t myId) :
	id ( myId ),
	numFragments ( MAX_FRAGMENTS_IN_BATCH ),
	creation(Timestamp::now())
	{}

	// cache keeps a set of batches needed for encoding and a set of batches
	// for decoding
	// encoder and decoder signal if a batch is no more needed for their
	// task
	// if the cache finds a batch isn't needed for either task anymore, it
	// deletes it.

	virtual ~NCBatch();

	Packet * addDsrHeader(const click_brn_dsr & inHeader, Packet * p_in);
	Packet * addNcHeader(const click_brn_netcoding & inHeader, Packet * p_in);

	uint32_t id;
	unsigned numFragments;
	Timestamp creation;
	DEQueue<Packet *> data;
	friend class NetcodingCache;
};

CLICK_ENDDECLS

#endif
