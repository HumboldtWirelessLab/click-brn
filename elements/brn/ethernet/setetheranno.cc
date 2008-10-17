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
 * etherdecap.{cc,hh} -- removes Ethernet header from packets
 */

#include <click/config.h>
#include "setetheranno.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
CLICK_DECLS

SetEtherAnno::SetEtherAnno()
{
}

SetEtherAnno::~SetEtherAnno()
{
}

int
SetEtherAnno::configure(Vector<String> &conf, ErrorHandler *errh)
{
	_offset = 0;

  if (cp_va_parse_remove_keywords(conf, 0, this, errh,
		"OFFSET", cpUnsigned, "Ether header offset", &_offset,
		cpEnd) < 0)
    return -1;

  if (conf.size() == 1)
  	cp_unsigned(conf[0], &_offset);
  	
  return 0;

}


Packet *
SetEtherAnno::simple_action(Packet *p)
{
//click_chatter("anno = %s\n", p->udevice_anno());

  click_ether *ether = (click_ether *) (p->data() + _offset);
  p->set_ether_header(ether);

//  click_chatter(" * %s -> %s\n", EtherAddress(ether->ether_shost).unparse().c_str(), EtherAddress(ether->ether_dhost).unparse().c_str());

//click_chatter("%s\n", p->udevice_anno());
  return p;
}


EXPORT_ELEMENT(SetEtherAnno)
CLICK_ENDDECLS
