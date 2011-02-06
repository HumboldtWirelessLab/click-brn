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

#ifndef SEISMO_ELEMENT_HH
#define SEISMO_ELEMENT_HH
#include <click/element.hh>
#include "elements/brn2/brnelement.hh"

#include "elements/brn2/services/sensor/gps/gps.hh"


CLICK_DECLS

struct click_seismo_data_header {
  int gps_lat;
  int gps_long;
  int gps_alt;
  int gps_hdop;

  int sampling_rate;
  int samples;
  int channels;
};

struct click_seismo_data {
  uint64_t time;
  int timing_quality;
};

class Seismo : public BRNElement {

 public:

  Seismo();
  ~Seismo();

  const char *class_name() const  { return "Seismo"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "1/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  void add_handlers();

  void push(int, Packet *p);

  GPS *_gps;
  bool _print;
};

CLICK_ENDDECLS
#endif
