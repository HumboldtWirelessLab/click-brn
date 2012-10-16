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
#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/straccum.hh>

#include "elements/brn2/standard/brn_md5.hh"
#include "elements/brn2/dht/standard/dhtnode.hh"
#include "elements/brn2/dht/standard/dhtnodelist.hh"
#include "dart_functions.hh"

CLICK_DECLS

String
DartFunctions::print_id(md5_byte_t *digest, int len)
{
  StringAccum sa;

  sa << "->";

  for ( int i = 0; i < len; i++ )
    sa << ( ( digest[ i / 8 ] >> ( i % 8 ) ) & 1 );

  sa << "<- (" << len << ")";

  return sa.take_string();
}

String
DartFunctions::print_id(DHTnode* n)
{
  return print_id(n->_md5_digest, n->_digest_length);
}

String
DartFunctions::print_raw_id(md5_byte_t *digest, int len)
{
  StringAccum sa;

  for ( int i = 0; i < len; i++ ) sa << ( ( digest[ i / 8 ] >> ( i % 8 ) ) & 1 );

  return sa.take_string();
}

String
DartFunctions::print_raw_id(DHTnode* n)
{
  return print_raw_id(n->_md5_digest, n->_digest_length);
}


bool
DartFunctions::has_max_id_length(DHTnode *n)
{
  return n->_digest_length == MAX_DIGEST_LENGTH;
}

void
DartFunctions::copy_id(DHTnode *dst, DHTnode *src)
{
  memcpy(dst->_md5_digest, src->_md5_digest, MAX_NODEID_LENTGH);
  dst->_digest_length = src->_digest_length;
}

void
DartFunctions::append_id_bit(DHTnode *node, int bit)
{
  if ( node->_digest_length == MAX_DIGEST_LENGTH )
    click_chatter("No space left in nodeid !");
  else {
    node->_md5_digest[node->_digest_length / 8] |= (bit & 1) << (node->_digest_length % 8);  //id[len/8] = id[len/8] | (bit << (len % 8))
    node->_digest_length++;
  }
}

bool
DartFunctions::equals(md5_byte_t *a, md5_byte_t *b, int len)
{
  for ( int i = 0; i < (len / 8); i++ ) if ( a[i] != b[i] ) return false;
  for ( int i = 0; i < (len % 8); i++ ) if ( (a[len/8] & (1 << i)) != (b[len/8] & (1 << i)) ) return false;

  return true;
}

bool
DartFunctions::equals(DHTnode *node, md5_byte_t *key)
{
  return equals(node->_md5_digest, key, node->_digest_length);
}

bool
DartFunctions::equals(DHTnode *node, md5_byte_t *key, int len)
{
  return equals(node->_md5_digest, key, len);
}


bool
DartFunctions::equals(DHTnode *a, DHTnode *b, int len)
{
  if ( (a->_digest_length < len) || (b->_digest_length < len) ) return false;

  return equals(a->_md5_digest, b->_md5_digest, len);
}

bool
DartFunctions::equalBit(DHTnode *a, DHTnode *b, int len)
{
  if ( (a->_digest_length < len) && (b->_digest_length < len) ) return false;
  return ((a->_md5_digest[len/8] & (1 << (len%8))) == (b->_md5_digest[len/8] & (1 << (len%8))));
}


int
DartFunctions::diff_bit(DHTnode *a, md5_byte_t *key)
{
  for ( int i = 0; i < a->_digest_length; i++ )
    if ( (a->_md5_digest[i/8] & (1 << (i%8))) != (key[i/8] & (1 << (i%8))) ) return i;

  return -1;
}

int
DartFunctions::position_last_1(DHTnode *a)
{
  for ( int i = (a->_digest_length  - 1); i >= 0; i-- )
    if ( (a->_md5_digest[i/8] & (1 << (i%8))) != 0 ) return i;
  return -1;
}

int
DartFunctions::sibling_position(DHTnode *a, DHTnode *b)
{
  int minlen,i;

  if ( a->_digest_length < b->_digest_length ) minlen = a->_digest_length;
  else minlen = b->_digest_length;

  for ( i = 0; i < minlen; i++ )
    if ( (a->_md5_digest[i/8] & (1 << (i%8))) != (b->_md5_digest[i/8] & (1 << (i%8))) ) break;

  if ( (i+1) == minlen ) return i;

  for ( int ai = i; ai < a->_digest_length; ai++ )
    if ( (a->_md5_digest[ai/8] & (1 << (ai%8))) != 0 ) return -1;

  for ( int bi = i; bi < b->_digest_length; bi++ )
    if ( (b->_md5_digest[bi/8] & (1 << (bi%8))) != 0 ) return -1;

  return i;
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(DartFunctions)
