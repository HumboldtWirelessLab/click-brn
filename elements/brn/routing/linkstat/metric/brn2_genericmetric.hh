/* Copyright (C) 2005 BerlinRoofNet Lab
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
 * For additional licensing options, consult http://www.BerlinRoofNet.de 
 * or contact brn@informatik.hu-berlin.de. 
 */

#ifndef BRN2GENERICMETRIC_HH
#define BRN2GENERICMETRIC_HH
#include <click/element.hh>

#include "elements/brn/brnelement.hh"

CLICK_DECLS

//
// Common interface of all route metric elements.
//

class BrnRateSize {
  public:
    uint16_t _rate; //Rate of Linkprobe //for n use packed_16
    uint16_t _size; //Size of Linkprobe
    uint8_t _power;

    size_t _hashcode;

    BrnRateSize(): _rate(0), _size(0), _power(0), _hashcode(0) {};

    BrnRateSize(uint16_t r, uint16_t s, uint16_t p): _rate(r), _size(s), _power(p) {
      _hashcode = (((uint32_t)_rate) << 16) + (((uint32_t)_size) << 2) + _power;
    };

    inline bool operator==(BrnRateSize other)
    {
      if ( _hashcode != other._hashcode ) return false;
      return (other._rate == _rate && other._size == _size && other._power == _power);
    }
};

inline unsigned hashcode(BrnRateSize brn_rs) {
  return brn_rs._hashcode;
}
#define METRIC_UPDATE_ACTIVE  LINK_UPDATE_LOCAL_ACTIVE
#define METRIC_UPDATE_PASSIVE LINK_UPDATE_LOCAL_PASSIVE

class BRN2GenericMetric : public BRNElement {

 public:
  virtual void update_link(const EtherAddress &from, EtherAddress &to,
                           Vector<BrnRateSize> &rs, Vector<uint8_t> &fwd, Vector<uint8_t> &rev, uint32_t seq, uint8_t update_mode) = 0;

};

CLICK_ENDDECLS
#endif
