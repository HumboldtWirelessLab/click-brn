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
 * brncompoundhandler.{cc,hh} -- combines handlers on a class basis
 * M. Kurth
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/router.hh>

#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "brncompoundhandler.hh"
#include <click/handlercall.hh>
CLICK_DECLS

BrnCompoundHandler::BrnCompoundHandler() :
        _debug(2)
{
}

BrnCompoundHandler::~BrnCompoundHandler()
{
}

int
BrnCompoundHandler::configure(Vector<String> &conf, ErrorHandler* errh)
{
    if (cp_va_kparse(conf, this, errh,
                     "HANDLER", cpkP+cpkM, cpString, &_handler,
                     "CLASSES", cpkP, cpString, &_items,
                     "VALUE", cpkP, cpString, &_value,
                     cpEnd) < 0)
        return -1;

    if ( 0 >= _handler.length() )
        return errh->error("Handler not specified");

    /*  if( 0 >= _items.length() )
        return errh->error("Handler items not specified");
      */
    /*
     * The location of handlers is carried out in 'initialize', because
     * add_handlers is called after configure, but before initialize.
     */
    return 0;
}

int
BrnCompoundHandler::initialize(ErrorHandler *errh)
{



    // Build vector of class names
    /*  Vector<String> vecClasses;
      cp_spacevec( _items, vecClasses );
      if( 0 >= vecClasses.size() )
        return errh->error("Handler items not specified");
      */
    // Collect handlers from elements with appropriate class names in a vector
    /*  Vector<Element*> vecElements = router()->elements();
      for(int j = 0; j < vecClasses.size(); j++ )
      {
        bool bFound = false;
        BRN_DEBUG(" * BrnCompoundHandler: processing parameter %s.", vecClasses[j].c_str() );

        click_chatter("FOO"); //output wie printf

        for (int i = 0; i < vecElements.size(); i++)
        {
          Element* pElement = vecElements[i];
          BRN_DEBUG(" * BrnCompoundHandler: processing element %s.", pElement->declaration().c_str() );

          if( String(pElement->class_name()) == vecClasses[j]
            || router()->ename(pElement->eindex()) == vecClasses[j] )
          {
            BRN_DEBUG(" * BrnCompoundHandler: processing handler %s on element %s.\n",
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
      */
    return 0;
}

void
BrnCompoundHandler::set_value( const String& value, ErrorHandler *errh )
{
    _value = value;

    for ( int i = 0; i < _vec_handlers.size(); i++ )
    {
        const Handler* pHandler = _vec_handlers[i];
        Element* pElement = _vec_elements[i];

        BRN_DEBUG(" * BrnCompoundHandler: calling handler %s on element %s"
                  " with value %s.", _handler.c_str(), pElement->declaration().c_str(),
                  _value.c_str() );

        ContextErrorHandler cerrh( errh, String("In write handler '" +
                                                pHandler->unparse_name(pElement) + "':").c_str());

        pHandler->call_write( _value, pElement, &cerrh );
    }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

//CHRIS
String
BrnCompoundHandler::read_handlers()
{
    // readhandler ansprechen: /testbedhome/testbed/click/click-brn-x86/elements/brn2/test/brn2_packetqueuecontrol.cc
    StringAccum sa;
    Vector<Element*> vecElements = router()->elements();

    for (int i = 0; i < vecElements.size(); i++) {
        Element* pElement = vecElements[i];
        BRN_DEBUG(" * BrnCompoundHandler: processing element %s.", pElement->declaration().c_str() );
    }

    Vector<String> vecHandler;
    cp_spacevec(_handler, vecHandler);

    sa << "<compound_handler>\n";
    for ( int j=0; j < vecHandler.size(); j++) {
        sa << "\t<handler name=\"" << vecHandler[j] << "\">\n";
        sa << "\t" << HandlerCall::call_read(vecHandler[j], router()->root_element(), ErrorHandler::default_handler()).c_str() << "\n";
        sa << "\t</handler>\n";
    }
    sa << "</compound_handler>\n";

    return sa.take_string();
}


static String
read_handler(Element *e, void *thunk)
{
    return ((BrnCompoundHandler*)e)->read_handlers();
}

static String
read_param(Element *e, void *thunk)
{
}

static int
write_param(const String &in_s, Element *e, void *,
            ErrorHandler *errh)
{
    BrnCompoundHandler *dd = (BrnCompoundHandler *)e;

    String s = cp_uncomment(in_s);
    dd->set_value( s, errh );

    return( 0 );
}

void
BrnCompoundHandler::add_handlers()
{
    //add_read_handler( _handler, read_param, 0);
    //add_write_handler( _handler, write_param, 0);
    add_read_handler( "handler", read_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnCompoundHandler)
