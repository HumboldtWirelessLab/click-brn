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

#ifndef DHT_DART_FUNCTIONS_HH
#define DHT_DART_FUNCTIONS_HH

#include <click/element.hh>

#include "elements/brn/dht/standard/dhtnode.hh"
#include "elements/brn/dht/standard/dhtnodelist.hh"

CLICK_DECLS

class DartFunctions {
  public:

    static String print_id(DHTnode* n);
    static String print_id(md5_byte_t *digest, uint16_t len);
    static String print_raw_id(DHTnode* n);
    static String print_raw_id(md5_byte_t *digest, uint16_t len);
    static bool has_max_id_length(DHTnode *n);
    static void copy_id(DHTnode *dst, DHTnode *src);
    static void append_id_bit(DHTnode *node, uint16_t bit);

    static bool equals(md5_byte_t *a, md5_byte_t *b, uint16_t len);
    static bool equals(DHTnode *node, md5_byte_t *key);
    static bool equals(DHTnode *node, md5_byte_t *key, uint16_t len);
    static bool equals(DHTnode *a, DHTnode *b, uint16_t len);
    static bool equalBit(DHTnode *a, DHTnode *b, uint16_t len);

    static int diff_bit(DHTnode *a, md5_byte_t *key);
    static int position_last_1(DHTnode *a);
    static int position_first_0(DHTnode *a);
    static int sibling_position(DHTnode *a, DHTnode *b);


};

CLICK_ENDDECLS
#endif
