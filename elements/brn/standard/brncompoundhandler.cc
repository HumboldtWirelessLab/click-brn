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

#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "brncompoundhandler.hh"
#include <click/handlercall.hh>

#include "elements/brn/standard/compression/base64.hh"

CLICK_DECLS

BrnCompoundHandler::BrnCompoundHandler():
  _update_mode(UPDATEMODE_SEND_ALL),
  _record_mode(RECORDMODE_LAST_ONLY),
  _record_samples(10),
  _sample_time(1000),
  _record_timer(this),
  _compression_limit(0),
  _lzw_buffer(NULL),
  _lzw_buffer_size(32768),
  _base64_buffer(NULL),
  _base64_buffer_size(43692)
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

  if ( _lzw_buffer != NULL ) delete[] _lzw_buffer;
  if ( _base64_buffer != NULL ) delete[] _base64_buffer;
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
                    "COMPRESSIONLIMIT", cpkP, cpInteger, &_compression_limit,
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
  String element_name = router()->ename(this->eindex());
  int last_slash = element_name.find_right('/',element_name.length());
  if ( last_slash > 0 ) _element_class_prefix = element_name.substring(0,last_slash+1);
  
  BRN_DEBUG("PRefix: %s",_element_class_prefix.c_str());
  
  _lzw_buffer = new unsigned char[_lzw_buffer_size];
  _base64_buffer = new unsigned char[_base64_buffer_size];

  Vector<String> vec_handlers_tmp;

  BRN_DEBUG(" * BrnCompoundHandler: processing handler %s.", _handler.c_str() );
  cp_spacevec(_handler, vec_handlers_tmp);
  
  for( int i = 0; i < vec_handlers_tmp.size(); i++) {
    int handler_point = vec_handlers_tmp[i].find_right('.',vec_handlers_tmp[i].length());
    if ( handler_point > 0 ) {
      element_name = vec_handlers_tmp[i].substring(0,handler_point);
      String handler_name = vec_handlers_tmp[i].substring(handler_point+1,vec_handlers_tmp[i].length()-1);
      
      BRN_DEBUG("Check Handler: %s . %s", element_name.c_str(), handler_name.c_str());
    
      Element* s_element = router()->find(element_name);
      if ( s_element != NULL ) _vec_handlers.push_back(vec_handlers_tmp[i]);
      else {
        s_element = router()->find(_element_class_prefix + element_name);
        if ( s_element != NULL ) _vec_handlers.push_back(_element_class_prefix + vec_handlers_tmp[i]);
      } 
    }
  }
  
  vec_handlers_tmp.clear();

  // Build vector of class names
  Vector<String> vecClasses;
  cp_spacevec( _classes, vecClasses );

  // Init blacklist
  Vector<Element*> vecElements = router()->elements();

  for (int i = 0; i < vecElements.size(); i++) {
    Element* pElement = vecElements[i];
    BRN_DEBUG(" * BrnCompoundHandler: processing element %s.", pElement->declaration().c_str() );
    if ( String(pElement->class_name()) == String("BrnCompoundHandler") ) {
      _handler_blacklist.insert(String(router()->ename(pElement->eindex()) + ".read"),
                                String(router()->ename(pElement->eindex()) + ".read"));
    }
  }

  if ( vecClasses.size() != 0 ) {

    for (int j = 0; j < vecClasses.size(); j++ ) {
      // Collect handlers from elements with appropriate class names in a vector

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
    _record_handler.insert( _vec_handlers[j], new HandlerRecord(_record_samples, _sample_time));
  }

  _record_timer.initialize(this);
  _time_position = 0;

  if ( _record_mode == RECORDMODE_LAST_SAMPLE ) {
    _record_timer.schedule_after_msec(_sample_time);
  }

  return 0;
}

void
BrnCompoundHandler::run_timer(Timer *)
{
  Timestamp now = Timestamp::now();
  String new_value;

  if ( _record_mode == RECORDMODE_LAST_SAMPLE ) {
    _record_timer.schedule_after_msec(_sample_time);
  }

  for ( int j = 0; j < _vec_handlers.size(); j++) {
    HandlerRecord *hr = _record_handler.find(_vec_handlers[j]);

    if ( ((_time_position + hr->_base_time) % hr->_interval) == 0 ) {
      new_value = HandlerCall::call_read(_vec_handlers[j], router()->root_element(),
                                         ErrorHandler::default_handler());
      hr->insert(now,new_value);
    }
  }

  _time_position += _sample_time;
  /* Avoid overflow : TODO: impl. better solution */
  if ( _time_position > 2100000000 ) _time_position -= 2100000000;
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

String
BrnCompoundHandler::read_handler()
{
  StringAccum sa;

  sa << "<compoundhandler node=\"" << BRN_NODE_NAME << "\" time=\"" << Timestamp::now().unparse().c_str();
  sa << "\" recordmode=\"" << _record_mode << "\" updatemode=\"" << _update_mode << "\" >\n";

  if ( _record_mode == RECORDMODE_LAST_ONLY ) {
    for ( int j = 0; j < _vec_handlers.size(); j++) {
      if ( _update_mode == UPDATEMODE_SEND_ALL ) {
        sa << "\t<handler name=\"" << _vec_handlers[j] << "\">\n";
        sa << "\t\t<![CDATA[" << HandlerCall::call_read(_vec_handlers[j], router()->root_element(),
                                            ErrorHandler::default_handler()).c_str();
        sa << "]]>\n";
      } else if (( _update_mode == UPDATEMODE_SEND_INFO ) ||
                 ( _update_mode == UPDATEMODE_SEND_UPDATE_ONLY )) {
        String new_value = HandlerCall::call_read(_vec_handlers[j], router()->root_element(),
                                                                    ErrorHandler::default_handler());

        String *last_value = _last_handler_value.findp(_vec_handlers[j]);
        sa << "\t<handler name=\"" << _vec_handlers[j];
        if ( (last_value != NULL) && (*last_value == new_value)) {
          sa << "\" update=\"false\" >";
        } else {
          _last_handler_value.insert(_vec_handlers[j],new_value);
          sa << "\" update=\"true\" >\n<![CDATA[";
          sa << new_value.c_str() << "]]>\n";
        }
      }
      sa << "\t</handler>\n";
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

  if ( ( _compression_limit != 0 ) && ( sa.length() > _compression_limit ) ) {
    if ( sa.length() > _lzw_buffer_size ) {
      _lzw_buffer_size = 2 * sa.length();
      _base64_buffer_size = (( 4 * _lzw_buffer_size ) / 3) + 2;

      delete[] _lzw_buffer;
      delete[] _base64_buffer;

      _lzw_buffer = new unsigned char[_lzw_buffer_size];
      _base64_buffer = new unsigned char[_base64_buffer_size];
    }

    String result = sa.take_string();
    int lzw_len = lzw.encode((unsigned char*)result.data(), result.length(), _lzw_buffer, _lzw_buffer_size);

    int xml_len = sprintf((char*)_base64_buffer,
                         "<compressed_data type=\"lzw,base64\" uncompressed=\"%d\" compressed=\"%d\"><![CDATA[",
                         result.length(), lzw_len);

    int base64_len = Base64::encode(_lzw_buffer, lzw_len, &(_base64_buffer[xml_len]), _base64_buffer_size-xml_len);

    xml_len += sprintf((char*)&(_base64_buffer[xml_len + base64_len]),"]]></compressed_data>\n");

    return String((char*)_base64_buffer);
  }

  return sa.take_string();
}

void
BrnCompoundHandler::reset()
{
  for( int i = 0; i < _vec_handlers.size(); i++ )  _record_handler.erase(_vec_handlers[i]);

  _vec_handlers.clear();
}

String
BrnCompoundHandler::handler()
{
  StringAccum sa;

  sa << "<compoundhandlerinfo >\n";
  for ( int j = 0; j < _vec_handlers.size(); j++) {
    sa << "\t<handler name=\"" << _vec_handlers[j];

    HandlerRecord *hr = _record_handler.find(_vec_handlers[j]);
    if ( hr != NULL )  sa << "\" record_interval=\"" << hr->_interval << "\" unit=\"ms";
    sa << "\" />\n";
  }
  sa << "</compoundhandlerinfo>\n";

  return sa.take_string();
}

enum { H_HANDLER_INSERT, H_HANDLER_SET, H_HANDLER_REMOVE, H_HANDLER_RESET, H_HANDLER_UPDATEMODE, H_HANDLER_RECORDMODE, H_HANDLER_SAMPLECOUNT, H_HANDLER_SAMPLETIME, H_HANDLER_COMPRESSIONLIMIT };

int
BrnCompoundHandler::handler_operation(const String &in_s, void *vparam, ErrorHandler *errh)
{
  switch((intptr_t)vparam) {
    case H_HANDLER_INSERT:
      {
        uint32_t t;
        bool found_time;
        BRN_DEBUG("New handler %s",in_s.c_str());
        String s = cp_uncomment(in_s);
        Vector<String> args;
        cp_spacevec(s, args);

        for ( int args_i = 0 ; args_i < args.size(); args_i++ ) {
          HandlerCall hcall(args[args_i]);
          if ( hcall.initialize(HandlerCall::OP_READ, router()->root_element(), NULL) >= 0 ) {

            found_time = false;
            if ( (args_i+1) == args.size() ){
              t = _sample_time;
            } else {
              if ( cp_integer(args[args_i+1], &t) ) {
                found_time = true;
                if ( (int)t < _sample_time ) {
                  t = _sample_time;
                } else if ( (t % _sample_time) != 0 ) {
                  t = (t / _sample_time) * _sample_time;
                }
              } else {
                t = _sample_time;
              }
            }

            if ( _handler_blacklist.findp(args[args_i]) == NULL ) {
              BRN_DEBUG("Search handler %s to avoid dups",args[args_i].c_str());
              int i = 0;
              for( ; i < _vec_handlers.size(); i++ ) {
                if ( args[args_i] == _vec_handlers[i]) {
                  BRN_DEBUG("Found handler %s: index %d (%s).",args[args_i].c_str(),i,_vec_handlers[i].c_str());
                  if ( found_time ) {
                    HandlerRecord *hr = _record_handler.find(_vec_handlers[i]);
                    if ( hr != NULL ) {
                      hr->_interval = t;
                    }
                  }
                  break;
                }
              }
              if ( i == _vec_handlers.size() ) {
                BRN_DEBUG("Didn't found handler %s. Insert.",args[args_i].c_str());
                _vec_handlers.push_back(args[args_i]);
                _record_handler.insert( args[args_i], new HandlerRecord(_record_samples, t));
              }
            } else {
              BRN_DEBUG("Handler %s (index %d) is compoundhandler read.",args[args_i].c_str(),args_i);
            }

            if ( found_time ) {
              args_i++;
            }
          } else {
            BRN_DEBUG("Handler %s (index %d) is not valid.",args[args_i].c_str(),args_i);
          }
        }
        break;
      }
    case H_HANDLER_SET:
      {
        BRN_DEBUG("Reset");
        reset();
        BRN_DEBUG("Set");
        handler_operation(in_s, (void*)H_HANDLER_INSERT, errh);
        break;
      }
    case H_HANDLER_REMOVE:
      {
        for( int i = 0; i < _vec_handlers.size(); i++ ) {
          if ( in_s == _vec_handlers[i] ) {
            HandlerRecord *hr = _record_handler.find(in_s);
            if ( hr != NULL ) {
              delete hr;
              _record_handler.erase(in_s);
            }
            _vec_handlers.erase(_vec_handlers.begin() + i);
            break;
          }
        }
        break;
      }
    case H_HANDLER_RESET:
      {
        BRN_DEBUG("Reset");
        reset();
      }
    default: {}
  }
  return 0;
}

void
BrnCompoundHandler::set_recordmode(int mode)
{
  if ( mode == _record_mode ) return;

  _record_mode = mode;

  if ( _record_mode != RECORDMODE_LAST_SAMPLE ) {
    _record_timer.unschedule();
  } else {
    for (HandlerRecordMapIter iter = _record_handler.begin(); iter.live(); iter++) {
      HandlerRecord *hr = iter.value();
      hr->clear();
    }
    _record_timer.schedule_after_msec(_sample_time);
  }
}

void
BrnCompoundHandler::set_updatemode(int mode)
{
  if ( mode == _update_mode ) return;

  _update_mode = mode;

  if ( (_update_mode != UPDATEMODE_SEND_ALL) && (_record_mode != RECORDMODE_LAST_SAMPLE) ) {
    for( int i = 0; i < _vec_handlers.size(); i++ ) {
      _last_handler_value.insert(_vec_handlers[i],String());
    }
  }
}

void
BrnCompoundHandler::set_samplecount(int count)
{
  for (HandlerRecordMapIter iter = _record_handler.begin(); iter.live(); iter++) {
    HandlerRecord *hr = iter.value();
    hr->set_max_records(count);
  }
  _record_samples = count;
}

void
BrnCompoundHandler::set_sampletime(int time)
{
  if ( _sample_time == time ) return;

  for (HandlerRecordMapIter iter = _record_handler.begin(); iter.live(); iter++) {
    HandlerRecord *hr = iter.value();

    _sample_time = time;

    if (( hr->_interval % time ) != 0 ) {
      hr->_interval = _sample_time * (hr->_interval/_sample_time);
      if ( hr->_interval == 0 ) hr->_interval = _sample_time;
    }
  }

  if ( _record_mode == RECORDMODE_LAST_SAMPLE ) {
    _record_timer.unschedule();
    _record_timer.schedule_after_msec(_sample_time);
  }
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

/* Single handler for many handler with the same name */

static String
BrnCompoundHandler_read_compoundparam(Element */*e*/, void */*thunk*/)
{
  return String();
}

static int
BrnCompoundHandler_write_compoundparam(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  BrnCompoundHandler *ch = (BrnCompoundHandler *)e;

  String s = cp_uncomment(in_s);
  ch->set_value(s, errh);

  return( 0 );
}

/* handler to control the element */

static String
BrnCompoundHandler_read_param(Element *e, void *vparam)
{
  BrnCompoundHandler *ch = (BrnCompoundHandler *)e;
  StringAccum sa;

  switch((intptr_t)vparam) {
    case H_HANDLER_UPDATEMODE:
    {
      sa << ch->get_updatemode();
      break;
    }
    case H_HANDLER_RECORDMODE:
    {
      sa << ch->get_recordmode();
      break;
    }
    case H_HANDLER_SAMPLECOUNT:
    {
      sa << ch->get_samplecount();
      break;
    }
    case H_HANDLER_SAMPLETIME:
    {
      sa << ch->get_sampletime();
      break;
    }
    case H_HANDLER_COMPRESSIONLIMIT:
    {
      sa << ch->get_compressionlimit();
      break;
    }
  }

  sa << "\n";

  return sa.take_string();

}

static int
BrnCompoundHandler_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  BrnCompoundHandler *ch = (BrnCompoundHandler *)e;
  String s = cp_uncomment(in_s);
  int value;

  if (!cp_integer(s, &value))
    return errh->error("value must be integer");

  switch((intptr_t)vparam) {
    case H_HANDLER_UPDATEMODE:
    {
      ch->set_updatemode(value);
      break;
    }
    case H_HANDLER_RECORDMODE:
    {
      ch->set_recordmode(value);
      break;
    }
    case H_HANDLER_SAMPLECOUNT:
    {
      ch->set_samplecount(value);
      break;
    }
    case H_HANDLER_SAMPLETIME:
    {
      ch->set_sampletime(value);
      break;
    }
    case H_HANDLER_COMPRESSIONLIMIT:
    {
      ch->set_compressionlimit(value);
      break;
    }
  }

  return( 0 );

}

/* add handler function */

void
BrnCompoundHandler::add_handlers()
{
  //BRNElement::add_handlers(); //what if handler has the name "debug"

  if ( _classes_handler.length() > 0 ) {
    add_read_handler( _handler, BrnCompoundHandler_read_compoundparam, 0);
    add_write_handler( _handler, BrnCompoundHandler_write_compoundparam, 0);
  }

  add_read_handler( "read", BrnCompoundHandler_read_handler, 0);

  add_read_handler( "handler", BrnCompoundHandler_handler, 0);

  add_write_handler( "insert", BrnCompoundHandler_handler_operation, H_HANDLER_INSERT);
  add_write_handler( "set", BrnCompoundHandler_handler_operation, H_HANDLER_SET);
  add_write_handler( "remove", BrnCompoundHandler_handler_operation, H_HANDLER_REMOVE);
  add_write_handler( "reset", BrnCompoundHandler_handler_operation, H_HANDLER_RESET);

  add_read_handler( "updatemode", BrnCompoundHandler_read_param, H_HANDLER_UPDATEMODE);
  add_read_handler( "recordmode", BrnCompoundHandler_read_param, H_HANDLER_RECORDMODE);
  add_read_handler( "samples", BrnCompoundHandler_read_param, H_HANDLER_SAMPLECOUNT);
  add_read_handler( "sampletime", BrnCompoundHandler_read_param, H_HANDLER_SAMPLETIME);
  add_read_handler( "compressionlimit", BrnCompoundHandler_read_param, H_HANDLER_COMPRESSIONLIMIT);

  add_write_handler( "updatemode", BrnCompoundHandler_write_param, H_HANDLER_UPDATEMODE);
  add_write_handler( "recordmode", BrnCompoundHandler_write_param, H_HANDLER_RECORDMODE);
  add_write_handler( "samplecount", BrnCompoundHandler_write_param, H_HANDLER_SAMPLECOUNT);
  add_write_handler( "sampletime", BrnCompoundHandler_write_param, H_HANDLER_SAMPLETIME);
  add_write_handler( "compressionlimit", BrnCompoundHandler_write_param, H_HANDLER_COMPRESSIONLIMIT);

}

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnCompoundHandler)
