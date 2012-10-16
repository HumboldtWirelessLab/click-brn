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

#ifndef BRNGATEWAYDECAP_H_
#define BRNGATEWAYDECAP_H_

#include "elements/brn/brnelement.hh"

CLICK_DECLS

class BRNGateway;
class BRN2LinkTable;


class BRNGatewayDecap : public BRNElement {
  public:

    BRNGatewayDecap();
    ~BRNGatewayDecap();

    const char *class_name() const { return "BRNGatewayDecap"; }

    const char *port_count() const {
        return "1/1";
    }

    const char *processing() const {
        return PUSH;
    }

    int initialize(ErrorHandler *);
    int configure(Vector<String> &conf, ErrorHandler *errh);
    void add_handlers();

    void push(int, Packet *);


  private:
    BRNGateway *_gw; // the gateway element, which stores infos about known hosts
};


CLICK_ENDDECLS
#endif
