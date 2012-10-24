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
 * timesyncinfo.{cc,hh}
 * R. Sombrutzki
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "timesyncinfo.hh"

CLICK_DECLS

TimeSyncInfo::TimeSyncInfo() :
  _max_ids(TIMESYNCINFO_DEFAULT_MAX_IDS),
  _next_id(0)
{
  BRNElement::init();
}

TimeSyncInfo::~TimeSyncInfo()
{
}

int
TimeSyncInfo::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "MAXIDS", cpkP, cpInteger, &_max_ids,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
TimeSyncInfo::initialize(ErrorHandler */*errh*/)
{
  _timestamps = new Timestamp[_max_ids];
  _packet_ids = new int32_t[_max_ids];

  for(uint32_t i = 0; i < _max_ids; i++ ) _packet_ids[i] = -1;

  return 0;
}

void
TimeSyncInfo::push(int /*port*/, Packet *p)
{
  int32_t id = (int32_t)*((int32_t*)p->data());
  _packet_ids[_next_id] = ntohl(id);
  _timestamps[_next_id] = p->timestamp_anno();

  _next_id = (_next_id + 1) % _max_ids;

  if ( noutputs() > 1 )
    output(1).push(p);
  else
    p->kill();
}
/*************************************************************************/
/************************ H A N D L E R **********************************/
/*************************************************************************/

enum {
  H_TIMESYNCINFO,
  H_MAXIDS
};

String
TimeSyncInfo::read_syncinfo()
{
  StringAccum sa;

  sa << "<timesyncinfo id='" << BRN_NODE_ADDRESS << "' name='" << BRN_NODE_NAME << "' time='" << Timestamp::now().unparse() << "'>\n";

  int start = 0;
  int end = _next_id - 1;

  if ( _packet_ids[_next_id] != -1) {
    start = _next_id;
    end = _next_id + _max_ids - 1;
  }

  for ( int i = start; i <= end; i++ ) {
    sa << "\t<syncpacket time='" << _timestamps[i%_max_ids].usecval() << "' unit='us' id='" << _packet_ids[i%_max_ids] << "' />\n";
  }

  sa << "</timesyncinfo>\n";

  return sa.take_string();
}

static String
read_handler(Element *e, void *thunk)
{
  TimeSyncInfo *tsi = (TimeSyncInfo *)e;

  switch ((uintptr_t) thunk) {
    case H_TIMESYNCINFO: {
      return tsi->read_syncinfo();
    }
  }

  return String() + "\n";
}

void
TimeSyncInfo::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("syncinfo", read_handler, (void *) H_TIMESYNCINFO);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TimeSyncInfo)
