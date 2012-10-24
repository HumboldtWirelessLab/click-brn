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

#ifndef CLICK_GPSLINKPROBEHANDLER_HH
#define CLICK_GPSLINKPROBEHANDLER_HH
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"
#include "elements/brn/brnelement.hh"
#include "gps_map.hh"
#include "gps.hh"


CLICK_DECLS

/*
=c
GPSLinkprobeHandler()

=d

=a

*/

class GPSLinkprobeHandler : public BRNElement {

  public:

    GPSLinkprobeHandler();
    ~GPSLinkprobeHandler();

    const char *class_name() const  { return "GPSLinkprobeHandler"; }
    const char *port_count() const  { return "0/0"; }

    int configure(Vector<String> &conf, ErrorHandler* errh);
    int initialize(ErrorHandler *);

    void add_handlers();

    int lpSendHandler(char *buffer, int size, const EtherAddress *ea);
    int lpReceiveHandler(char *buffer, int size, EtherAddress *ea);

  private:

    BRN2LinkStat *_linkstat;
    GPS *_gps;
    GPSMap *_gpsmap;

};

CLICK_ENDDECLS
#endif
