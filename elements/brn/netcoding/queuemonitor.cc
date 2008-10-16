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
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "queuemonitor.hh"

CLICK_DECLS

int QueueMonitor::configure ( Vector<String> & conf, ErrorHandler * errh ) {
	if ( cp_va_kparse ( conf, this, errh,
	                   cpKeywords, "QUEUE", cpElement, "monitored queue", &queue,
	                   "CLIENT", cpElement, "notified client", &client,
	                   cpEnd ) < 0 )
		return -1;
	if ( !queue || !queue->cast ( "SimpleQueue" ) )
		return -1;
	if ( !client || !client->cast ( "NotificationClient" ) )
		return -1;
	return 0;
}

Packet * QueueMonitor::pull(int) {
	client->notify();
	Packet * p = input(0).pull();
	if (p == NULL)
		BRN_DEBUG("pulled NULL");
	if (queue->size() == 0)
		BRN_DEBUG("queue drained");
	return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(QueueMonitor)
