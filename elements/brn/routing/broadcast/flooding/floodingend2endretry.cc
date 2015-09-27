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
 * floodingend2endretry.{cc,hh}
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/etheraddress.hh>


#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "floodingend2endretry.hh"

CLICK_DECLS

FloodingEnd2EndRetry::FloodingEnd2EndRetry():
  _dfl_retries(0),
  _dfl_timeout(25),
  _time_tolerance(10),
  _retransmit_timer(this),
  _queued_pkts(0)
  //,_dequeued_pkts(0),
  //_retransmissions(0)
{
  BRNElement::init();
}

FloodingEnd2EndRetry::~FloodingEnd2EndRetry()
{
  for ( int i = 0; i < p_queue.size(); i++ ) {
    RetryPacket *p_next = p_queue[i];
    p_next->_p->kill();
    delete p_next;
  }
  
  p_queue.clear();
}

int
FloodingEnd2EndRetry::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEFAULTRETRIES", cpkP, cpInteger, &_dfl_retries,
      "DEFAULTTIMEOUT", cpkP, cpInteger, &_dfl_timeout,
      "TIMETOLERANCE", cpkP, cpInteger, &_time_tolerance,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
FloodingEnd2EndRetry::initialize(ErrorHandler *)
{
  _retransmit_timer.initialize(this);
  return 0;
}

void
FloodingEnd2EndRetry::run_timer(Timer *)
{
  BRN_DEBUG("Timer");

  Timestamp now = Timestamp::now();

  int next_timeout_qi = -1;
  int next_time_left = _dfl_timeout << 1;

  for( int i =  p_queue.size()-1; i >= 0; i--) {
    int time_left = p_queue[i]->time_left(now);
    if ( time_left < (int32_t)_time_tolerance ) {

      p_queue[i]->set_next_retry();

      if (p_queue[i]->retries_left()) {

        output(0).push(p_queue[i]->_p->clone()->uniqueify());

        if ( (time_left = p_queue[i]->time_left(now)) < next_time_left ) {
          next_time_left = time_left;
          next_timeout_qi = i;
        }

      } else {

        output(0).push(p_queue[i]->_p);

        delete p_queue[i];

        p_queue.erase(p_queue.begin() + i);
        next_timeout_qi--;
      }
    }
  }

  if ( next_timeout_qi >= 0) {
    _retransmit_timer.schedule_after_msec(p_queue[next_timeout_qi]->time_left(now));
  }

}

void
FloodingEnd2EndRetry::push(int, Packet *packet )
{
  if ( _dfl_retries > 0 ) {
    BRN_DEBUG("Enqueu: %d %d",_dfl_retries, _dfl_timeout);
    RetryPacket *rp = new RetryPacket(packet->clone()->uniqueify(), _dfl_retries, _dfl_timeout);
    p_queue.push_back(rp);

    _queued_pkts++;

    if ( p_queue.size() == 1 ) {
      BRN_DEBUG("Time after %d",p_queue[0]->time_left(Timestamp::now()));
      _retransmit_timer.schedule_after_msec(p_queue[0]->time_left(Timestamp::now()));
    }
  }

  output(0).push(packet);
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

String
FloodingEnd2EndRetry::stats()
{
  StringAccum sa;
  //Timestamp now = Timestamp::now();

  sa << "<floodingend2endretry node=\"" << BRN_NODE_NAME << "\" >\n";  
  sa << "</floodingpassiveack>\n";

  return sa.take_string();
}

static String
read_stats_param(Element *e, void *)
{
  return ((FloodingEnd2EndRetry *)e)->stats();
}

void
FloodingEnd2EndRetry::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", read_stats_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FloodingEnd2EndRetry)
