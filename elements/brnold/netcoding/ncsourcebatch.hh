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

#ifndef NCSOURCEBATCH_HH
#define NCSOURCEBATCH_HH
#include "ncencodingbatch.hh"
#include <click/dequeue.hh>

CLICK_DECLS


class NCSourceBatch : public NCEncodingBatch
{
public:
    //add a plain packet
    virtual void putPlain ( click_brn_dsr & dsrHeader, Packet * packet );
    virtual void putPlain ( Packet * packet );

    //return a random combination of all available packets
    virtual Packet * getEncoded();
    
    bool lastFragmentSeen() const 
    	{return sendHeader.lastFragmentSeen();}
	
    ~NCSourceBatch();
private:
    NCSourceBatch ( NCSourceBatch * previous );
    NCSourceBatch ( uint32_t myId, const click_brn_dsr & route, unsigned maxSize );
    
    DEQueue<Packet *> overflow;
    click_brn_netcoding sendHeader;
    click_brn_dsr latestRoute;
    
    static int overflowQueueLength;
    	
    unsigned maxSize;
    friend class NetcodingCache;
};

CLICK_ENDDECLS

#endif
