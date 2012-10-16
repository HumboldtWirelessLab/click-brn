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
#include "elements/brn/brnelement.hh"


CLICK_DECLS

class BrnPushHandler : public BRNElement
{
  public:
    BrnPushHandler();
    virtual ~BrnPushHandler();

  public:
    const char *class_name() const  {return "BrnPushHandler";}

    const char *processing() const  { return PUSH; }
    const char *port_count() const  { return "0/1"; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void add_handlers();

    static void static_push_handler_timer_hook(Timer *, void *);
    void push_handler();

    String _handler;
    int _period;

  private:

    Timer _pushhandler_timer;
  public:
    void set_period(int period) {
      if ( _period != 0 ) _pushhandler_timer.unschedule();
      _period = period;
      if ( _period != 0 ) _pushhandler_timer.schedule_after_msec(_period);
    }
};

CLICK_ENDDECLS

#endif /*BRNPUSHHANDLER_HH_*/
