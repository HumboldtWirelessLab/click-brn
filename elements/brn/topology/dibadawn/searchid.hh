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

#ifndef TOPOLOGY_DIBADAWN_SEARCHID_HH
#define TOPOLOGY_DIBADAWN_SEARCHID_HH

#include <click/element.hh>

CLICK_DECLS;

class DibadawnSearchId
{
public:
    static const size_t length = 10;

    DibadawnSearchId(Timestamp t, const EtherAddress &creator);
    DibadawnSearchId();
    uint8_t* PointerTo10BytesOfData();
    void setByPointerTo10BytesOfData(const uint8_t *value);
    void set(DibadawnSearchId &id);
    void set(Timestamp t, const EtherAddress &creator);
    bool isEqualTo(DibadawnSearchId &id);
    String asString();
    DibadawnSearchId & operator =(const DibadawnSearchId &id);
    friend StringAccum& operator << (StringAccum &output, const DibadawnSearchId &id);
    
private:
    uint8_t data[length];
};

StringAccum& operator << (StringAccum &output, const DibadawnSearchId &id);


CLICK_ENDDECLS
#endif
