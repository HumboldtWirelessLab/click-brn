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

SystemInfo::SystemInfo() :
#ifdef CLICK_USERLEVEL
#ifndef CLICK_NS
   _cpu_timer(this),
   _cpu_timer_interval(DEFAULT_CPU_TIMER_INTERVAL),
   _cpu_stats_index(0),
#endif
#endif
   _me()
{
  BRNElement::init();
}

SystemInfo::~SystemInfo()
{
}

int
SystemInfo::configure(Vector<String> &conf, ErrorHandler* errh)
{
  uint32_t cpu_interval = 0;

  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "CPUTIMERINTERVAL", cpkP, cpInteger, &cpu_interval,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("BRN2NodeIdentity")) 
    return errh->error("BRN2NodeIdentity not specified");

#ifdef CLICK_USERLEVEL
#ifndef CLICK_NS
  _cpu_timer_interval = cpu_interval;
  click_pid = getpid();
#endif
#endif

  return 0;
}

int
SystemInfo::initialize(ErrorHandler */*errh*/)
{
#ifdef CLICK_USERLEVEL
#ifndef CLICK_NS
  _cpu_timer.initialize(this);
  if ( _cpu_timer_interval > 0 ) {
    _cpu_timer.reschedule_after_msec(_cpu_timer_interval);
  }
#endif
#endif
  return 0;
}

#ifdef CLICK_USERLEVEL
#ifndef CLICK_NS
void
SystemInfo::run_timer(Timer *)
{
  _cpu_stats_index = (_cpu_stats_index + 1) % 2;
  CPUStats::get_usage(click_pid, &_cpu_stats[_cpu_stats_index]);

  _cpu_timer.reschedule_after_msec(_cpu_timer_interval);
}
#endif
#endif

/*************************************************************************/
/************************ H A N D L E R **********************************/
/*************************************************************************/

enum {
  H_SYSINFO,
  H_SCHEMA
};

static String
read_handler(Element *e, void *thunk)
{
  SystemInfo *si = (SystemInfo *)e;
  Timestamp now = Timestamp::now();

  StringAccum sa;

  switch ((uintptr_t) thunk) {
     case H_SYSINFO: {

  sa << "<system id=\"" << si->_me->getMasterAddress()->unparse() << "\" name=\"" << si->_me->_nodename << "\" time=\"" << now.unparse() << "\">\n";

  // meminfo
#if CLICK_USERLEVEL
#ifndef CLICK_NS
  String raw_info = file_string("/proc/meminfo");
#else
  String raw_info = String("0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0"); 
#endif
#else
  String raw_info = String("0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0");
#endif
  Vector<String> first_col, second_col, third_col, fourth_col;

  parse_tabbed_lines(raw_info, &first_col, &second_col, &third_col, &fourth_col, NULL);

  //click_chatter(" * %s, %s, %s\n", second_col[1].c_str(), third_col[1].c_str(), fourth_col[1].c_str());

  sa << "\t<mem ";
  sa << "total=\"" << second_col[0] << "\" ";
  sa << "used=\"" << second_col[1] << "\" ";
  sa << "cached=\"" << second_col[3] << "\" ";
  sa << "buffers=\"" << second_col[2] << "\"";
  sa << "NFS_Unstable=\"" << second_col[17] << "\" ";
  sa << "/>\n";

  // recycle vectors
  first_col.clear();
  second_col.clear();
  third_col.clear();
  fourth_col.clear();

  // load average
#if CLICK_USERLEVEL
#ifndef CLICK_NS
  raw_info = String(file_string("/proc/loadavg"));
#endif
#endif

  parse_tabbed_lines(raw_info, &first_col, &second_col, &third_col, &fourth_col, NULL, NULL);

  //click_chatter(" * %s, %s, %s\n", first_col[0].c_str(), second_col[0].c_str(), third_col[0].c_str());

  sa << "\t<loadavg ";
  sa << "onemin=\"" << first_col[0] << "\" ";
  sa << "fivemin=\"" << second_col[0] << "\" ";
  sa << "fifteen=\"" << third_col[0] << "\" ";
  sa << "/>\n";

  // recycle vectors
  first_col.clear();
  second_col.clear();
  third_col.clear();
  fourth_col.clear();

  // uptime
#if CLICK_USERLEVEL
#ifndef CLICK_NS
  raw_info = String(file_string("/proc/uptime"));
#endif
#endif

  parse_tabbed_lines(raw_info, &first_col, &second_col, NULL);

  //click_chatter(" * %s, %s\n", first_col[0].c_str(), second_col[0].c_str());

  sa << "\t<uptime ";
  sa << "total=\"" << first_col[0] << "\" ";
  sa << "idle=\"" << second_col[0] << "\" ";
  sa << "/>\n";

  uint32_t ucpu = 0, scpu = 0, cpu = 0;

  // uptime
#if CLICK_USERLEVEL
#ifndef CLICK_NS
  CPUStats::calc_cpu_usage_int(&(si->_cpu_stats[si->_cpu_stats_index]), &(si->_cpu_stats[(si->_cpu_stats_index+1)%2]), &ucpu, &scpu, &cpu);
#endif
#endif

  //click_chatter(" * %s, %s\n", first_col[0].c_str(), second_col[0].c_str());

  sa << "\t<cpu_usage real=\"" << cpu << "\" user=\"" << ucpu << "\" sys=\"" << scpu << "\" unit=\"percent\" />\n";

  // linux version
#if CLICK_USERLEVEL
#ifndef CLICK_NS
  raw_info = String(file_string("/proc/version"));
#endif
#endif

  sa << "\t<linux version=\"" << raw_info.trim_space() << "\" />\n</system>\n";

  break;
  }
  case H_SCHEMA: {
    sa << "Tbd." << "\n";
  break;
  }
  default:
      return String() + "\n";
  }


  return sa.take_string();
}

void
SystemInfo::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("systeminfo", read_handler, (void *) H_SYSINFO);
  add_read_handler("schema", read_handler, (void *) H_SCHEMA);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SystemInfo)
