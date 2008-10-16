/* Copyright (C) 2008 Ulf Hermann
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
 */

#include <click/config.h>
#include <elements/brn/common.hh>
#include "netcoding.h"
#include "tracereporter.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <click/router.hh>

CLICK_DECLS


int TraceReporter::configure(Vector<String> & conf, ErrorHandler * errh) {
  mode = FORWARD;
  parent = NULL;
  if (cp_va_kparse(conf, this, errh,
                  cpKeywords, 
                  "MODE", cpInteger, "mode", &mode,
                  "TRACE_COLLECTOR", cpElement, "trace collector", &parent,
                  cpEnd) < 0)
    return -1;
  if (!parent || !parent->cast("TraceCollector"))
    return -1;
  return 0;
}
 
 
Packet * TraceReporter::simple_action(Packet * p) {
	click_brn_dsr * dsr = (click_brn_dsr *)(p->data() + sizeof(click_brn));
	switch (mode) { 
		case FORWARD:
			parent->forwarded(dsr, 0);
			break;
		case DUPE:
			parent->duplicate(dsr, 0);
			break;
		case DROP:
			parent->corrupt(dsr, 0);
			break;
	}
	return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TraceReporter);
