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

  if ( port == 0 ) {  //local data
    FixPointNumber fp_lat,fp_long,fp_alt;
    fp_lat.convertFromPrefactor(ntohl(seismo_header->gps_lat), 100000);
    fp_long.convertFromPrefactor(ntohl(seismo_header->gps_long), 100000);
    fp_alt.convertFromPrefactor(ntohl(seismo_header->gps_alt), 100000);

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
      src_i = _node_stats_tab.findp(src_node_id);
      if (!src_i) {
        _node_stats_tab.insert(src_node_id, SrcInfo(ntohl(seismo_header->gps_lat), ntohl(seismo_header->gps_long),
                                                    ntohl(seismo_header->gps_alt), ntohl(seismo_header->gps_hdop),
                                                    ntohl(seismo_header->sampling_rate), ntohl(seismo_header->channels)));
        src_i = _node_stats_tab.findp(src_node_id);
        if ( port == 0 ) {
          _local_info = src_i;
        }
      } else {
        src_i->update_gps(ntohl(seismo_header->gps_lat), ntohl(seismo_header->gps_long),
                          ntohl(seismo_header->gps_alt), ntohl(seismo_header->gps_hdop));
      }
  }

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
    if ( _record_data && (src_i->_latest_seismo_infos.size() < 1000) ) {
      //click_chatter("!");
      SeismoInfo seismo_info;
      seismo_info._time = seismo_data->time;
      seismo_info._channels = ntohl(seismo_header->channels);
      for ( uint32_t j = 0; j < ntohl(seismo_header->channels); j++ ) seismo_info._channel_values[j] = (int32_t)ntohl(data32[j]);
      src_i->_latest_seismo_infos.push_back(seismo_info);
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
    SrcInfo *src = si->_node_stats_tab.findp(id);

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
  }
  sa << "</seismo>\n";

  si->_node_stats_tab.clear();  // clear node stat before returning
  si->_local_info = NULL;           // clear link to local info

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
    SrcInfo *src = si->_node_stats_tab.findp(id);

    sa << "\t<node id='" << id.unparse() << "'" << " time='" << now.unparse() << "'>\n\t\t<channel_infos size='" << src->_latest_seismo_infos.size() << "' >\n";
    if ( src->_latest_seismo_infos.size() != 0 ) {

      LatestSeismoInfosIter lsi_iter;

      for (lsi_iter = src->_latest_seismo_infos.begin(); lsi_iter != src->_latest_seismo_infos.end(); lsi_iter++) {

        sa << "\t\t\t<channel_info time='" << lsi_iter->_time << "'";
        int channels = lsi_iter->_channels - 1;

        for (int32_t j = 0; j < channels; j++) {
          sa << " channel_" << j << "='" << lsi_iter->_channel_values[j] << "'";
        }

        sa << " />\n";
      }
    }

    sa << "\t\t</channel_infos>\n\t</node>\n";

    src->_latest_seismo_infos.clear();
  }

  sa << "</seismo_channel_infos>\n";

  return sa.take_string();
}

static String _seismo_stats_tags[2][4] = { { "channel_infos>", "\t<channel_info time='", " channel_", "\n" },
                                           { "c>", "<v t='", " c", "" } };

static String
local_latest_handler(Element *e, void */*thunk*/)
{
  Seismo *si = (Seismo*)e;
  StringAccum sa;
  LatestSeismoInfosIter iter;

  sa << "<" << _seismo_stats_tags[si->_tag_len][0] << _seismo_stats_tags[si->_tag_len][3];
  if ( si->_local_info != NULL ) {
    if ( si->_local_info->_latest_seismo_infos.size() != 0 ) {
      for (iter = si->_local_info->_latest_seismo_infos.begin(); iter != si->_local_info->_latest_seismo_infos.end(); iter++) {

        sa << _seismo_stats_tags[si->_tag_len][1] << iter->_time << "'";
        int channels = iter->_channels - 1;

        for (int32_t j = 0; j < channels; j++) {
          sa << _seismo_stats_tags[si->_tag_len][2] << j << "='" << iter->_channel_values[j] << "'";
        }

        sa << "/>" << _seismo_stats_tags[si->_tag_len][3];
      }
    }
  }
  sa << "</" << _seismo_stats_tags[si->_tag_len][0] << "\n";

  if ( si->_local_info != NULL ) {
    si->_local_info->_latest_seismo_infos.clear();
  }

  return sa.take_string();
}

void
Seismo::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("channelstatinfo", read_handler, 0);

  add_read_handler("latestchannelinfos", latest_handler, 0);
  add_read_handler("local_latestchannelinfos", local_latest_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Seismo)
