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
 * PacketCompression.{cc,hh}
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
#include "lzw.hh"

CLICK_DECLS

PacketCompression::PacketCompression() :
 _compression(COMPRESSION_LZW)
{
  BRNElement::init();
}

PacketCompression::~PacketCompression()
{
}

int
PacketCompression::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "CMODE", cpkP+cpkM, cpUnsigned, &cmode,
      "COMPRESSION", cpkP, cpUnsigned, &_compression,
      "STRIPLEN", cpkP, cpUnsigned, &_strip_len,
      "DEBUG", cpkP, cpUnsigned, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
PacketCompression::initialize(ErrorHandler *)
{
  ethertype = COMPRESSION_ETHERTYPE;
  return 0;
}

void
PacketCompression::push( int /*port*/, Packet *packet )
{
  int resultsize;
  int oldlen;
  unsigned char *data;
  struct compression_header *ch;
  WritablePacket *p = packet->uniqueify();

  oldlen = p->length();
  if ( oldlen > MAX_COMPRESSION_BUFFER ) {
    output(COMP_ERROR_OUTPORT).push(packet);
    return;
  }

  switch (cmode) {
    case COMPRESSION_MODE_FULL: {
      if ( _compression == COMPRESSION_LZW ) {
        resultsize = lzw.encode(p->data(), oldlen, compbuf, MAX_COMPRESSION_BUFFER);
        if ( ( resultsize < oldlen ) && ( resultsize > 0 ) ) {
          p->take(p->length() - resultsize);
          memcpy(p->data(), compbuf, resultsize);
        }
      } else if ( _compression == COMPRESSION_STRIP ) {
        if ( oldlen > _strip_len ) {
          p->take(oldlen - _strip_len);
          resultsize=_strip_len;
        } else {
          resultsize = oldlen;
        }
      }

      if ( ( resultsize < oldlen ) && ( resultsize > 0 ) ) {
        p = p->push(sizeof(struct compression_header));
        ch = (struct compression_header *)p->data();
        ch->marker = COMPRESSION_MARKER;
        ch->compression_mode = COMPRESSION_MODE_FULL;
        ch->compression_type = _compression;
        ch->uncompressed_len = htons(oldlen);
        output(COMP_OUTPORT).push(p);
      } else {
        output(COMP_ERROR_OUTPORT).push(packet);
      }
      break;
    }
    case COMPRESSION_MODE_ETHERNET:
    case COMPRESSION_MODE_BRN:
    {
      oldlen -= 12;
      resultsize = lzw.encode(p->data() + 12, oldlen, compbuf, MAX_COMPRESSION_BUFFER);

      if ((resultsize < oldlen) && ( resultsize > 0 )) {
        unsigned char macbuf[12];
        memcpy(macbuf, p->data(), 12);               //save mac-addresses
        p->take(p->length() - resultsize);           //reduce packetsize (Compression)
        memcpy(p->data() + 12, compbuf, resultsize); //insert compressed data

        if ( cmode == COMPRESSION_MODE_ETHERNET ) {
          p = p->push(sizeof(struct compression_header) + sizeof(uint16_t) /*Ether type*/); //space for mac, ethertype, ...
          memcpy(p->data(), macbuf, 12);
          data = (unsigned char*)p->data();
          memcpy(&data[12],&ethertype, sizeof(uint16_t));
          ch = (struct compression_header*)&data[14];
          ch->marker = COMPRESSION_MARKER;
          ch->compression_mode = COMPRESSION_MODE_ETHERNET;
          ch->compression_type = COMPRESSION_LZW;
          ch->uncompressed_len = htons(oldlen);
        }

        if ( cmode == COMPRESSION_MODE_BRN ) {
          p->pull(12);
          p = p->push(sizeof(struct compression_header));
          ch = (struct compression_header*)p->data();
          ch->marker = COMPRESSION_MARKER;
          ch->compression_mode = COMPRESSION_MODE_BRN;
          ch->compression_type = COMPRESSION_LZW;
          ch->uncompressed_len = htons(oldlen);
          p = BRNProtocol::add_brn_header(p, BRN_PORT_COMPRESSION, BRN_PORT_COMPRESSION, 255, 0);
          p = p->push(sizeof(click_ether));
          click_ether *ether = (click_ether *)p->data();
          memcpy(ether->ether_shost, &macbuf[6], 6);
          memcpy(ether->ether_dhost, macbuf, 6);
          ether->ether_type = htons(ETHERTYPE_BRN);
        }
        output(COMP_OUTPORT).push(p);
      } else {
        output(COMP_ERROR_OUTPORT).push(packet);
      }
      break;
    }
  }
}

void
PacketCompression::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PacketCompression)
