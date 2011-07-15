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
 * R. Sombrutzki & M. Kurth
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

BrnCompoundHandler::BrnCompoundHandler():
  _update_mode(UPDATEMODE_SEND_ALL),
  _record_mode(RECORDMODE_LAST_ONLY),
  _record_samples(10),
  _sample_time(1000),
  _record_timer(this)
{
  BRNElement::init();
}

BrnCompoundHandler::~BrnCompoundHandler()
{
  for (HandlerRecordMapIter iter = _record_handler.begin(); iter.live(); iter++) {
    HandlerRecord *hr = iter.value();
    delete hr;
  }
  _record_handler.clear();
  _last_handler_value.clear();
  _vec_elements.clear();
  _vec_elements_handlers.clear();
  _vec_handlers.clear();
}

int
BrnCompoundHandler::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
                    "HANDLER", cpkP, cpString, &_handler,
                    "CLASSES", cpkP, cpString, &_classes,
                    "CLASSESHANDLER", cpkP, cpString, &_classes_handler,
                    "CLASSESVALUE", cpkP, cpString, &_classes_value,
                    "UPDATEMODE", cpkP, cpInteger, &_update_mode,
                    "RECORDMODE", cpkP, cpInteger, &_record_mode,
                    "RECORDSAMPLES", cpkP, cpInteger, &_record_samples,
                    "SAMPLETIME", cpkP, cpInteger, &_sample_time,
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
BrnCompoundHandler::initialize(ErrorHandler *errh)
{
  BRN_DEBUG(" * BrnCompoundHandler: processing handler %s.", _handler.c_str() );
  cp_spacevec(_handler, _vec_handlers);

  // Build vector of class names
  Vector<String> vecClasses;
  cp_spacevec( _classes, vecClasses );

  if ( vecClasses.size() != 0 ) {

    // Collect handlers from elements with appropriate class names in a vector
    Vector<Element*> vecElements = router()->elements();
    for (int j = 0; j < vecClasses.size(); j++ ) {

      bool bFound = false;
      BRN_DEBUG(" * BrnCompoundHandler: processing parameter %s.", vecClasses[j].c_str() );

      for (int i = 0; i < vecElements.size(); i++) {
        Element* pElement = vecElements[i];
        BRN_DEBUG(" * BrnCompoundHandler: processing element %s.", pElement->declaration().c_str() );

        if ( String(pElement->class_name()) == vecClasses[j]
            || router()->ename(pElement->eindex()) == vecClasses[j] )
        {
          BRN_DEBUG(" * BrnCompoundHandler: processing handler %s on element %s. %s\n",
                    _classes_handler.c_str(), router()->ename(pElement->eindex()).c_str(),
                    pElement->declaration().c_str() );

          const Handler* pHandler = Router::handler( pElement, _classes_handler );
          if ( NULL == pHandler || false == pHandler->writable() ) {
            return errh->error("Element class %s has no write handler %s.\n",
                                pElement->class_name(), _classes_handler.c_str() );
          }

          _vec_elements.push_back( pElement );
          _vec_elements_handlers.push_back( pHandler );
          _vec_handlers.push_back( router()->ename(pElement->eindex()) + "." + _classes_handler);

          bFound = true;
        }
      }

      if ( false == bFound ) {
        return errh->error("Handler item %s could no be resolved.\n", vecClasses[j].c_str() );
      }
    }

    // Set default value
    if((0 < _classes_value.length()) && (_vec_elements.size() > 0 )) {
      set_value( _classes_value, errh );
    }
  }

  for (int j = 0; j < _vec_handlers.size(); j++ ) {
    _record_handler.insert( _vec_handlers[j], new HandlerRecord(_record_samples, 0));
  }

  _record_timer.initialize(this);
  if ( _record_mode == RECORDMODE_LAST_SAMP ) {
    _record_timer.schedule_after_msec(_sample_time);
  }

  return 0;
}

void
BrnCompoundHandler::run_timer(Timer *)
{
  Timestamp now = Timestamp::now();
  String new_value;

  for ( int j = 0; j < _vec_handlers.size(); j++) {
    new_value = HandlerCall::call_read(_vec_handlers[j], router()->root_element(),
                                       ErrorHandler::default_handler());

    HandlerRecord *hr = _record_handler.find(_vec_handlers[j]);

    hr->insert(now,new_value);

  }

  if ( _record_mode == RECORDMODE_LAST_SAMP ) {
    _record_timer.schedule_after_msec(_sample_time);
  }
}

void
BrnCompoundHandler::set_value( const String& value, ErrorHandler *errh )
{
  _classes_value = value;

  for ( int i = 0; i < _vec_elements_handlers.size(); i++ )
  {
    const Handler* pHandler = _vec_elements_handlers[i];
    Element* pElement = _vec_elements[i];

    BRN_DEBUG(" * BrnCompoundHandler: calling handler %s on element %s with value %s.",
              _classes_handler.c_str(), pElement->declaration().c_str(), _classes_value.c_str() );

    ContextErrorHandler cerrh( errh, String("In write handler '" +
                                            pHandler->unparse_name(pElement) + "':").c_str());

    pHandler->call_write( _classes_value, pElement, &cerrh );
  }
}

/* TODO: leerer record wegnehemn */


String
BrnCompoundHandler::read_handler()
{
  StringAccum sa;

  sa << "<compoundhandler time=\"" << Timestamp::now().unparse().c_str() << "\" recordmode=\"" << _record_mode << "\" updatemode=\"" << _update_mode << "\" >\n";

  if ( _record_mode == RECORDMODE_LAST_ONLY ) {
    for ( int j = 0; j < _vec_handlers.size(); j++) {
      if ( _update_mode == UPDATEMODE_SEND_ALL ) {
        sa << "\t<handler name=\"" << _vec_handlers[j] << "\">\n";
        sa << "\t" << HandlerCall::call_read(_vec_handlers[j], router()->root_element(),
                                            ErrorHandler::default_handler()).c_str();
        sa << "\t";
      } else if (( _update_mode == UPDATEMODE_SEND_INFO ) ||
                 ( _update_mode == UPDATEMODE_SEND_UPDATE_ONLY )) {
        String new_value = HandlerCall::call_read(_vec_handlers[j], router()->root_element(),
                                                                    ErrorHandler::default_handler());

        String *last_value = _last_handler_value.findp(_vec_handlers[j]);
        sa << "\t<handler name=\"" << _vec_handlers[j];
        if ( (last_value != NULL) && (*last_value == new_value)) {
          sa << " \" update=\"false\" >";
        } else {
          _last_handler_value.insert(_vec_handlers[j],new_value);
          sa << " \" update=\"true\" >\n";
          sa << new_value.c_str() << "\t";
        }
      }
      sa << "</handler>\n";
    }
  } else { // show samples
    for (HandlerRecordMapIter iter = _record_handler.begin(); iter.live(); iter++) {
      String handler = iter.key();
      HandlerRecord *hr = iter.value();
      sa << "\t<handler name=\"" << handler.c_str();
      if ( hr == NULL ) {
        BRN_ERROR("No handler rec");
        sa << "\" error=\"true\" >\n\t</handler>\n";
      } else {
        sa << "\" count_records=\"" << hr->count_records();
        sa << "\" count_read=\"" << hr->_rec_index << "\" overflow=\"" << hr->overflow() << "\" >\n";
        int rec_count = hr->count_records();

        String *last_value = hr->get_record_i(0);
        for ( int i = 0; i < rec_count; i++) {
          if ( (_update_mode == UPDATEMODE_SEND_INFO) || (_update_mode == UPDATEMODE_SEND_UPDATE_ONLY) ) {
            String *current_value = hr->get_record_i(i);
            if ( (i == 0) || (*last_value != *current_value) ) {
              sa << "\t\t<record time=\"" << hr->get_timestamp_i(i)->unparse() << "\" update=\"";
              sa << "true\" >\n\t\t\t<![CDATA[" << current_value->c_str();
              last_value = current_value;
              sa << "]]>\n\t\t</record>\n";
            } else {
              if (_update_mode == UPDATEMODE_SEND_INFO) {
                sa << "\t\t<record time=\"" << hr->get_timestamp_i(i)->unparse();
                sa << "\" update=\"false\" ><![CDATA[]]></record>\n";
              }
            }
          } else {
            sa << "\t\t<record time=\"" << hr->get_timestamp_i(i)->unparse() << "\" update=\"true\" >\n\t\t\t<![CDATA[";
            sa << hr->get_record_i(i)->c_str() << "]]>\n\t\t</record>\n";
          }
        }
        sa << "\t</handler>\n";
        hr->clear();
      }
    }
  }
  sa << "</compoundhandler>\n";

  return sa.take_string();
}

String
BrnCompoundHandler::handler()
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
BrnCompoundHandler::handler_operation(const String &in_s, void *vparam, ErrorHandler */*errh*/)
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
          _record_handler.insert( in_s, new HandlerRecord(_record_samples, 0));
        }
        break;
      }
    case H_HANDLER_REMOVE:
      {
        for( int i = 0; i < _vec_handlers.size(); i++ ) {
          if ( in_s == _vec_handlers[i] ) {
            _record_handler.erase(in_s);
            _vec_handlers.erase(_vec_handlers.begin() + i);
            break;
          }
        }
        break;
      }
    default: {}
  }
  return 0;
}


//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
BrnCompoundHandler_read_handler(Element *e, void */*thunk*/)
{
  return ((BrnCompoundHandler*)e)->read_handler();
}

static String
BrnCompoundHandler_handler(Element *e, void */*thunk*/)
{
  return ((BrnCompoundHandler*)e)->handler();
}

static int
BrnCompoundHandler_handler_operation(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  return ((BrnCompoundHandler*)e)->handler_operation(in_s, vparam, errh);
}

static String
BrnCompoundHandler_read_param(Element */*e*/, void */*thunk*/)
{
  return String();
}

static int
BrnCompoundHandler_write_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  BrnCompoundHandler *ch = (BrnCompoundHandler *)e;

  String s = cp_uncomment(in_s);
  ch->set_value(s, errh);

  return( 0 );
}

void
BrnCompoundHandler::add_handlers()
{
  //BRNElement::add_handlers(); //what if handler has the name "debug"

  if ( _classes_handler.length() > 0 ) {
    add_read_handler( _handler, BrnCompoundHandler_read_param, 0);
    add_write_handler( _handler, BrnCompoundHandler_write_param, 0);
  }

  add_read_handler( "read", BrnCompoundHandler_read_handler, 0);

  add_read_handler( "handler", BrnCompoundHandler_handler, 0);
  add_write_handler( "insert", BrnCompoundHandler_handler_operation, H_HANDLER_INSERT);
  add_write_handler( "remove", BrnCompoundHandler_handler_operation, H_HANDLER_REMOVE);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnCompoundHandler)
