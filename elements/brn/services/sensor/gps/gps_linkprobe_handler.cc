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
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/bighashmap.hh>
#include <click/error.hh>
#include <click/userutils.hh>
#include <click/timer.hh>

#include <elements/brn/brn2.h>
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include <elements/brn/brnprotocol/brnpacketanno.hh>
#include "elements/brn/routing/identity/brn2_device.hh"

#include "gps_linkprobe_handler.hh"

CLICK_DECLS

GPSLinkprobeHandler::GPSLinkprobeHandler()
 : _linkstat(NULL),_gps(NULL),_gpsmap(NULL)
{
  BRNElement::init();
}

GPSLinkprobeHandler::~GPSLinkprobeHandler()
{
}

int
GPSLinkprobeHandler::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret = cp_va_kparse(conf, this, errh,
                     "LINKSTAT", cpkP+cpkM, cpElement, &_linkstat,
                     "GPS", cpkP+cpkM, cpElement, &_gps,
                     "GPSMAP", cpkP+cpkM, cpElement, &_gpsmap,
                     "DEBUG", cpkP, cpInteger, &_debug,
                     cpEnd);

  return ret;
}

static int
tx_handler(void *element, const EtherAddress *ea, char *buffer, int size)
{
  GPSLinkprobeHandler *gpsh = reinterpret_cast<GPSLinkprobeHandler*>(element);
  return gpsh->lpSendHandler(buffer, size, ea);
}

static int
rx_handler(void *element, EtherAddress *ea, char *buffer, int size, bool /*is_neighbour*/, uint8_t /*fwd_rate*/, uint8_t /*rev_rate*/)
{
  GPSLinkprobeHandler *gpsh = reinterpret_cast<GPSLinkprobeHandler*>(element);
  return gpsh->lpReceiveHandler(buffer, size, ea);
}

int
GPSLinkprobeHandler::initialize(ErrorHandler *)
{
  _linkstat->registerHandler(this,BRN2_LINKSTAT_MINOR_TYPE_GPS,&tx_handler,&rx_handler);
  return 0;
}

int
GPSLinkprobeHandler::lpSendHandler(char *buffer, int size, const EtherAddress */*ea*/)
{
  struct gps_position *pos = (struct gps_position *)buffer;

  if ( size < (int)sizeof(struct gps_position) ) {
    BRN_WARN("No Space for GPSPosition in Linkprobe");
    return 0;
  }

  _gps->getPosition()->getPosition(pos);

  return sizeof(struct gps_position);
}

int
GPSLinkprobeHandler::lpReceiveHandler(char *buffer, int size, EtherAddress *ea)
{
  struct gps_position *pos = (struct gps_position *)buffer;
  _gpsmap->insert(*ea, GPSPosition(pos));

  return size;
}


/**************************************************************************************************************/
/********************************************** H A N D L E R *************************************************/
/**************************************************************************************************************/

void
GPSLinkprobeHandler::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(GPSLinkprobeHandler)
ELEMENT_MT_SAFE(GPSLinkprobeHandler)
