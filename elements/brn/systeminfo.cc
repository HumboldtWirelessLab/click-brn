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
 * systeminfo.{cc,hh} -- shows information about the system
 * A. Zubow
 */

#include <click/config.h>

#include "systeminfo.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
CLICK_DECLS

SystemInfo::SystemInfo()
  : _debug(BrnLogger::DEFAULT),
    _me()
{
}

SystemInfo::~SystemInfo()
{
  delete(_handler);
}

int
SystemInfo::configure(Vector<String> &conf, ErrorHandler* errh)
{
  String hndl_name;
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
                  cpElement, "NodeIdentity", &_me,
                  cpString, "sdp meta handler", &hndl_name,
		  cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("NodeIdentity")) 
    return errh->error("NodeIdentity not specified");

  if(_debug)
    click_chatter(" * using handler %s", hndl_name.c_str());
  _handler = new HandlerCall(hndl_name);

  return 0;
}

int
SystemInfo::initialize(ErrorHandler *errh)
{
  if (_handler && _handler->initialize_read(this, errh) < 0)
    return -1;
  return 0;
}

static String
read_handler(Element *e, void *)
{
  SystemInfo *si = (SystemInfo *)e;

  StringAccum sa;

  sa << "<system id='" << si->_me->getMyWirelessAddress()->unparse() << "'>\n";

  //sdp info
  if (si->_handler) {
    String sdp_meta = si->_handler->call_read();
    sa << "\t" << sdp_meta;
  }

  // meminfo
  String raw_info = file_string("/proc/meminfo");
  Vector<String> first_col, second_col, third_col, fourth_col;

  parse_tabbed_lines(raw_info, &first_col, &second_col, &third_col, &fourth_col, NULL);

  //click_chatter(" * %s, %s, %s\n", second_col[1].c_str(), third_col[1].c_str(), fourth_col[1].c_str());

  sa << "\t<mem ";
  sa << "total='" << second_col[1] << "' ";
  sa << "used='" << third_col[1] << "' ";
  sa << "free='" << fourth_col[1] << "' ";
  sa << "/>\n";

  // recycle vectors
  first_col.clear();
  second_col.clear();
  third_col.clear();
  fourth_col.clear();

  // load average
  raw_info = String(file_string("/proc/loadavg"));

  parse_tabbed_lines(raw_info, &first_col, &second_col, &third_col, &fourth_col, NULL, NULL);

  //click_chatter(" * %s, %s, %s\n", first_col[0].c_str(), second_col[0].c_str(), third_col[0].c_str());

  sa << "\t<loadavg ";
  sa << "onemin='" << first_col[0] << "' ";
  sa << "fivemin='" << second_col[0] << "' ";
  sa << "fifteen='" << third_col[0] << "' ";
  sa << "/>\n";

  // recycle vectors
  first_col.clear();
  second_col.clear();
  third_col.clear();
  fourth_col.clear();

  // load average
  raw_info = String(file_string("/proc/uptime"));
  parse_tabbed_lines(raw_info, &first_col, &second_col, NULL);

  //click_chatter(" * %s, %s\n", first_col[0].c_str(), second_col[0].c_str());

  sa << "\t<uptime ";
  sa << "total='" << first_col[0] << "' ";
  sa << "idle='" << second_col[0] << "' ";
  sa << "/>\n";



  sa << "</system>\n";

  return sa.take_string();
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  SystemInfo *ds = (SystemInfo *)e;
  return String(ds->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  SystemInfo *ds = (SystemInfo *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  ds->_debug = debug;
  return 0;
}

void
SystemInfo::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);

  add_read_handler("systeminfo", read_handler, 0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(SystemInfo)
