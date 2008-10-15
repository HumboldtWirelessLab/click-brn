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

#ifndef QUEUEMONITOR_HH
#define QUEUEMONITOR_HH
#include <click/element.hh>
#include <elements/brn/common.hh>
#include <elements/standard/simplequeue.hh>
#include "notificationclient.hh"
CLICK_DECLS

/*
 * =c
 * TxQueueMonitor()
 *
 * =s netcoding
 * monitor the transmission queue and send notifying packets
 * =d
 * TxQueueMonitor pushes a small notification about the length of the transmission queue
 * whenever a packet is pulled. The pulled packet is passed on unmodified
 *
 * TxQueueMonitor's input and output are pull.
 *
 */

class QueueMonitor : public Element
{

	public:

		QueueMonitor() : _debug(BrnLogger::DEFAULT), queue(0), client(0) {}
		~QueueMonitor() {}

		const char *class_name() const		{ return "QueueMonitor"; }
		const char *port_count() const		{ return "1/1"; }
		const char *processing() const		{ return "PULL"; }

		int configure ( Vector<String> &, ErrorHandler * );

		Packet *pull ( int );
		
	private:
		int _debug;
		SimpleQueue * queue;
		NotificationClient * client;
};


CLICK_ENDDECLS
#endif
