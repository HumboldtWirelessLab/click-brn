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
#include "fragmenter.hh"
#include <elements/brn/common.hh>
#include <click/packet_anno.hh>

CLICK_DECLS


int Fragmenter::configure ( Vector<String> &conf, ErrorHandler *errh )
{
  fragmentDataLength = 0;
  if ( cp_va_parse ( conf, this, errh,
                     cpKeywords, 
                     "FRAGMENT_LENGTH", cpInteger, "fragment length", &fragmentDataLength, 
                     cpEnd ) < 0 )
    return -1;
  if (fragmentDataLength == 0)
  	return -1;
  return 0;
}

// input: click_brn + click_brn_dsr + data
// output: click_brn + click_brn_dsr + click_brn_netcoding + data/n
void Fragmenter::push(int, Packet * packet)
{
    click_brn brnHeader = *(click_brn *)(packet->data());
    packet->pull(sizeof(click_brn));
    click_brn_dsr dsrHeader = *(click_brn_dsr *)(packet->data());
    packet->pull(sizeof(click_brn_dsr));
    click_brn_netcoding ncHeader;
    int numFragments = 0;
    unsigned packetLength = packet->length();
    while (packetLength > 0)
    {
    	
        WritablePacket * out = Packet::make(fragmentDataLength +
                                            sizeof(click_brn_netcoding) +
                                            sizeof(click_brn_dsr) + sizeof(click_brn));
        
        numFragments++;                                    
        unsigned fragmentLength = fragmentDataLength;
        if (packetLength <= fragmentDataLength)
        {
        	SET_EXTRA_PACKETS_ANNO(out, numFragments);
          fragmentLength = packetLength;
        } else {
        	SET_EXTRA_PACKETS_ANNO(out, 0);
        }
        
        memcpy(out->data(), &brnHeader, sizeof(click_brn));
        memcpy(out->data() + sizeof(click_brn), &dsrHeader, sizeof(click_brn_dsr));
        memcpy(out->data() + sizeof(click_brn) + sizeof(click_brn_dsr),
               &ncHeader, sizeof(click_brn_netcoding));
        memcpy(out->data() + sizeof(click_brn) + sizeof(click_brn_dsr) +
               sizeof(click_brn_netcoding), packet->data(), fragmentLength);
        packet->pull(fragmentLength);
        packetLength = packet->length();
        out->set_timestamp_anno(Timestamp::now());
        output(0).push(out);
    }
    packet->kill();
}

CLICK_ENDDECLS

EXPORT_ELEMENT(Fragmenter)
