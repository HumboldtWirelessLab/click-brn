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

#include "elements/brn/standard/brn_md5.hh"
#include "elements/brn/dht/standard/dhtnode.hh"
#include "elements/brn/dht/standard/dhtnodelist.hh"
#include "falcon_functions.hh"

CLICK_DECLS

/**
 * is md5 c between node a and b ??
 */


bool
FalconFunctions::is_in_between(md5_byte_t *a, md5_byte_t *b, md5_byte_t *c)
{
  if ( MD5::is_smaller( a, b ) )
    return ( MD5::is_smaller( a, c ) && MD5::is_bigger( b, c ) );

  return ( MD5::is_smaller( a, c ) || MD5::is_bigger( b, c ) );
}

bool
FalconFunctions::is_in_between(DHTnode *a, DHTnode *b, DHTnode *c)
{
  return is_in_between(a->_md5_digest, b->_md5_digest, c->_md5_digest);
}

bool
FalconFunctions::is_in_between(md5_byte_t *a, DHTnode *b, DHTnode *c)
{
  return is_in_between(a, b->_md5_digest, c->_md5_digest);
}

bool
FalconFunctions::is_in_between(DHTnode *a, md5_byte_t *b, DHTnode *c)
{
  return is_in_between(a->_md5_digest, b, c->_md5_digest);
}

bool
FalconFunctions::is_in_between(DHTnode *a, DHTnode *b, md5_byte_t *c)
{
  return is_in_between(a->_md5_digest, b->_md5_digest, c);
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(FalconFunctions)
