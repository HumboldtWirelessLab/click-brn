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

#include "packetcompression.hh"
#include "lzw.hh"

CLICK_DECLS

PacketCompression::PacketCompression()
{
}

PacketCompression::~PacketCompression()
{
}

int
PacketCompression::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "CMODE", cpkP+cpkM, cpUnsigned, &cmode,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
PacketCompression::initialize(ErrorHandler *)
{
  ethertype = COMP_ETHERTYPE;
  return 0;
}

void
PacketCompression::push( int port, Packet *packet ) {

  LZW lzw;
  int resultsize;
  int oldlen;
  unsigned char *data;
  struct compression_header *ch;
  WritablePacket *p = packet->uniqueify();

  if ( port == 0 ) {
    oldlen = p->length();
    switch (cmode) {
      case COMPRESSION_MODE_FULL: {
        resultsize = lzw.encode(p->data(), oldlen, compbuf, MAX_COMPRESSION_BUFFER);
        if ( resultsize < oldlen ) {
          p->take(p->length() - resultsize);
          memcpy(p->data(), compbuf, resultsize);
          p = p->push(sizeof(struct compression_header));
          ch = (struct compression_header *)p->data();
          ch->marker = COMPRESSION_MARKER;
          ch->compression_mode = COMPRESSION_MODE_FULL;
          ch->compression_type = COMPRESSION_LZW;
          ch->uncompressed_len = oldlen;
        } else {
          output(0).push(packet);
          return;
        }
        break;
      }
      case COMPRESSION_MODE_ETHERNET:
      case COMPRESSION_MODE_BRN:
      {
        oldlen -= 12;
        resultsize = lzw.encode(p->data() + 12, oldlen, compbuf, MAX_COMPRESSION_BUFFER);
        if ( resultsize < oldlen ) {
          unsigned char macbuf[12];
          memcpy(macbuf, p->data(), 12);            //save mac-addresses
          p->take(p->length() - resultsize);        //reduce packetsize (Compression)
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
            ch->uncompressed_len = oldlen;
          }

          if ( cmode == COMPRESSION_MODE_BRN ) {
            p->pull(12);
            p = p->push(sizeof(struct compression_header));
            ch = (struct compression_header*)p->data();
            ch->marker = COMPRESSION_MARKER;
            ch->compression_mode = COMPRESSION_MODE_BRN;
            ch->compression_type = COMPRESSION_LZW;
            ch->uncompressed_len = oldlen;
            p = BRNProtocol::add_brn_header(p, BRN_PORT_COMPRESSION, BRN_PORT_COMPRESSION, 255, 0);
            p = p->push(sizeof(click_ether));
            click_ether *ether = (click_ether *)p->data();
            memcpy(ether->ether_shost, &macbuf[6], 6);
            memcpy(ether->ether_dhost, macbuf, 6);
            ether->ether_type = htons(ETHERTYPE_BRN);
          }

        }
        break;
      }
    }

    output(0).push(p);

  } else {
    switch (cmode) {
      case COMPRESSION_MODE_FULL: {
        p->pull(sizeof(struct compression_header));
        resultsize = lzw.decode(p->data(), p->length(), compbuf, MAX_COMPRESSION_BUFFER);
        p = p->put(resultsize - p->length());
        memcpy(p->data(), compbuf, resultsize );
        break;
      }
      case COMPRESSION_MODE_ETHERNET:
      {
        unsigned char macbuf[12];
        memcpy(macbuf, p->data(), 12);            //save mac-addresses
        p->pull(sizeof(struct compression_header) + 14); //remove mac + compression header

        resultsize = lzw.decode(p->data(), p->length(), compbuf, MAX_COMPRESSION_BUFFER);

        p = p->put(resultsize - p->length());
        memcpy(p->data(), compbuf, resultsize );

        p = p->push(12); //add mac  //TODO: short this ( don't pull than push again, use 1 pull )
        memcpy(p->data(), macbuf, 12 );

        break;
      }
      case COMPRESSION_MODE_BRN: {
        uint16_t *et;
        p->pull(sizeof(struct compression_header));
        resultsize = lzw.decode(p->data(), p->length(), compbuf, MAX_COMPRESSION_BUFFER);
        p = p->put(resultsize - p->length());
        memcpy(p->data(), compbuf, resultsize );
        et = (uint16_t*)p->data();
        BRNPacketAnno::set_ethertype_anno(p,*et);
        p->pull(sizeof(uint16_t));
      }
    }
    output(1).push(p);
  }

}

void
PacketCompression::compression_test() {
  unsigned char foo[2000];               //input buffer
  unsigned char bar[300000];             //compression buffer
  unsigned char zet[4000];               //output buffer

  for ( int z = 0; z < 100000; z++ ) {
    int resultsize,resultsize2;
    int inlen = (click_random() % 2000) + 1;

  //inlen = 64;
    memset(foo,0,2000);
    memset(bar,0,300000);
    memset(zet,0,4000);

    for( int h = 0; h < inlen; h++ ) {
      foo[h] = click_random() % 256;
    }

    LZW lzw;
    LZW lzw2;

    resultsize = lzw.encode(foo,inlen,bar,300000);
    resultsize2 = lzw2.decode(bar,resultsize,zet,4000);

    click_chatter("Res: %d Res2: %d",resultsize,resultsize2);

    click_chatter("Memcmp: %d %d ( %d %d )",(inlen == resultsize2),  memcmp(foo,zet,inlen), inlen,resultsize2);

    if ((inlen != resultsize2) || (memcmp(foo,zet,inlen) != 0 ) ) {
      assert(inlen == resultsize2);
      assert(memcmp(foo,zet,inlen) != 0 );
      click_chatter("ERROR");
    }
  }
}

static String
read_debug_param(Element *e, void *)
{
  PacketCompression *fl = (PacketCompression *)e;
  return String(fl->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  PacketCompression *fl = (PacketCompression *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  fl->_debug = debug;
  return 0;
}

void
PacketCompression::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PacketCompression)
