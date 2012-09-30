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
 * seismodetection_longshortavg.{cc,hh}
 */

#include <click/config.h>
#include <click/confparse.hh>

#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "seismo_reporting.hh"
#include "seismodetection_longshortavg.hh"

CLICK_DECLS

SeismoDetectionLongShortAvg::SeismoDetectionLongShortAvg():
  _long_avg_count(0),
  _short_avg_count(0),
  _dthreshold(DEFAULT_DIFF_THRESHOLD),
  _rthreshold(DEFAULT_RATIO_THRESHOLD),
  _normalize(DEFAULT_NORMALIZE),
  _max_alarm_count(0)
{
  BRNElement::init();
}

SeismoDetectionLongShortAvg::~SeismoDetectionLongShortAvg()
{
}

void *
SeismoDetectionLongShortAvg::cast(const char *n)
{
  if (strcmp(n, "SeismoDetectionLongShortAvg") == 0)
    return (SeismoDetectionLongShortAvg *) this;
  else if (strcmp(n, "SeismoDetectionAlgorithm") == 0)
    return (SeismoDetectionAlgorithm *) this;
  else
    return 0;
}

int
SeismoDetectionLongShortAvg::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "LONGAVG", cpkP, cpInteger, &_long_avg_count,
      "SHORTAVG", cpkP, cpInteger, &_short_avg_count,
      "DIFFTHRESHOLD", cpkP, cpInteger, &_dthreshold,
      "RATIOTHRESHOLD", cpkP, cpInteger, &_rthreshold,
      "NORMALIZE", cpkP, cpInteger, &_normalize,
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
SeismoDetectionLongShortAvg::initialize(ErrorHandler *)
{
  for ( int i = 0; i < 3; i++ ) {
    swin = SlidingWindow(_short_avg_count, _long_avg_count);
    swin._debug = _debug;
  }

  return 0;
}

void
SeismoDetectionLongShortAvg::update(SrcInfo *si, uint32_t next_new_block)
{
  BRN_DEBUG("get_data");

  seismo_evaluation(si, next_new_block);
}

void
SeismoDetectionLongShortAvg::seismo_evaluation(SrcInfo *si, uint32_t next_new_block)
{
    SeismoInfoBlock* sib;
    uint32_t ac_block_id = next_new_block;
    uint32_t _index_in_block;

    while ( (sib = si->get_block(ac_block_id)) != NULL ) {
      if ( ! sib->is_complete() ) break;

      for ( _index_in_block = 0; _index_in_block < sib->_next_value_index; _index_in_block++ ) {
        if ( swin.add_data(sib->_channel_values[_index_in_block][0]) == 0 ) {
          /* check for alarm */
          swin.calc_stats(false);

          int32_t long_term_stdev = swin._history_stdev;
          int32_t short_term_stdev = swin._window_stdev;

          int32_t long_term_avg = swin._history_avg;
          int32_t short_term_avg = swin._window_avg;

          BRN_DEBUG("Stdev: Long: %d Short: %d",long_term_stdev, short_term_stdev);
          BRN_DEBUG("AVG: Long: %d Short: %d",long_term_avg, short_term_avg);

          int32_t short_term_avg_norm = (_normalize * short_term_avg) / long_term_avg;

          int32_t short_long_diff = _normalize - short_term_avg_norm;
          int32_t short_long_ratio = short_term_avg / long_term_avg;

          /*
          if ((short_long_diff > _dthreshold) || (short_long_ration > _rthreshold)) {
            if ( (sal.size() == 0) || (sal[sal.size() - 1]._start == ALARM_MODE_END) ) {
            } else if (sal[sal.size() - 1]->start == ALARM_MODE_START) {
            }
          } else {
            if ( (sal.size() > 0) && (sal[sal.size() - 1]._start == ALARM_MODE_START) ) {
              sal[sal.size() - 1]->end_alarm();
            }
          }
          */
        }

        /*
        click_chatter("lsa: %d %d %d", sib->_channel_values[_index_in_block][0],
                                       sib->_channel_values[_index_in_block][1],
                                       sib->_channel_values[_index_in_block][2]);
        */
      }

      ac_block_id++;
      _index_in_block = 0;
    }

    if ( sib == NULL ) {
      BRN_DEBUG("No data left in block");
    }
}
/************************************************************************************/
/********************************* H A N D L E R ************************************/
/************************************************************************************/

static String
stats_handler(Element *e, void */*thunk*/)
{
  SeismoDetectionLongShortAvg *si = reinterpret_cast<SeismoDetectionLongShortAvg*>(e);
  StringAccum sa;

  si->swin.calc_stats(true);
  sa << "<seismodetection_longshortavg inserts=\"" << si->swin._inserts << "\" >\n";
  sa << "\t<history min=\"" << si->swin._history_min << "\" max=\"" << si->swin._history_max;
  sa << "\" avg=\"" << si->swin._history_avg << "\" sq_avg=\"" << si->swin._history_sq_avg;
  sa << "\" stdev=\"" << si->swin._history_stdev;
  sa << "\" values=\"" << si->swin._history_no_values << "\" />\n";
  sa << "\t<window min=\"" << si->swin._window_min << "\" max=\"" << si->swin._window_max;
  sa << "\" avg=\"" << si->swin._window_avg << "\" sq_avg=\"" << si->swin._window_sq_avg;
  sa << "\" stdev=\"" << si->swin._window_stdev;
  sa << "\" values=\"" << si->swin._window_no_values << "\" />\n";
  sa << "\t<detection long_short_diff=\"" << (si->swin._window_avg - si->swin._history_avg);
  if ( si->swin._history_avg != 0 ) {
    sa << "\" long_short_ration=\"-1\" />\n";
  } else {
    sa << "\" long_short_ration=\"" << (si->swin._window_avg/si->swin._history_avg);
    sa << "\" />\n";
  }
  sa << "</seismodetection_longshortavg>\n";

  return sa.take_string();
}

void
SeismoDetectionLongShortAvg::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", stats_handler);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SeismoDetectionLongShortAvg)
