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

#ifndef BRNPUSHHANDLER_HH_
#define BRNPUSHHANDLER_HH_

#include <click/element.hh>
#include "elements/brn2/brnelement.hh"


CLICK_DECLS

class BrnPushHandler : public BRNElement
{
	public:
		BrnPushHandler();
		virtual ~BrnPushHandler();

	public:
		const char *class_name() const  {return "BrnPushHandler";}
		int configure(Vector<String> &, ErrorHandler *);
		int initialize(ErrorHandler *);
		void add_handlers();

		void set_value( const String& value, ErrorHandler *errh );
		String read_handler();
		String handler();
		int handler_operation(const String &in_s, void *vparam, ErrorHandler *errh);

		static void static_lookup_timer_hook(Timer *, void *);
		void test();

	private:
		String _handler;
		String _classes;
		String _classes_handler;
		String _classes_value;

		Timer _pushhandler_timer;

		Vector<String> _vec_handlers;

		Vector<Element*> _vec_elements;
		Vector<const Handler*> _vec_elements_handlers;

	    int _period;
};

CLICK_ENDDECLS

#endif /*BrnPushHANDLER_HH_*/
