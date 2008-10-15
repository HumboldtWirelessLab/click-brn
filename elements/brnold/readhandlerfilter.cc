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
 * readhandlerfilter.{cc,hh} -- classifies elements according to a given read handler
 */

#include <click/config.h>
#include <clicknet/wifi.h>
#include "brn.h"
#include "readhandlerfilter.hh"

#include <click/handlercall.hh>
#include <click/notifier.hh>
#include "common.hh"
CLICK_DECLS

ReadHandlerFilter::ReadHandlerFilter() :
  _debug(BrnLogger::DEFAULT),
  _handler(NULL),
  _signals(NULL)
{
}

ReadHandlerFilter::~ReadHandlerFilter()
{
  delete _handler;
}

int 
ReadHandlerFilter::configure(Vector<String> &conf, ErrorHandler *errh)
{
  UNREFERENCED_PARAMETER(errh);
  
  if (cp_va_parse(conf, this, errh,
      cpReadHandlerCall, "read handler", &_handler,
      cpString, "matched pattern", &_pattern, 
      /* not required */
      cpKeywords,
      "DEBUG", cpInteger, "Debug", &_debug,
      cpEnd) < 0)
    return -1;
  
  return 0;
}

int
ReadHandlerFilter::initialize(ErrorHandler *errh)
{
  if (input_is_pull(0) != output_is_pull(0))
    errh->error("ReadHandlerFilter: Input 0 must have the processing of output 0.");
  
  if (noutputs() > 1 && !output_is_push(1))
    errh->error("ReadHandlerFilter: Output 0 must be push.");

  if( _handler->initialize_read(this, errh) < 0 )
    return errh->error("Could not initialize read handler");

  if (!(_signals = new NotifierSignal[ninputs()]))
    return errh->error("out of memory!");
  for (int i = 0; i < ninputs(); i++)
    _signals[i] = Notifier::upstream_empty_signal(this, i, 0);

  return 0;
}

void
ReadHandlerFilter::cleanup(CleanupStage)
{
  delete[] _signals;
}

Packet *
ReadHandlerFilter::pull(int port)
{
  UNREFERENCED_PARAMETER(port);
  
  Packet *p = NULL;
  
  while (NULL == p)
  {
    bool associated = (_handler->call_read() == _pattern);
    BRN_DEBUG("read handler returned %s, associated %s", 
      _handler->call_read().c_str(), associated ? "true" : "false");

    if (associated && ninputs() > 1)
      p = (_signals[1] ? input(1).pull() : NULL);
    if (NULL != p) {
      BRN_INFO("associated, sending previously buffered packet");

      return p;
    }
    
    p = (_signals[0] ? input(0).pull() : NULL);
    if (NULL == p)
      return (NULL);
    
    click_wifi* wifi = (click_wifi*) p->data();
    BRN_CHECK_EXPR_RETURN(wifi == NULL,
      ("could not determine wifi header"), return p;);

    // Process only class 3 frames, i.e. data fromds/tods
    // TODO refactor this condition into another element    
    if (WIFI_FC0_TYPE_DATA != (wifi->i_fc[0] & WIFI_FC0_TYPE_MASK)
      || WIFI_FC0_SUBTYPE_DATA != (wifi->i_fc[0] & WIFI_FC0_SUBTYPE_MASK)
      || WIFI_FC1_DIR_NODS == (wifi->i_fc[1] & WIFI_FC1_DIR_MASK))
      return p;
    
    if (!associated) {
      BRN_INFO("not associated, buffering packet");

      checked_output_push(1, p);
      p = NULL;
    }
  }
    
  return p;
}

enum {H_DEBUG};

static String 
read_param(Element *e, void *thunk)
{
  ReadHandlerFilter *td = (ReadHandlerFilter *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  default:
    return String();
  }
}

static int 
write_param(const String &in_s, Element *e, void *vparam,
          ErrorHandler *errh)
{
  ReadHandlerFilter *f = (ReadHandlerFilter *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    int debug;
    if (!cp_integer(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  }
  return 0;
}

void 
ReadHandlerFilter::add_handlers()
{
  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ReadHandlerFilter)
ELEMENT_MT_SAFE(ReadHandlerFilter)
