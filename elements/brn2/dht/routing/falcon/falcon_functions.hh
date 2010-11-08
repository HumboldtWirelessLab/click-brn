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

#ifndef DHT_FALCON_FUNCTIONS_HH
#define DHT_FALCON_FUNCTIONS_HH

#include <click/element.hh>

#include "elements/brn2/dht/standard/dhtnode.hh"
#include "elements/brn2/dht/standard/dhtnodelist.hh"

CLICK_DECLS

class FalconFunctions {
  public:
    static bool is_equals(DHTnode *a, DHTnode *b);
    static bool is_equals(DHTnode *a, md5_byte_t *md5d);
    static bool is_bigger(DHTnode *a, DHTnode *b);
    static bool is_bigger(DHTnode *a, md5_byte_t *md5d);
    static bool is_bigger_or_equals(DHTnode *a, DHTnode *b);
    static bool is_bigger_or_equals(DHTnode *a, md5_byte_t *md5d);
    static bool is_smaller(DHTnode *a, md5_byte_t *md5d);
    static bool is_smaller(DHTnode *a, DHTnode *b );
    static bool is_smaller_or_equals(DHTnode *a, DHTnode *b);
    static bool is_smaller_or_equals(DHTnode *a, md5_byte_t *md5d);

    static bool is_in_between(md5_byte_t *a, md5_byte_t *b, md5_byte_t *c);
    static bool is_in_between(DHTnode *a, DHTnode *b, DHTnode *c);
    static bool is_in_between(DHTnode *a, DHTnode *b, md5_byte_t *c);
    static bool is_in_between(DHTnode *a, md5_byte_t *b, DHTnode *c);
    static bool is_in_between(md5_byte_t *a, DHTnode *b, DHTnode *c);

};

CLICK_ENDDECLS
#endif
