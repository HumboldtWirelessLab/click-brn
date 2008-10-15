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

#ifndef NCDECODINGBATCH_HH
#define NCDECODINGBATCH_HH


#include <elements/brn/brn.h>
#include <elements/brn/brnlinktable.hh>
#include "ncbatch.hh"
#include "netcoding.h"

CLICK_DECLS

/**
 * DecodingBatch is a base class for both ForwardBatch and DestBatch.
 * It implements the gaussian algorithm on netcoding headers, which is used
 * to decode packets or find out if they're decodable
 */
class NCDecodingBatch : virtual public NCBatch
{
public:

  virtual void putEncoded(click_brn_dsr & dsrHeader, 
  	click_brn_netcoding & ncHeader, Packet * packet);

  virtual Packet * getPlain() = 0;

  // tell if the batch is fully decoded
  virtual bool finished() {return processedRows >= numFragments;}
  
  unsigned getProcessedRows() {return processedRows;}
  
  virtual bool isAckable() {return ackable;}
  
  const click_brn_dsr & getIncomingRoute() {return incomingRoute;}

  bool isSelfContained();
  
  unsigned getReceivedEncoded() {return receivedEncoded;}
  
  void setStopSent() {
  	stopSent = true;
  }
  
  bool getStopSent() const {
  	return stopSent;
  }
  
  void setStartSent() {
  	startSent = true;
  }
  
  bool getStartSent() const {
  	return startSent;
  }
  
protected:
  NCDecodingBatch();
  NCDecodingBatch(NCDecodingBatch * old);
  virtual ~NCDecodingBatch();
  void sortColumns(click_brn_netcoding & header);
  void swapHeaders(unsigned row1, unsigned row2);
  void swapHeaders(Vector<click_brn_netcoding> & h, unsigned row1, 
  		unsigned row2);
  void swapPackets(unsigned row1, unsigned row2);
  virtual void swapRows(unsigned, unsigned) = 0;
  virtual void addRow(click_brn_netcoding & header, Packet * p);
  void swapColumns(unsigned col1, unsigned col2);
  bool eliminateZeroPivot(unsigned pos);
  virtual void divideRow(unsigned row, unsigned multiplier) = 0;
  virtual void subtractMultipleRow(unsigned base, unsigned subtracted, 
  		unsigned multiplier) = 0;
  bool trySolve();
  void init();
  unsigned receivedEncoded;
  unsigned deliveredPlain;
  unsigned processedRows;

  bool stopSent;
  bool startSent;
  bool ackable;
  
  click_brn_dsr incomingRoute;
  uint8_t lastFragments[LAST_FRAGMENTS_LENGTH];
  unsigned columnPositions[MAX_FRAGMENTS_IN_BATCH];
  Vector<click_brn_netcoding> headers;
	
  friend class NetcodingCache;
};


CLICK_ENDDECLS

#endif
