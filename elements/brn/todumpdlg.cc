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
#include <click/glue.hh>
#include "todumpdlg.hh"
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <elements/userlevel/todump.hh>
CLICK_DECLS

ToDumpDlg::ToDumpDlg()
    : _to_dump(NULL)
{
}

ToDumpDlg::~ToDumpDlg()
{
}

int
ToDumpDlg::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_kparse(conf, this, errh,
        cpElement, "Element to print to", &_to_dump,
		    cpEnd) < 0)
	return -1;

  if (NULL == _to_dump || !_to_dump->cast("ToDump")) 
    return errh->error("ToDumpDlg not specified");
    
  return 0;
}

Packet* 
ToDumpDlg::simple_action(Packet* p) 
{
/*  if (NULL != _to_dump) {
    _to_dump->write_packet(p);
  }
*/
  return p;  
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns)
EXPORT_ELEMENT(ToDumpDlg)
