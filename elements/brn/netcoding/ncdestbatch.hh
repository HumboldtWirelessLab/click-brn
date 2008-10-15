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

#ifndef NCDESTBATCH_HH
#define NCDESTBATCH_HH
#include "ncdecodingbatch.hh"

CLICK_DECLS

/**
 * DestBatch decodes fragments. It's managed by Cache and used by Decoder. It
 * uses its inheritence from DecodingBatch for most of its functions
 */
class NCDestBatch : public NCDecodingBatch
{
public:

  virtual Packet * getPlain();
  virtual void putEncoded(click_brn_dsr & dsrHeader, 
  	click_brn_netcoding & ncHeader, Packet * packet);

private:
  int deliverable;
  
  NCDestBatch(uint32_t myId) : 
    NCBatch(myId), deliverable(0) {}
  virtual ~NCDestBatch();
  virtual void swapRows(unsigned, unsigned);
  virtual void divideRow(unsigned row, unsigned multiplier);
  virtual void subtractMultipleRow(unsigned base, unsigned subtracted, 
  		unsigned multiplier) ;
  friend class NetcodingCache;
};

CLICK_ENDDECLS

#endif
