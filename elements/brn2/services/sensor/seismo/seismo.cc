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
 * seismo.{cc,hh}
 */

#include <click/config.h>
#include <click/confparse.hh>

#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "seismo.hh"

CLICK_DECLS

Seismo::Seismo():
  _gps(NULL),
  _print(false),
  _calc_stats(false),
  _record_data(true),
  _local_info(NULL),
  _tag_len(SEISMO_TAG_LENGTH_LONG)
{
  BRNElement::init();
}

Seismo::~Seismo()
{
  for (NodeStatsIter iter = _node_stats_tab.begin(); iter.live(); iter++) {
    EtherAddress id = iter.key();
    SrcInfo *src = _node_stats_tab.find(id);
    delete src;
  };

  _node_stats_tab.clear();
}

int
Seismo::configure(Vector<String> &conf, ErrorHandler* errh)
{
  bool short_tag_len = false;

  if (cp_va_kparse(conf, this, errh,
      "GPS", cpkM+cpkP, cpElement, &_gps,
      "CALCSTATS", cpkP, cpBool, &_calc_stats,
      "PRINT", cpkP, cpBool, &_print,
      "RECORD", cpkP, cpBool, &_record_data,
      "SHORTTAGS", cpkP, cpBool, &short_tag_len,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  if ( short_tag_len ) _tag_len = SEISMO_TAG_LENGTH_SHORT;

  return 0;
}

void
Seismo::push(int port, Packet *p)
{
  struct click_seismo_data_header *seismo_header = (struct click_seismo_data_header *)p->data();
  uint8_t *data = (uint8_t*)&seismo_header[1];
  EtherAddress src_node_id;

  if ( seismo_header->version != CLICK_SEISMO_VERSION ) {
    click_chatter("Unsupported seismo version");
    p->kill();
    return;
  }

  if ( port == 0 ) {  //local data
    FixPointNumber fp_lat,fp_long,fp_alt;
    fp_lat.convertFromPrefactor(ntohl(seismo_header->gps_lat), 100000);
    fp_long.convertFromPrefactor(ntohl(seismo_header->gps_long), 100000);
    fp_alt.convertFromPrefactor(ntohl(seismo_header->gps_alt), 100000);

    //TODO: check precision
    _gps->set_gps(fp_lat,fp_long,fp_alt);

    src_node_id = EtherAddress();

  } else {           //remote port
    click_ether *annotated_ether = (click_ether *)p->ether_header();
    src_node_id = EtherAddress(annotated_ether->ether_shost);
  }

  if (_print) {
    click_chatter("Node: %s GPS: Lat: %d Long: %d Alt: %d HDOP: %d SamplingRate: %d Samples: %d Channel: %d",
                  src_node_id.unparse().c_str(),
                  ntohl(seismo_header->gps_lat), ntohl(seismo_header->gps_long),
                  ntohl(seismo_header->gps_alt), ntohl(seismo_header->gps_hdop),
                  ntohl(seismo_header->sampling_rate), ntohl(seismo_header->samples), ntohl(seismo_header->channels));
  }

  // store in internal structure
  SrcInfo *src_i;
  if ( _calc_stats || _record_data ) {
      src_i = _node_stats_tab.find(src_node_id);
      if (!src_i) {
        _node_stats_tab.insert(src_node_id, new SrcInfo(ntohl(seismo_header->gps_lat), ntohl(seismo_header->gps_long),
                                                    ntohl(seismo_header->gps_alt), ntohl(seismo_header->gps_hdop),
                                                    ntohl(seismo_header->sampling_rate), ntohl(seismo_header->channels)));
        src_i = _node_stats_tab.find(src_node_id);
        if ( port == 0 ) {
          _local_info = src_i;
        }
      } else {
        src_i->update_gps(ntohl(seismo_header->gps_lat), ntohl(seismo_header->gps_long),
                          ntohl(seismo_header->gps_alt), ntohl(seismo_header->gps_hdop));
      }
  }

  SeismoInfoBlock* sib = src_i->get_last_block();

  /* update systime */
  //get time in usec
  uint64_t systemtime = (uint64_t)(seismo_header->time.tv_sec) * 1000000 + (uint64_t)seismo_header->time.tv_usec;

  if ( sib != NULL ) {  //we already have data, so update the time
    uint64_t sample_interval = 0;
    uint16_t missing_times = sib->missing_time_updates();

    SeismoInfoBlock* pre_sib = src_i->get_next_to_last_block();
    if ( pre_sib != NULL ) missing_times += pre_sib->missing_time_updates();

    if ( missing_times == 0 ) {
      BRN_WARN("No missing systimes");
      //assert(missing_times != 0);
    } else {
      sample_interval = (systemtime - _last_systemtime) / missing_times;
    }

    if ( pre_sib != NULL ) {
      while ( ! pre_sib->systime_complete() ) {
        pre_sib->update_systemtime(_last_systemtime);
        _last_systemtime += sample_interval;
      }
    }

    while ( ! sib->systime_complete() ) {
      sib->update_systemtime(_last_systemtime);
      _last_systemtime += sample_interval;
    }
  }

  _last_systemtime = systemtime;

  for ( uint32_t i = 0; i < ntohl(seismo_header->samples); i++) {
    struct click_seismo_data *seismo_data = (struct click_seismo_data *)data;
    int32_t *data32 = (int32_t*)&seismo_data[1];

    if (_print) {
      StringAccum sa;
      sa << "ID: " << src_node_id.unparse() << " Time: " << seismo_data->time;
      sa << " Time_qual: " << seismo_data->timing_quality << " Data:";
      for ( uint32_t j = 0; j < ntohl(seismo_header->channels); j++ )  sa << " " << (int32_t)ntohl(data32[j]);
      click_chatter("%s",sa.take_string().c_str());
    }

    // update stats counter
    if (_calc_stats) {
      src_i->update_time(seismo_data->time);
      src_i->inc_sample_count();

      for ( uint32_t j = 0; j < ntohl(seismo_header->channels); j++ )  src_i->add_channel_val(j, (int)ntohl(data32[j]));
    }

    //click_chatter("Size: %d",src_i->_latest_seismo_infos.size() );
    if ( _record_data ) {
      //click_chatter("!");
      if ( (sib == NULL) || (sib->is_complete()) ) sib = src_i->new_block();
      sib->insert(seismo_data->time, _last_systemtime, ntohl(seismo_header->channels), data32);
    }

    data = (uint8_t*)&data32[ntohl(seismo_header->channels)];
  }

  p->kill();
}

/************************************************************************************/
/********************************* H A N D L E R ************************************/
/************************************************************************************/

static String
read_handler(Element *e, void */*thunk*/)
{
  Seismo *si = (Seismo*)e;
  Timestamp now = Timestamp::now();

  if (si->_node_stats_tab.size() == 0) {
     // return old value
     return si->_last_channelstatinfo;
  } else {

  StringAccum sa;

  sa << "<seismo>\n";
  for (NodeStatsIter iter = si->_node_stats_tab.begin(); iter.live(); iter++) {

    EtherAddress id = iter.key();
    SrcInfo *src = si->_node_stats_tab.find(id);

    if (  src->_sample_count != 0 ) {
      sa << "\t<node id='" << id.unparse() << "'" << " time='" << now.unparse() << "'>\n";
      sa << "\t\t<gps lat='" << src->_gps_lat << "' long='" << src->_gps_long << "' alt='" << src->_gps_alt << "' HDOP='";
      sa << src->_gps_hdop << "' />\n";
      sa << "\t\t<sensor samplingrate='" << src->_sampling_rate << "' sample_count='" << src->_sample_count << "' channels='";
      sa << src->_channels << "' last_update='" << src->_last_update_time << "'>\n";

      for (int32_t j = 0; j < src->_channels; j++) {
        sa << "\t\t\t<chaninfo id='" << j << "' avg_value='" << (int)src->avg_channel_info(j);
        sa << "' std_value='" << (int)src->std_channel_info(j) << "' min_value='" << (int)src->min_channel_info(j);
        sa << "' max_value='" << (int)src->max_channel_info(j) << "'/>\n";
      }

      sa << "\t\t</sensor>\n";
      sa << "\t</node>\n";

      src->reset();
    }
  }
  sa << "</seismo>\n";

  si->_last_channelstatinfo = sa.take_string();
  return si->_last_channelstatinfo;
  }
}

static String
latest_handler(Element *e, void */*thunk*/)
{
  Seismo *si = (Seismo*)e;
  StringAccum sa;
  Timestamp now = Timestamp::now();

  sa << "<seismo_channel_infos>\n";
  for (NodeStatsIter iter = si->_node_stats_tab.begin(); iter.live(); iter++) {

    EtherAddress id = iter.key();
    SrcInfo *src = si->_node_stats_tab.find(id);

    int no_blocks = src->get_last_block()->_block_index - src->get_next_block(src->_next_seismo_info_block_for_handler)->_block_index;
    if ( (! src->get_last_block()->is_complete()) || (! src->get_last_block()->systime_complete()) ) no_blocks--;

    sa << "\t<node id='" << id.unparse() << "'" << " time='" << now.unparse() << "'>\n\t\t<channel_infos size='";
    sa << (no_blocks * CHANNEL_INFO_BLOCK_SIZE) << "' >\n";

    SeismoInfoBlock* sib;

    while ( (sib = src->get_next_block(src->_next_seismo_info_block_for_handler)) != NULL ) {
      if ( sib->is_complete() && sib->systime_complete()) {

        src->_next_seismo_info_block_for_handler = sib->_block_index + 1;;

        for (int i = 0; i < CHANNEL_INFO_BLOCK_SIZE; i++ ) {

          sa << "\t\t\t<channel_info time='" << sib->_time[i] << "' systime='" << sib->_systime[i] << "'";
          int channels = sib->_channels[i] - 1;

          for (int32_t j = 0; j < channels; j++) {
            sa << " channel_" << j << "='" << sib->_channel_values[i][j] << "'";
          }

          sa << " />\n";
        }
      } else {
        break;
      }
    }

    sa << "\t\t</channel_infos>\n\t</node>\n";
  }

  sa << "</seismo_channel_infos>\n";

  return sa.take_string();
}

static String _seismo_stats_tags[2][5] = { { "channel_infos>", "\t<channel_info time='", " systime='", " channel_", "\n" },
                                           { "c>", "<v t='", " c", " s='", "" } };

static String
local_latest_handler(Element *e, void */*thunk*/)
{
  Seismo *si = (Seismo*)e;
  StringAccum sa;

  sa << "<" << _seismo_stats_tags[si->_tag_len][0] << _seismo_stats_tags[si->_tag_len][4];
  if ( si->_local_info != NULL ) {
    SeismoInfoBlock* sib;

    while ( (sib = si->_local_info->get_next_block(si->_local_info->_next_seismo_info_block_for_handler)) != NULL ) {
      if ( sib->is_complete() && sib->systime_complete()) {
        si->_local_info->_next_seismo_info_block_for_handler = sib->_block_index + 1;

        for (int i = 0; i < CHANNEL_INFO_BLOCK_SIZE; i++ ) {

          sa << _seismo_stats_tags[si->_tag_len][1] << sib->_time[i] << "'";
          sa << _seismo_stats_tags[si->_tag_len][2] << sib->_systime[i] << "'";
          int channels = sib->_channels[i] - 1;

          for (int32_t j = 0; j < channels; j++) {
            sa << _seismo_stats_tags[si->_tag_len][3] << j << "='" << sib->_channel_values[i][j] << "'";
          }

          sa << "/>" << _seismo_stats_tags[si->_tag_len][4];
        }
      } else {
        break;
      }
    }
  }

  sa << "</" << _seismo_stats_tags[si->_tag_len][0] << "\n";

  return sa.take_string();
}

static String
read_tag_param(Element *e, void */*thunk*/)
{
  Seismo *si = (Seismo*)e;
  StringAccum sa;

  if (si->_tag_len == SEISMO_TAG_LENGTH_LONG) {
    sa << "false";
  } else {
    sa << "true";
  }

  return sa.take_string();
}

static int
write_tag_param(const String &in_s, Element *e, void */*vparam*/, ErrorHandler *errh)
{
  Seismo *si = (Seismo*)e;
  String s = cp_uncomment(in_s);

  bool shorttag;
  if (!cp_bool(s, &shorttag))
    return errh->error("short tag is bool");

  if ( shorttag && (si->_tag_len == SEISMO_TAG_LENGTH_LONG)) {
    si->_tag_len = SEISMO_TAG_LENGTH_SHORT;
  } else {
    if ( (!shorttag) && (si->_tag_len == SEISMO_TAG_LENGTH_SHORT)) {
      si->_tag_len = SEISMO_TAG_LENGTH_LONG;
    }
  }

  return 0;
}

void
Seismo::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("channelinfostats", read_handler);

  add_read_handler("channelinfo", latest_handler);
  add_read_handler("localchannelinfo", local_latest_handler);

  add_read_handler("shorttag", read_tag_param);
  add_write_handler("shorttag", write_tag_param);

}

CLICK_ENDDECLS
EXPORT_ELEMENT(Seismo)
