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
 * compoundhandler.{cc,hh} -- combines handlers on a class basis
 * M. Kurth
 */

#include <click/config.h>
#include "compoundhandler.hh"

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/router.hh>
CLICK_DECLS

CompoundHandler::CompoundHandler() :
  _debug(BrnLogger::DEFAULT)
{
}

CompoundHandler::~CompoundHandler()
{
}

int
CompoundHandler::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_parse(conf, this, errh,
      cpString, "handler", &_handler,
      cpString, "classes", &_items,
      cpOptional,
      cpString, "value", &_value,
      cpEnd) < 0)
    return -1;

  if( 0 >= _handler.length() )
    return errh->error("Handler not specified");

  if( 0 >= _items.length() )
    return errh->error("Handler items not specified");

  /*
   * The location of handlers is carried out in 'initialize', because
   * add_handlers is called after configure, but before initialize.
   */
  return 0;
}

int
CompoundHandler::initialize(ErrorHandler *errh)
{
  // Build vector of class names
  Vector<String> vecClasses;
  cp_spacevec( _items, vecClasses );
  if( 0 >= vecClasses.size() ) 
    return errh->error("Handler items not specified");

  // Collect handlers from elements with appropriate class names in a vector
  Vector<Element*> vecElements = router()->elements();
  for(int j = 0; j < vecClasses.size(); j++ )
  {
    bool bFound = false;
    BRN_DEBUG(" * CompoundHandler: processing parameter %s.", vecClasses[j].c_str() );

    for (int i = 0; i < vecElements.size(); i++)
    {
      Element* pElement = vecElements[i];
      BRN_DEBUG(" * CompoundHandler: processing element %s.", pElement->declaration().c_str() );

      if( String(pElement->class_name()) == vecClasses[j]
        || router()->ename(pElement->eindex()) == vecClasses[j] )
      {
        BRN_DEBUG(" * CompoundHandler: processing handler %s on element %s.\n", 
            _handler.c_str(), router()->ename(pElement->eindex()).c_str() );//pElement->declaration().c_str() );

        const Handler* pHandler = Router::handler( pElement, _handler );
        if( NULL == pHandler 
          || false == pHandler->writable() )
        {
          return errh->error("Element class %s has no write handler %s.\n", 
            pElement->class_name(), _handler.c_str() );
        }

        _vec_elements.push_back( pElement );
        _vec_handlers.push_back( pHandler );
        bFound = true;
      }
    }

    if( false == bFound )
    {
      return errh->error("Handler item %s could no be resolved.\n", 
         vecClasses[j].c_str() );
    }
  }
  
  if( 0 >= _vec_handlers.size() )
    return errh->error("Handler classes not specified");

  // Set default value
  if( 0 < _value.length() )
  {
    set_value( _value, errh );
  }
  
  return 0;
}

void
CompoundHandler::set_value( const String& value, ErrorHandler *errh )
{
  _value = value;

  for( int i = 0; i < _vec_handlers.size(); i++ )
  {
    const Handler* pHandler = _vec_handlers[i];
    Element* pElement = _vec_elements[i];

    BRN_DEBUG(" * CompoundHandler: calling handler %s on element %s"
        " with value %s.", _handler.c_str(), pElement->declaration().c_str(),
        _value.c_str() );

    ContextErrorHandler cerrh( errh, "In write handler '" + 
      pHandler->unparse_name(pElement) + "':");

    pHandler->call_write( _value, pElement, true, &cerrh );
  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

//static String
//read_param(Element *e, void *thunk)
//{
//}

static int 
write_param(const String &in_s, Element *e, void *,
          ErrorHandler *errh)
{
  CompoundHandler *dd = (CompoundHandler *)e;
  
  String s = cp_uncomment(in_s);
  dd->set_value( s, errh );

  return( 0 );
}

void
CompoundHandler::add_handlers()
{
//  add_read_handler( _handler, read_param, 0);
  add_write_handler( _handler, write_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CompoundHandler)
