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

/*
 * PacketDecompression.{cc,hh}
 */

#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>

#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "packetcompression.hh"
#include "packetdecompression.hh"
#include "lzw.hh"

CLICK_DECLS

PacketDecompression::PacketDecompression()
{
  BRNElement::init();
}

PacketDecompression::~PacketDecompression()
{
}

int
PacketDecompression::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "CMODE", cpkP+cpkM, cpUnsigned, &cmode,
      "DEBUG", cpkP, cpUnsigned, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
PacketDecompression::initialize(ErrorHandler *)
{
  ethertype = COMPRESSION_ETHERTYPE;
  return 0;
}

void
PacketDecompression::push( int /*port*/, Packet *packet )
{
  int resultsize;
  WritablePacket *p = packet->uniqueify();

  switch (cmode) {
    case COMPRESSION_MODE_FULL: {
      struct compression_header *ch = (struct compression_header *)p->data();
      int uncompsize = ntohs(ch->uncompressed_len);
      bool is_lzw = false;

      if ( uncompsize <= MAX_COMPRESSION_BUFFER ) {
        if ( ch->compression_type == COMPRESSION_LZW ) {
          resultsize = lzw.decode(&(p->data()[sizeof(struct compression_header)]), p->length() - sizeof(struct compression_header), compbuf, uncompsize);
          is_lzw = true;
        } else if ( ch->compression_type == COMPRESSION_STRIP ) {
          resultsize = uncompsize;
        }

        if ( resultsize == uncompsize ) {
          p->pull(sizeof(struct compression_header));
          p = p->put(uncompsize - p->length());
          if ( is_lzw ) {
            memcpy(p->data(), compbuf, uncompsize );
          }
          BRN_INFO("Decode: Uncomplen %d Resultlen: %d.", uncompsize, resultsize);
          output(DECOMP_OUTPORT).push(p);
        } else {
          BRN_INFO("Decode error. Decompsize should be: %d Resultlen: %d.", uncompsize, resultsize);
          output(DECOMP_ERROR_OUTPORT).push(packet);
        }
      } else {
        BRN_INFO("Decompsize too big: %d.", uncompsize);
        output(DECOMP_ERROR_OUTPORT).push(packet);
      }
      break;
    }
    case COMPRESSION_MODE_ETHERNET:
    {
      unsigned char macbuf[sizeof(click_ether)];
      memcpy(macbuf, p->data(), sizeof(click_ether));            //save mac-addresses

      struct compression_header *ch = (struct compression_header *)&(p->data()[sizeof(click_ether)]);
      int uncompsize = ntohs(ch->uncompressed_len);

      if ( uncompsize <= MAX_COMPRESSION_BUFFER ) {
        resultsize = lzw.decode(&(p->data()[sizeof(struct compression_header) + sizeof(struct click_ether)]),
                                  p->length() - (sizeof(struct compression_header) + sizeof(struct click_ether)), compbuf, uncompsize );

        if ( resultsize == uncompsize ) {
          p->pull(sizeof(struct compression_header) + sizeof(struct click_ether)); //remove mac + compression header
          p = p->put(resultsize - p->length());
          memcpy(p->data(), compbuf, resultsize );

          p = p->push(sizeof(click_ether) - sizeof(uint16_t));  //add mac  //TODO: short this ( don't pull than push again, use 1 pull )
          memcpy(p->data(), macbuf, sizeof(click_ether) - sizeof(uint16_t));

          output(DECOMP_OUTPORT).push(p);
        } else {
          output(DECOMP_ERROR_OUTPORT).push(packet);
        }
      } else {
        output(DECOMP_ERROR_OUTPORT).push(packet);
      }
      break;
    }
    case COMPRESSION_MODE_BRN: {
      uint16_t *et;
      struct compression_header *ch = (struct compression_header *)p->data();
      int uncompsize = ntohs(ch->uncompressed_len);

      if ( uncompsize <= MAX_COMPRESSION_BUFFER ) {
        resultsize = lzw.decode(&(p->data()[sizeof(struct compression_header)]), p->length() - sizeof(struct compression_header), compbuf, uncompsize);

        if ( resultsize == uncompsize ) {
          p->pull(sizeof(struct compression_header));
          p = p->put(uncompsize - p->length());
          memcpy(p->data(), compbuf, uncompsize );

          et = (uint16_t*)p->data();
          BRNPacketAnno::set_ethertype_anno(p,*et);
          p->pull(sizeof(uint16_t));

          output(DECOMP_OUTPORT).push(p);
        } else {
          output(DECOMP_ERROR_OUTPORT).push(p);
        }
      } else {
        output(DECOMP_ERROR_OUTPORT).push(p);
      }
    }
  }
}

void
PacketDecompression::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PacketDecompression)
