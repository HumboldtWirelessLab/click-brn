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
  _print(false)
{
  BRNElement::init();
}

Seismo::~Seismo()
{
}

int
Seismo::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "GPS", cpkM+cpkP, cpElement, &_gps,
      "PRINT", cpkP, cpBool, &_print,
      "CALCSTATS", cpkP, cpBool, &_calc_stats,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

void
Seismo::push(int, Packet *p)
{
  uint8_t *data = (uint8_t*)p->data();
  struct click_seismo_data_header *seismo_header = (struct click_seismo_data_header *)data;

  FixPointNumber fp_lat,fp_long,fp_alt;
  fp_lat.convertFromPrefactor(ntohl(seismo_header->gps_lat), 100000);
  fp_long.convertFromPrefactor(ntohl(seismo_header->gps_long), 100000);
  fp_alt.convertFromPrefactor(ntohl(seismo_header->gps_alt), 100000);

  _gps->set_gps(fp_lat,fp_long,fp_alt);

  int src_node_id = 42; // TODO: we need an identifier!!!
  if (_print) {
    click_chatter("GPS: Lat: %d Long: %d Alt: %d HDOP: %d SamplingRate: %d Samples: %d Channel: %d",
                  ntohl(seismo_header->gps_lat), ntohl(seismo_header->gps_long),
                  ntohl(seismo_header->gps_alt), ntohl(seismo_header->gps_hdop),
                  ntohl(seismo_header->sampling_rate), ntohl(seismo_header->samples), ntohl(seismo_header->channels));
  }

  // store in internal structure
  SrcInfo *src_i;
  if (_calc_stats) {
      src_i = _node_stats_tab.findp(src_node_id);
      if (!src_i) {
        _node_stats_tab.insert(src_node_id, SrcInfo(ntohl(seismo_header->gps_lat),ntohl(seismo_header->gps_long),ntohl(seismo_header->gps_alt), ntohl(seismo_header->gps_hdop), ntohl(seismo_header->sampling_rate), ntohl(seismo_header->channels)));
      }
      src_i = _node_stats_tab.findp(src_node_id);
  }
  data = (uint8_t*)&seismo_header[1];

  for ( uint32_t i = 0; i < ntohl(seismo_header->samples); i++) {
    struct click_seismo_data *seismo_data = (struct click_seismo_data *)data;
    uint32_t *data32 = (uint32_t*)&seismo_data[1];

    if (_print) {
      StringAccum sa;
      sa << "Time: " << seismo_data->time << " Time_qual: " << seismo_data->timing_quality << " Data:";
      for ( uint32_t j = 0; j < ntohl(seismo_header->channels); j++ )  sa << " " << ntohl(data32[j]);
      click_chatter("%s",sa.take_string().c_str());
    }

    // update stats counter
    if (_calc_stats) {
      for ( uint32_t j = 0; j < ntohl(seismo_header->channels); j++ )  src_i->add_channel_val(j, ntohl(data32[j]));
    }

    data = (uint8_t*)&data32[ntohl(seismo_header->channels)];
  }
  p->kill();
}

static String
read_handler(Element *e, void *thunk)
{
  Seismo *si = (Seismo*)e;

  if (si->_node_stats_tab.size() == 0) {
     // return old value
     return si->_last_channelstatinfo;
  } else {

  StringAccum sa;

  for (NodeStatsIter iter = si->_node_stats_tab.begin(); iter.live(); iter++) {

    SrcInfo src = iter.value();
    int id = iter.key();

    sa << "<node id='" << id << "'>\n";
    sa << "<gps long='" << src._gps_long << "' lat='" << src._gps_lat << "' alt='" << src._gps_alt << "' HDOP='" << src._gps_hdop << "' />\n";

    sa << "<seismo samplingrate='" << src._sampling_rate << "' sample_count='" << src._sample_count << "' channel='" << src._channels << "'>\n";

    for (uint32_t j = 0; j < 4; j++) {
	    sa << "<chaninfo id='" << j << "' avg_value='" << src.avg_channel_info(j) << "' />\n";
    }

    sa << "</seismo>\n";
    sa << "</node>\n";

  }

  // clear node stat before returning
  si->_node_stats_tab.clear();

  si->_last_channelstatinfo = sa.take_string();
  return si->_last_channelstatinfo;
  }
}

void
Seismo::add_handlers()
{
  BRNElement::add_handlers();
  add_read_handler("channelstatinfo", read_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Seismo)
