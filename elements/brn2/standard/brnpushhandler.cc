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
	_pushhandler_timer(static_lookup_timer_hook,(void*)this),
	_period(1000)
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
                     "HANDLER", cpkP, cpString, &_handler,
                     "PERIOD", cpkP, cpInteger, &_period,
                     "DEBUG", cpkP, cpInteger, &_debug,
                     cpEnd) < 0)
        return -1;
    /*
      * The location of handlers is carried out in 'initialize', because
      * add_handlers is called after configure, but before initialize.
      */
    return 0;
}


int
BrnPushHandler::initialize(ErrorHandler *errh)
{

	BRN_DEBUG(" * BrnPushHandler: processing handler %s.", _handler.c_str() );

	_pushhandler_timer.initialize(this);
	_pushhandler_timer.schedule_after_msec(_period);

	return 0;
}


void
BrnPushHandler::static_lookup_timer_hook(Timer *, void *f)
{
  ((BrnPushHandler*)f)->test();
}

void
BrnPushHandler::test()
{

  String handler_output = HandlerCall::call_read(_handler,router()->root_element(), ErrorHandler::default_handler());

  click_chatter("%s\n%d",handler_output.c_str(), handler_output.length());
  _pushhandler_timer.schedule_after_msec(_period);
}












void
BrnPushHandler::set_value( const String& value, ErrorHandler *errh )
{
  _classes_value = value;

  for ( int i = 0; i < _vec_elements_handlers.size(); i++ )
  {
    const Handler* pHandler = _vec_elements_handlers[i];
    Element* pElement = _vec_elements[i];

    BRN_DEBUG(" * BrnPushHandler: calling handler %s on element %s with value %s.",
              _classes_handler.c_str(), pElement->declaration().c_str(), _classes_value.c_str() );

    ContextErrorHandler cerrh( errh, String("In write handler '" +
                                            pHandler->unparse_name(pElement) + "':").c_str());

    pHandler->call_write( _classes_value, pElement, &cerrh );
  }
}

String
BrnPushHandler::read_handler()
{
    StringAccum sa;

    sa << "<compoundhandler>\n";
    for ( int j = 0; j < _vec_handlers.size(); j++) {
        sa << "\t<handler name=\"" << _vec_handlers[j] << "\">\n";
        sa << "\t" << HandlerCall::call_read(_vec_handlers[j], router()->root_element(),
                                             ErrorHandler::default_handler()).c_str();
        sa << "\t</handler>\n";
    }
    sa << "</compoundhandler>\n";

    return sa.take_string();
}

String
BrnPushHandler::handler()
{
    StringAccum sa;

    sa << "<compoundhandlerinfo >\n";
    for ( int j = 0; j < _vec_handlers.size(); j++) {
        sa << "\t<handler name=\"" << _vec_handlers[j] << "\" />\n";
    }
    sa << "</compoundhandlerinfo>\n";

    return sa.take_string();
}


enum { H_HANDLER_INSERT, H_HANDLER_REMOVE};

int
BrnPushHandler::handler_operation(const String &in_s, void *vparam, ErrorHandler */*errh*/)
{
    switch((intptr_t)vparam) {
    case H_HANDLER_INSERT:
    {
        BRN_DEBUG("Search handler %s to avoid dups",in_s.c_str());
        int i = 0;
        for( ; i < _vec_handlers.size(); i++ ) {
            if ( in_s == _vec_handlers[i]) {
                BRN_DEBUG("Found handler %s: index %d (%s).",in_s.c_str(),i,_vec_handlers[i].c_str());
                break;
            }
        }
        if ( i == _vec_handlers.size() ) {
            BRN_DEBUG("Didn't found handler %s. Insert.",in_s.c_str());
            _vec_handlers.push_back(in_s);
        }
        break;
    }
    case H_HANDLER_REMOVE:
    {
        for( int i = 0; i < _vec_handlers.size(); i++ ) {
            if ( in_s == _vec_handlers[i] ) {
                _vec_handlers.erase(_vec_handlers.begin() + i);
                break;
            }
        }
        break;
    }
    default:
    {}
    }
    return 0;
}


//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
BrnPushHandler_read_handler(Element *e, void */*thunk*/)
{
    return ((BrnPushHandler*)e)->read_handler();
}

static String
BrnPushHandler_handler(Element *e, void */*thunk*/)
{
    return ((BrnPushHandler*)e)->handler();
}

static int
BrnPushHandler_handler_operation(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
    return ((BrnPushHandler*)e)->handler_operation(in_s, vparam, errh);
}

static String
BrnPushHandler_read_param(Element */*e*/, void */*thunk*/)
{
    return String();
}

static int
BrnPushHandler_write_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
    BrnPushHandler *ch = (BrnPushHandler *)e;

    String s = cp_uncomment(in_s);
    ch->set_value(s, errh);

    return( 0 );
}

void
BrnPushHandler::add_handlers()
{
    //BRNElement::add_handlers(); //what if handler has the name "debug"

    if ( _classes_handler.length() > 0 ) {
        add_read_handler( _handler, BrnPushHandler_read_param, 0);
        add_write_handler( _handler, BrnPushHandler_write_param, 0);
    }

    add_read_handler( "read", BrnPushHandler_read_handler, 0);

    add_read_handler( "handler", BrnPushHandler_handler, 0);
    add_write_handler( "insert", BrnPushHandler_handler_operation, H_HANDLER_INSERT);
    add_write_handler( "remove", BrnPushHandler_handler_operation, H_HANDLER_REMOVE);
}



CLICK_ENDDECLS
EXPORT_ELEMENT(BrnPushHandler)
