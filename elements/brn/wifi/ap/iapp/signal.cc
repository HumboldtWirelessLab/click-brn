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
 * signal.{cc,hh} -- handles the inter-ap protocol type data within brn
 * M. Kurth
 */
 
#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/router.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "signal.hh"

CLICK_DECLS

////////////////////////////////////////////////////////////////////////////////

Signal::Signal() :
  _debug(BrnLogger::DEFAULT),
  _active(true)
{
}

////////////////////////////////////////////////////////////////////////////////

Signal::~Signal()
{
}

////////////////////////////////////////////////////////////////////////////////
int 
Signal::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "SIGNALNAME", cpkP+cpkM, cpString, /*"signal name",*/ &_signal,
      "RECEPTIONLIST", cpkP+cpkM, cpString,/* "list of receptions",*/ &_receptions,
      "DEBUG", cpkP, cpInteger, /*"Debug",*/ &_debug,
      "ACTIVE", cpkP, cpBool, /*"",*/ &_active,
      cpEnd) < 0)
    return -1;

  if( 0 >= _signal.length() )
    return errh->error("signal not specified");

  if( _signal == String("debug") )
    return errh->error("invalid signal name");

  if( 0 >= _receptions.length() && _active )
    return errh->error("receptions not specified");
  
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int
Signal::initialize(ErrorHandler *errh)
{
  if (!_active)
    return 0;

  // Build vector of reception names
  Vector<String> vec_receptions;
  cp_spacevec( _receptions, vec_receptions );
  if( 0 >= vec_receptions.size() ) 
    return errh->error("receptions not specified");

  // Collect handlers from elements with appropriate name in a vector
  for(int j = 0; j < vec_receptions.size(); j++ )
  {
    BRN_DEBUG("processing reception %s.", vec_receptions[j].c_str() );
    Element* elem = router()->find(vec_receptions[j], errh);
    if (NULL == elem)
      return (errh->error("Element %s not found.", vec_receptions[j].c_str()));

    BRN_DEBUG("processing handler %s on element %s.", 
      _signal.c_str(), elem->name().c_str() );
    const Handler* handler = Router::handler( elem, _signal );
    if (!handler || !handler->writable())
      return errh->error("Element %s has no write handler %s.", 
        elem->name().c_str(), _signal.c_str() );

    _vec_elements.push_back(elem);
    _vec_handlers.push_back(handler);
  }

  if (0 >= _vec_handlers.size())
    return errh->error("Handler classes not specified");

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void 
Signal::send_signal_action(const String& param, ErrorHandler *errh)
{
  if (!_active)
    return;

  for( int i = 0; i < _vec_handlers.size(); i++ )
  {
    const Handler* handler = _vec_handlers[i];
    Element* elem = _vec_elements[i];

    BRN_DEBUG("calling handler %s on element %s with param %s.", 
      _signal.c_str(), elem->declaration().c_str(), param.c_str() );

    if (NULL == errh)
    {
      errh = router()->chatter_channel(String("default"));
    }

    ContextErrorHandler cerrh( errh, String("In write handler '" +
      handler->unparse_name(elem) + "':").c_str());

    handler->call_write( param, elem, &cerrh );
  }
}

////////////////////////////////////////////////////////////////////////////////

enum {H_DEBUG, H_SIGNAL};

static String 
read_param(Element *e, void *thunk)
{
  Signal *td = (Signal *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_SIGNAL:
    return String("N/A\n");
  default:
    return String();
  }
}

static int
write_param(const String &in_s, Element *e, void *vparam,
          ErrorHandler *errh)
{
  Signal *f = (Signal *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    int debug;
    if (!cp_integer(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  case H_SIGNAL:
    {    //debug
      f->send_signal_action(s, errh);
      break;
    }
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void 
Signal::add_handlers()
{
  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);

//add_read_handler(_signal, read_param, (void *) H_SIGNAL);
  add_write_handler(_signal, write_param, (void *) H_SIGNAL);

  // NOTE: do not add handlers for _active
}

////////////////////////////////////////////////////////////////////////////////

CLICK_ENDDECLS
EXPORT_ELEMENT(Signal)

////////////////////////////////////////////////////////////////////////////////
