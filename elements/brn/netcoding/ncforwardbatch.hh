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

#ifndef NCFORWARDBATCH_HH
#define NCFORWARDBATCH_HH
#include "ncencodingbatch.hh"
#include "ncdecodingbatch.hh"
#include <click/hashmap.hh>

CLICK_DECLS

typedef HashMap<Packet *, int> PacketSet;

class NCForwardBatch : public NCDecodingBatch, public NCEncodingBatch
{
public:
  // forwarder can ignore the packets here and only update the header if needed
  virtual void putPlain(click_brn_dsr & dsrHeader, Packet * packet);
  virtual void putPlain ( Packet * packet );

  //return a combination of any encoded packets
  virtual Packet * getEncoded();

  //no decoding, no plain packets
  virtual Packet * getPlain() {return NULL;}
  
  bool lastFragmentSeen() const {return receivedEncoded > 0;}
  
  virtual bool isAckable() 
  			{return stopReceived || ackable;}
  
  virtual bool finished() 
  			{return stopReceived || (processedRows >= numFragments);}

private:
  NCForwardBatch(NCForwardBatch * oldBatch, uint32_t id);
  NCForwardBatch(uint32_t id);
  virtual void addRow ( click_brn_netcoding & header, Packet *);
  virtual void swapRows(unsigned, unsigned);
  virtual void divideRow(unsigned row, unsigned multiplier);
  virtual void subtractMultipleRow(unsigned base, unsigned subtracted, 
  		unsigned multiplier);
  bool isZero(const click_brn_netcoding & header);
  Packet * getDeepEncoded(click_brn_netcoding & header);
  Packet * getShallowEncoded(click_brn_netcoding & header);
  Vector<click_brn_netcoding> originalHeaders;

  friend class NetcodingCache;
};

CLICK_ENDDECLS

#endif
