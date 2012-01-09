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
 * seismo_reporting.{cc,hh}
 */

#include <click/config.h>
#include <click/confparse.hh>

#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "seismo_reporting.hh"

CLICK_DECLS

SeismoReporting::SeismoReporting():
  _reporting_timer(this),
  _next_block_id(0),
  _index_in_block(0),
  _interval(SEISMO_REPORT_DEFAULT_INTERVAL),
  _long_avg_count(SEISMO_REPORT_LONG_INTERVAL),
  _short_avg_count(SEISMO_REPORT_SHORT_INTERVAL),
  _max_alarm_count(SEISMO_REPORT_MAX_ALARM_COUNT)
{
  BRNElement::init();
}

SeismoReporting::~SeismoReporting()
{
}

int
SeismoReporting::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "SEISMO", cpkP, cpElement, &_seismo,
      "INTERVAL", cpkP, cpInteger, &_interval,
      "LONGAVG", cpkP, cpInteger, &_long_avg_count,
      "SHORTAVG", cpkP, cpInteger, &_short_avg_count,
      "MAXALARM", cpkP, cpInteger, &_max_alarm_count,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  if ( _long_avg_count % _short_avg_count != 0 ) {
    return errh->error("LONGAVG must be a multiple of SHORTAVG");
  }

  return 0;
}

int
SeismoReporting::initialize(ErrorHandler *)
{

  _reporting_timer.initialize(this);
  _reporting_timer.schedule_after_msec(_interval);

  for ( int i = 0; i < 3; i++ ) {
    swl.push_back(SlidingWindow(_short_avg_count, _long_avg_count/_short_avg_count));
  }

  return 0;
}

void
SeismoReporting::run_timer(Timer *)
{
  BRN_DEBUG("Run timer");

  seismo_evaluation();

  _reporting_timer.schedule_after_msec(_interval);
}

void
SeismoReporting::seismo_evaluation()
{
  if ( _seismo->_local_info != NULL ) {
    BRN_DEBUG("Has local info");
    SeismoInfoBlock* sib;

    while ( (sib = _seismo->_local_info->get_block(_next_block_id)) != NULL ) {

      for ( ;_index_in_block < sib->_next_value_index; _index_in_block++ ) {
        if ( swl[0].add_data(sib->_channel_values[_index_in_block][0]) == 1 ) {
          /* check for alarm */
          int32_t last = swl[0].get_history_index(-1);
          int32_t current = swl[0].get_history_index(0);

          int32_t last_stdev = swl[0]._stdev[last];
          int32_t current_stdev = swl[0]._stdev[current];

          if ( (sal.size() == 0) || (sal[sal.size() - 1]._mode == ALARM_MODE_END) ) {
            if ( current_stdev > (3 * last_stdev) ) {
              sal.push_back(SeismoAlarm(last_stdev, current_stdev));
              if ( sal.size() > _max_alarm_count ) sal.erase(sal.begin());
            }
          } else {
            if ( sal.size() != 0 ) {
              if ( (current_stdev) < ( 3 * last_stdev)) {
                sal[sal.size() - 1].end_alarm(current_stdev);
              }
            }
          }
        }

        /*
        click_chatter("%d %d %d", sib->_channel_values[_index_in_block][0],
                                  sib->_channel_values[_index_in_block][1],
                                  sib->_channel_values[_index_in_block][2]);
        */
      }

      if ( ! sib->is_complete() ) break;
      _next_block_id++;
      _index_in_block = 0;
    }

    if ( sib == NULL ) {
      BRN_DEBUG("No data left in block");
    }
  } else {
    BRN_DEBUG("No local info");
  }

}
/************************************************************************************/
/********************************* H A N D L E R ************************************/
/************************************************************************************/

String
SeismoReporting::print_stats()
{
  StringAccum sa;

  sa << "<seismo_reporting>\n\t<history size='" << swl[0].size() << "' >\n";

  for ( int32_t i = 0; i < swl[0].size(); i++ ) {
    int32_t hi = swl[0].get_history_index(i);

    sa << "\t\t<history_entry id='" << i << "' avg='" << swl[0]._avg[hi] << "' min='";
    sa << swl[0]._min[hi] << "' max='" << swl[0]._max[hi] << "' stddev='";
    sa << swl[0]._stdev[hi] << "' raw_avg='" << swl[0]._raw_avg[hi] << "'/>\n";
  }

  sa << "\t</history>\n</seismo_reporting>\n";

  return sa.take_string();
}


String
SeismoReporting::print_alarm()
{
  StringAccum sa;

  sa << "<seismo_alarm size='" << sal.size() << "' >\n";

  for ( int32_t i = 0; i < sal.size(); i++ ) {
    sa << "\t<alarm start='" << sal[i]._start.unparse() << "' end='" << sal[i]._end.unparse() << "' std_before='";
    sa << sal[i]._stdev_before << "' stdev_mid='" << sal[i]._stdev_during << "' std_end='" << sal[i]._stdev_after << "' />\n";
  }

  sa << "</seismo_alarm>\n";

  return sa.take_string();
}

static String
read_stats_handler(Element *e, void */*thunk*/)
{
  SeismoReporting *sr = (SeismoReporting*)e;
  return sr->print_stats();
}

static String
read_alarm_handler(Element *e, void */*thunk*/)
{
  SeismoReporting *sr = (SeismoReporting*)e;
  return sr->print_alarm();
}

void
SeismoReporting::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", read_stats_handler);
  add_read_handler("alarm", read_alarm_handler);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SeismoReporting)
