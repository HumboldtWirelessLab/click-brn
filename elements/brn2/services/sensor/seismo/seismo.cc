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

  if (_print) {
    click_chatter("GPS: Lat: %d Long: %d Alt: %d HDOP: %d SamplingRate: %d Samples: %d Channel: %d",
                  ntohl(seismo_header->gps_lat), ntohl(seismo_header->gps_long),
                  ntohl(seismo_header->gps_alt), ntohl(seismo_header->gps_hdop),
                  ntohl(seismo_header->sampling_rate), ntohl(seismo_header->samples), ntohl(seismo_header->channels));
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
    //click_chatter("%d",(ntohl(data32[0]) & 0x0007ffff) - 0x00040000);

    data = (uint8_t*)&data32[ntohl(seismo_header->channels)];
  }

  p->kill();
}

void
Seismo::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Seismo)
