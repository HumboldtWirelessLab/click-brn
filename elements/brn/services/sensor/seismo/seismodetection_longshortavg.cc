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

#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "seismo_reporting.hh"
#include "seismodetection_longshortavg.hh"

CLICK_DECLS

SeismoDetectionLongShortAvg::SeismoDetectionLongShortAvg():
  _long_avg_count(0),
  _short_avg_count(0),
  _dthreshold(DEFAULT_DIFF_THRESHOLD),
  _rthreshold(DEFAULT_RATIO_THRESHOLD),
  _normalize(DEFAULT_NORMALIZE),
  _max_alarm_count(0),
  _print_alarm(false),
  _init_swin(false),
  _alarm_id(0),
  _alarm_dist(DEFAULT_ALARM_DIST_MS)
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
      "PRINTALARM", cpkP, cpBool, &_print_alarm,
      "ALARMTIMEDIST", cpkP, cpInteger, &_alarm_dist,
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
  return 0;
}

void
SeismoDetectionLongShortAvg::update(SrcInfo *si, uint32_t next_new_block)
{
    SeismoInfoBlock* sib;
    uint32_t ac_block_id = next_new_block;
    uint32_t _index_in_block;

    if ( ! _init_swin ) {
      for ( int i = 0; i < 1 /*si->_channels*/; i++ ) {
        swin = SlidingWindow(_short_avg_count, _long_avg_count);
        swin._debug = _debug;
        swin._samplerate = si->_sampling_rate;
        _init_swin = true;
      }
    }

    uint32_t timenow_ms = Timestamp::now().msecval();
    while ( (sib = si->get_block(ac_block_id)) != NULL ) {
      if ( ! sib->is_complete() ) break;

      for ( _index_in_block = 0; _index_in_block < sib->_next_value_index; _index_in_block++ ) {
        if ( swin.add_data(sib->_channel_values[_index_in_block][0]) == 0 ) {
          /* check for alarm */
          swin.calc_stats(false);

          int32_t long_term_abs_avg = swin._history_abs_avg;
          int32_t short_term_abs_avg = swin._window_abs_avg;

          BRN_DEBUG("AVG: Long: %d Short: %d",long_term_abs_avg, short_term_abs_avg);

	  int32_t short_term_avg_norm = _normalize;
	  int32_t short_long_ratio = 1;
	  int32_t short_long_diff = 0;

	  if ( long_term_abs_avg != 0 ) {
            short_long_ratio = short_term_avg_norm = (_normalize * short_term_abs_avg) / long_term_abs_avg;
            short_long_diff = short_term_avg_norm - _normalize;
	  }


          if ((short_long_diff > (int32_t)_dthreshold) || (short_long_ratio > (int32_t)_rthreshold)) {
            if ( _print_alarm )
	      click_chatter("Alarm: Time: %d Diff: %d Ratio %d", swin._inserts, short_long_diff, short_long_ratio);

            //if we have no alarm or if the last already end and is to far then add
            //a new one
            if ( (sal.size() == 0) ||
                 ((((SeismoAlarmLTASTAInfo*)(sal[sal.size()-1]->_detection_info))->_mode == ALARM_MODE_END) &&
                  (sal[sal.size()-1]->_start.msecval()+_alarm_dist <= timenow_ms)) ) {
              sal.push_back(new SeismoAlarm());

              SeismoAlarmLTASTAInfo *sa_info = new SeismoAlarmLTASTAInfo(
                         swin._history_stdev,  swin._window_stdev,
                         long_term_abs_avg, short_term_abs_avg,
                         swin._history_sq_avg, swin._window_sq_avg,
                         swin._inserts);

              sa_info->_sampletime = sib->_time[_index_in_block]/1000000; //sampletime is ns but we want sec
              sa_info->_mode = ALARM_MODE_START;
              sal[sal.size()-1]->_detection_info = (void*)sa_info;
              sal[sal.size()-1]->_id = _alarm_id++;

              if ( (uint32_t)sal.size() > _max_alarm_count ) {
                SeismoAlarmLTASTAInfo *sa_info;
                sa_info = (SeismoAlarmLTASTAInfo *)sal[0]->_detection_info;
                delete sa_info;
                delete sal[0];
                sal.erase(sal.begin());
              }
            }/* else if (sal[sal.size() - 1]->start == ALARM_MODE_START) {
              //update ?
            }*/
          } else {
            if ( (sal.size() != 0) &&
                 (((SeismoAlarmLTASTAInfo*)(sal[sal.size()-1]->_detection_info))->_mode == ALARM_MODE_START) ) {
             SeismoAlarmLTASTAInfo *sa_info;
             sa_info = (SeismoAlarmLTASTAInfo *)sal[sal.size()-1]->_detection_info;
             sa_info->end_alarm();
             sal[sal.size()-1]->end_alarm();
	   }
          }
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

String
SeismoDetectionLongShortAvg::stats()
{
  StringAccum sa;
  swin.calc_stats(true);
  sa << "<seismodetection_stalta id=\"" << BRN_NODE_NAME << "\" inserts=\"";
  sa << swin._inserts << "\" >\n";
  sa << "\t<history min=\"" << swin._history_min << "\" max=\"" << swin._history_max;
  sa << "\" avg=\"" << swin._history_avg << "\" abs_avg=\"" << swin._history_abs_avg;
  sa << "\" sq_avg=\"" << swin._history_sq_avg << "\" stdev=\"" << swin._history_stdev;
  sa << "\" values=\"" << swin._history_no_values << "\" />\n";
  sa << "\t<window min=\"" << swin._window_min << "\" max=\"" << swin._window_max;
  sa << "\" avg=\"" << swin._window_avg << "\" abs_avg=\"" << swin._window_abs_avg;
  sa << "\" sq_avg=\"" << swin._window_sq_avg << "\" stdev=\"" << swin._window_stdev;
  sa << "\" values=\"" << swin._window_no_values << "\" />\n";

  int32_t short_term_avg_norm = _normalize;
  int32_t short_long_ratio = 1; 
  int32_t short_long_diff = 0;

  if ( swin._history_abs_avg != 0 ) {
    short_long_ratio = short_term_avg_norm = (_normalize*swin._window_abs_avg)/ swin._history_abs_avg;
    short_long_diff = short_term_avg_norm - _normalize;
  }

  sa << "\t<detection long_short_diff=\"" << short_long_diff << "\" long_short_ration=\"";
  sa << short_long_ratio << "\" />\n";

  sa << "\t<alarmlist count=\"" << sal.size() << "\" max=\"" << _max_alarm_count << "\" >\n";

  for ( int i = 0; i < sal.size(); i++) {
    SeismoAlarm *salarm = sal[i];
    SeismoAlarmLTASTAInfo *sainfo = (SeismoAlarmLTASTAInfo*)salarm->_detection_info;

    short_long_ratio=(sainfo->_avg_long==0)?0:(short_term_avg_norm * sainfo->_avg_short)/sainfo->_avg_long;

    sa << "\t\t<alarm start=\"" << salarm->_start.unparse() << "\" end=\"" << salarm->_end.unparse();
    sa << "\" avg_long=\"" << sainfo->_avg_long << "\" av_short=\"" << sainfo->_avg_short;
    sa << "\" ratio=\"" << short_long_ratio << "\" insert=\"" << sainfo->_insert;
    sa << "\" sampletime=\"" << sainfo->_sampletime << "\" />\n";
  }

  sa << "\t</alarmlist>\n</seismodetection_stalta>\n";

  return sa.take_string();
}


static String
stats_handler(Element *e, void */*thunk*/)
{
  SeismoDetectionLongShortAvg *si = reinterpret_cast<SeismoDetectionLongShortAvg*>(e);
  return si->stats();
}

void
SeismoDetectionLongShortAvg::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", stats_handler);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SeismoDetectionLongShortAvg)
