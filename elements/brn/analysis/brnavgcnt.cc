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
 * brnavgcnt.{cc,hh}
 */

#include <click/config.h>
#include "brnavgcnt.hh"
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/sync.hh>
#include <click/glue.hh>
#include <click/error.hh>
#include <click/straccum.hh>
CLICK_DECLS

BrnAvgCnt::BrnAvgCnt()
 : _me()
{
}

BrnAvgCnt::~BrnAvgCnt()
{
}

void
BrnAvgCnt::reset()
{
  // clear table
  _stat_map.clear();
}

int
BrnAvgCnt::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
    cpOptional,
    cpElement, "NodeIdentity", &_me,
    cpEnd) < 0)
    return -1;

  if (!_me || _me->cast("NodeIdentity") == 0) 
    return errh->error("NodeIdentity element is not provided or not a NodeIdentity");

  return 0;
}

int
BrnAvgCnt::initialize(ErrorHandler *)
{
  _last = 0;
  reset();
  return 0;
}

// handle incoming packets
Packet *
BrnAvgCnt::simple_action(Packet *p)
{
  click_ether *ether = (click_ether *) p->data();
  EtherAddress dst(ether->ether_dhost);
  EtherAddress src(ether->ether_shost);

  uint32_t *item = _stat_map.findp(dst);

  if (!item) { // no entry found for destination
    _stat_map.insert(dst, 0);
    item = _stat_map.findp(dst);
  }

  uint32_t new_value = (*item) + 1;
  //click_chatter(" * updating %s with %d\n", dst.unparse().c_str(), new_value);

  _stat_map.insert(dst, new_value);

  return p;
}

uint32_t
BrnAvgCnt::get_traffic_count(EtherAddress &dst, int &delta)
{
  delta = click_jiffies() - last();

  update_last();

  if (_stat_map.findp(dst))
     return *_stat_map.findp(dst);

  return 0;
}

static String
brnavgcnt_read_count_handler(Element *e, void *)
{
  BrnAvgCnt *c = (BrnAvgCnt *)e;

  //int delta = click_jiffies() - c->last();

  c->update_last();

  StringAccum sa;

  sa << "<traffic>\n";
  for (BrnAvgCnt::StatMap::iterator i = c->_stat_map.begin(); i.live(); i++) {
    EtherAddress dst = i.key();
    uint32_t &count = i.value();
    sa << "<link from='" << c->_me->getMyWirelessAddress()->unparse() << "' to='" << dst.unparse() << "' count='" << count << "' />\n";
  }
  sa << "</traffic>\n";

  // clear table
  c->reset();

  return sa.take_string();
}

static int
brnavgcnt_reset_write_handler
(const String &, Element *e, void *, ErrorHandler *)
{
  BrnAvgCnt *c = (BrnAvgCnt *)e;
  c->reset();
  return 0;
}

void
BrnAvgCnt::add_handlers()
{
  add_read_handler("count", brnavgcnt_read_count_handler, 0);
  add_write_handler("reset", brnavgcnt_reset_write_handler, 0);
}

EXPORT_ELEMENT(BrnAvgCnt)
ELEMENT_MT_SAFE(BrnAvgCnt)

#include <click/bighashmap.cc>
template class HashMap<EtherAddress, uint32_t>;

CLICK_ENDDECLS
