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
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "systeminfo.hh"

CLICK_DECLS

SystemInfo::SystemInfo()
  : _me()
{
  BRNElement::init();
}

SystemInfo::~SystemInfo()
{
}

int
SystemInfo::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("BRN2NodeIdentity")) 
    return errh->error("BRN2NodeIdentity not specified");

  return 0;
}

int
SystemInfo::initialize(ErrorHandler */*errh*/)
{
  return 0;
}

static String
read_handler(Element *e, void *)
{
  SystemInfo *si = (SystemInfo *)e;

  StringAccum sa;

  sa << "<system id='" << si->_me->getMasterAddress()->unparse() << "' name='" << si->_me->_nodename << "'>\n";

  // meminfo
#if CLICK_USERLEVEL
  String raw_info = file_string("/proc/meminfo");
#else
  String raw_info = String("0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0");
#endif
  Vector<String> first_col, second_col, third_col, fourth_col;

  parse_tabbed_lines(raw_info, &first_col, &second_col, &third_col, &fourth_col, NULL);

  //click_chatter(" * %s, %s, %s\n", second_col[1].c_str(), third_col[1].c_str(), fourth_col[1].c_str());

  sa << "\t<mem ";
  sa << "total='" << second_col[0] << "' ";
  sa << "used='" << second_col[1] << "' ";
  sa << "cached='" << second_col[3] << "' ";
  sa << "buffers='" << second_col[2] << "' ";
  sa << "NFS_Unstable='" << second_col[17] << "' ";
  sa << "/>\n";

  // recycle vectors
  first_col.clear();
  second_col.clear();
  third_col.clear();
  fourth_col.clear();

  // load average
#if CLICK_USERLEVEL
  raw_info = String(file_string("/proc/loadavg"));
#endif

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

  // uptime
#if CLICK_USERLEVEL
  raw_info = String(file_string("/proc/uptime"));
#endif

  parse_tabbed_lines(raw_info, &first_col, &second_col, NULL);

  //click_chatter(" * %s, %s\n", first_col[0].c_str(), second_col[0].c_str());

  sa << "\t<uptime ";
  sa << "total='" << first_col[0] << "' ";
  sa << "idle='" << second_col[0] << "' ";
  sa << "/>\n";

  // linux version
#if CLICK_USERLEVEL
  raw_info = String(file_string("/proc/version"));
#endif

  sa << "\t<linux ";
  sa << "version='" << raw_info.trim_space() << "'";
  sa << "/>\n";

  sa << "</system>\n";

  return sa.take_string();
}

void
SystemInfo::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("systeminfo", read_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SystemInfo)
