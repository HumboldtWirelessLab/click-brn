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
 * brnpushhandler.{cc,hh} -- coordinates the push-mechanism for information-retrieving of clickwatch
 * R. Sombrutzki & M. Kurth
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/timer.hh>

#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "brnpushhandler.hh"
#include <click/handlercall.hh>
CLICK_DECLS



BrnPushHandler::BrnPushHandler():
  _period(1000),
  _pushhandler_timer(static_push_handler_timer_hook,(void*)this)
{
    BRNElement::init();
}

BrnPushHandler::~BrnPushHandler()
{
}

int
BrnPushHandler::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
                    "HANDLER", cpkP+cpkM, cpString, &_handler,
                    "PERIOD", cpkP, cpInteger, &_period,
                    "DEBUG", cpkP, cpInteger, &_debug,
                    cpEnd) < 0)
    return -1;

  return 0;
}

int
BrnPushHandler::initialize(ErrorHandler */*errh*/)
{

  BRN_DEBUG(" * BrnPushHandler: processing handler %s.", _handler.c_str() );

  _pushhandler_timer.initialize(this);
  _pushhandler_timer.schedule_after_msec(_period);

  return 0;
}

void
BrnPushHandler::static_push_handler_timer_hook(Timer *, void *f)
{
  ((BrnPushHandler*)f)->push_handler();
}

void
BrnPushHandler::push_handler()
{

  String handler_output = HandlerCall::call_read(_handler,router()->root_element(), ErrorHandler::default_handler());

  click_chatter("%s\n%d",handler_output.c_str(), handler_output.length());

  WritablePacket *p = WritablePacket::make(64, handler_output.data(), handler_output.length(), 32);

  if (p) output(0).push(p);

  _pushhandler_timer.schedule_after_msec(_period);

}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

enum { H_PERIOD, H_HANDLER};

static String
BrnPushHandler_read_param(Element *e, void *vparam)
{
  BrnPushHandler *ph = (BrnPushHandler *)e;
  StringAccum sa;


  switch((intptr_t)vparam) {
    case H_PERIOD:
    case H_HANDLER: {
      sa << "<pushhandler period=\"" << ph->_period << "\" handler=\"" << ph->_handler << "\" />\n";
      return sa.take_string();
    }
  }
  return String();
}

static int
BrnPushHandler_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler */*errh*/)
{
  BrnPushHandler *ph = (BrnPushHandler *)e;

  switch((intptr_t)vparam) {
    case H_PERIOD: {
      cp_integer(in_s, &(ph->_period));
      break;
    }
    case H_HANDLER: {
      ph->_handler = in_s;
      break;
    }
  }

  return 0;
}

void
BrnPushHandler::add_handlers()
{
  BRNElement::add_handlers(); //what if handler has the name "debug"

  add_read_handler( "handler", BrnPushHandler_read_param, H_HANDLER);
  add_read_handler( "period", BrnPushHandler_read_param, H_PERIOD);

  add_write_handler( "handler", BrnPushHandler_write_param, H_HANDLER);
  add_write_handler( "period", BrnPushHandler_write_param, H_PERIOD);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnPushHandler)
