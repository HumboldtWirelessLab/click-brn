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
 * testsink.{cc,hh} -- for debugging purpose only
 * A. Zubow
 */

#include <click/config.h>

#include "testsink.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
CLICK_DECLS

TestSink::TestSink()
  : _to_stop(false)
{
  _count = 0;
}

TestSink::~TestSink()
{
}

int
TestSink::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpInteger, "count", &_MAX_COUNT,
		  cpEnd) < 0)
    return -1;
  return 0;
}

int
TestSink::initialize(ErrorHandler *)
{
  return 0;
}

void
TestSink::push(int, Packet *p)
{
  _count++;
  if (_count == _MAX_COUNT)
    _to_stop = true;
  p->kill();
}

static int 
write_handler(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  TestSink *ts = (TestSink *)e;
  String s = cp_uncomment(in_s);
  bool tostop;
  if (!cp_bool(s, &tostop)) 
    return errh->error("tostop parameter must be boolean");
  ts->_to_stop = tostop;

  return 0;
}

static String
read_handler(Element *e, void *)
{
  TestSink *ts = (TestSink *)e;

  if (ts->_to_stop)
    return "false\n";
  else
    return "true\n";
}

void
TestSink::add_handlers()
{
  // needed for QuitWatcher
  add_read_handler("scheduled", read_handler, 0);
  add_write_handler("tostop", write_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TestSink)
