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
      cpEnd) < 0)
       return -1;

  return 0;
}

int
PacketCompression::initialize(ErrorHandler *)
{
  return 0;
}

Packet *
PacketCompression::simple_action(Packet *) {
  return NULL;
}

void
PacketCompression::compression_test() {
  unsigned char foo[2000];
  unsigned char bar[300000];
  unsigned char zet[4000];

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
