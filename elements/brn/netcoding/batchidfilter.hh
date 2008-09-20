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

#ifndef BATCHIDFILTER_HH
#define BATCHIDFILTER_HH

#include <click/config.h>
#include <click/element.hh>
#include <click/dequeue.hh>
#include "netcoding.h"

CLICK_DECLS

/**
 * helper class to facilitate 'yank'ing of packets from the send queue based
 * on their batch id, see Encoder for details
 */
class BatchIdFilter {
	public:
		BatchIdFilter(uint32_t my_id) : id(my_id) {}
		bool operator()(Packet * p) {
			return (*(uint32_t *)(p->data() + sizeof(click_brn_dsr) 
				+ sizeof(click_brn))) == id;
		}
	private:
		uint32_t id;
};

CLICK_ENDDECLS

#endif
