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
 * setetheraddr.{cc,hh}
 */

#include <click/config.h>
#include "setetheraddr.hh"
#include <click/confparse.hh>
#include <click/error.hh>

CLICK_DECLS

SetEtherAddr::SetEtherAddr() :
	_src(EtherAddress()),
	_dst(EtherAddress())
{
}

SetEtherAddr::~SetEtherAddr()
{
}

int
SetEtherAddr::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
  		  cpOptional,
  		  cpKeywords,
		  "SRC", cpEthernetAddress, "src ether address", &_src,
		  "DST", cpEthernetAddress, "dst ether address", &_dst,
		  cpEnd) < 0)
    return -1;
  
  if (!_src && !_dst)
  	return errh->error("You have to specify at least one argument SRC or DST");
    
  return 0;
}

Packet *
SetEtherAddr::smaction(Packet *p)
{
  
  if (WritablePacket *q = p->uniqueify()) {
  	click_ether *ether = (click_ether *) q->ether_header();

  	BRN_CHECK_EXPR_RETURN(ether == NULL,
  					    ("No ether header found. Killing packet."),
  					    q->kill(); return NULL); 
  	
    if (_src) {
    	memcpy(ether->ether_shost, _src.data(), 6);
    	BRN_DEBUG("Setting src address %s.", _src.s().c_str());
    }
    	
    if (_dst) {
    	memcpy(ether->ether_dhost, _dst.data(), 6);
    	BRN_DEBUG("Setting dst address %s.", _dst.s().c_str());
    }
    	
    return q;
  }
  
  p->kill();
  return NULL;
}

void
SetEtherAddr::push(int, Packet *p)
{
  if (Packet *q = smaction(p)) {
    output(0).push(q);
  }
}

Packet *
SetEtherAddr::pull(int)
{
  if (Packet *p = input(0).pull()) {
    return smaction(p);
  } else {
    return NULL;
  }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetEtherAddr)
