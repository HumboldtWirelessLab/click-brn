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
 * topology_detection.{cc,hh} -- encapsulates packet in Ethernet header
 */

#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <click/userutils.hh>

#include "elements/brn/brn2.h"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "overlay_structure.hh"
#include "circle_overlay.hh"

CLICK_DECLS

CircleOverlay::CircleOverlay() 
{
  BRNElement::init();
}

CircleOverlay::~CircleOverlay()
{
}

int
CircleOverlay::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "CIRCLEPATH", cpkP+cpkM, cpString, &_circle_path,
      "OVERLAY_STRUCTURE", cpkP+cpkM, cpElement, &_ovl,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;
  get_neighbours(_circle_path);

  return 0;
}

EtherAddress CircleOverlay::ID_to_MAC (int id) {
 uint8_t data[] = {0,0,0,0,(uint8_t)(id/256),(uint8_t)(id%256)};
  return EtherAddress(data);
}

int CircleOverlay::MAC_to_ID(EtherAddress *add) {
  const unsigned char *p = add->data();
  return (int)p[5]+256*(int)p[4];
}

int
CircleOverlay::initialize(ErrorHandler *)
{
  
  return 0;
}

void CircleOverlay::get_neighbours(String path) {
  /*Dateiformat:
   * Jeder Kreis eine Zeile
   * Zeilen terminiert durch -1
   * Bsp:
   * 1 2 3 -1
   * 2 4 5 6 7 -1
   */
  String _data = file_string(path);
  Vector<String> _data_vec;
  cp_spacevec(_data, _data_vec);

  //Hier noch einfügen: Kreise zu Overlay (einfach jedes Paar einfügen, den Rest übernimmt overlay_structure)
  
  int akt,  last=-1,first=-1, i=0;
  
  while (i < _data_vec.size()) {
    cp_integer(_data_vec[i],&akt);
    ++i;
    if (last==-1) {
		first=akt;
		last=akt;
	} else {
		if (akt==-1) {
			EtherAddress node=ID_to_MAC(last);
			EtherAddress add=ID_to_MAC(first);
			_ovl->addChild(&node,&add);
			_ovl->addParent(&add,&node);
		} else {
			EtherAddress add=ID_to_MAC(akt);
			EtherAddress node=ID_to_MAC(last);
			_ovl->addChild(&node,&add);
			_ovl->addParent(&add,&node);
		}
		last=akt;
	}
  }
  
}


/*************************************************************************************************/
/***************************************** H A N D L E R *****************************************/
/*************************************************************************************************/


void CircleOverlay::add_handlers()
{

}

CLICK_ENDDECLS
EXPORT_ELEMENT(CircleOverlay)
