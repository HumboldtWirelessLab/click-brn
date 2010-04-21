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

#include "elements/brn2/standard/md5.h"
#include "elements/brn2/dht/standard/dhtnode.hh"
#include "elements/brn2/dht/standard/dhtnodelist.hh"
#include "falcon_functions.hh"

CLICK_DECLS

bool
FalconFunctions::is_equals(DHTnode *a, DHTnode *b)         //a = b ??
{
  return MD5::is_equals( a->_md5_digest, b->_md5_digest );
}

bool
FalconFunctions::is_equals(DHTnode *a, md5_byte_t *md5d)  //a = b ??
{
  return MD5::is_equals( a->_md5_digest, md5d );
}

bool
FalconFunctions::is_bigger(DHTnode *a, DHTnode *b)         //a > b ??
{
  return MD5::is_bigger( a->_md5_digest, b->_md5_digest );
}

bool
FalconFunctions::is_bigger(DHTnode *a, md5_byte_t *md5d)  //a > b ??
{
  return MD5::is_bigger( a->_md5_digest, md5d );
}

bool
FalconFunctions::is_bigger_or_equals(DHTnode *a, DHTnode *b)         //a >= b ??
{
  return MD5::is_bigger_or_equals( a->_md5_digest, b->_md5_digest );
}

bool
FalconFunctions::is_bigger_or_equals(DHTnode *a, md5_byte_t *md5d)  //a >= b ??
{
  return MD5::is_bigger_or_equals( a->_md5_digest, md5d );
}

bool
FalconFunctions::is_smaller(DHTnode *a, md5_byte_t *md5d)  //a < b ??
{
  return MD5::is_smaller( a->_md5_digest, md5d );
}

bool
FalconFunctions::is_smaller(DHTnode *a, DHTnode *b )  //a < b ??
{
  return MD5::is_smaller( a->_md5_digest,  b->_md5_digest );
}

bool
FalconFunctions::is_smaller_or_equals(DHTnode *a, DHTnode *b)         //a <= b ??
{
  return MD5::is_smaller_or_equals( a->_md5_digest, b->_md5_digest );
}

bool
FalconFunctions::is_smaller_or_equals(DHTnode *a, md5_byte_t *md5d)  //a <= b ??
{
  return MD5::is_smaller_or_equals( a->_md5_digest, md5d );
}

/**
 * is md5 c between node a and b ??
 */

bool
FalconFunctions::is_in_between(DHTnode *a, DHTnode *b, DHTnode *c)
{
  if ( MD5::is_smaller( a->_md5_digest, b->_md5_digest ) )
    return ( MD5::is_smaller( a->_md5_digest, c->_md5_digest ) && MD5::is_bigger( b->_md5_digest, c->_md5_digest ) );

  return ( MD5::is_smaller( a->_md5_digest, c->_md5_digest ) || MD5::is_bigger( b->_md5_digest, c->_md5_digest ) );
}

bool
FalconFunctions::is_in_between(DHTnode *a, DHTnode *b, md5_byte_t *c)
{
  if ( MD5::is_smaller( a->_md5_digest, b->_md5_digest ) )
    return ( MD5::is_smaller( a->_md5_digest, c ) && MD5::is_bigger( b->_md5_digest, c ) );

  return ( MD5::is_smaller( a->_md5_digest, c ) || MD5::is_bigger( b->_md5_digest, c ) );
}

bool
FalconFunctions::is_in_between(DHTnode *a, md5_byte_t *b, DHTnode *c)
{
  if ( MD5::is_smaller( a->_md5_digest, b ) )
    return ( MD5::is_smaller( a->_md5_digest, c->_md5_digest ) && MD5::is_bigger( b, c->_md5_digest ) );

  return ( MD5::is_smaller( a->_md5_digest, c->_md5_digest ) || MD5::is_bigger( b, c->_md5_digest ) );
}

bool
FalconFunctions::is_in_between(md5_byte_t *a, DHTnode *b, DHTnode *c)
{
  if ( MD5::is_smaller( a, b->_md5_digest ) )
    return ( MD5::is_smaller( a, c->_md5_digest ) && MD5::is_bigger( b->_md5_digest, c->_md5_digest ) );

  return ( MD5::is_smaller( a, c->_md5_digest ) || MD5::is_bigger( b->_md5_digest, c->_md5_digest ) );
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(FalconFunctions)
