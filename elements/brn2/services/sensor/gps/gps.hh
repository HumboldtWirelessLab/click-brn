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

#ifndef GPSSENSORELEMENT_HH
#define GPSSENSORELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

CLICK_DECLS
/*
 * =c
 * GPS()
 * =s
 * Input 0  : Packets from gpsdaemon
 * Output 0 : Packets to gpsdaemon
 * =d
 * gps
 */

class GPS : public Element {
 public:
  class GPSPosition {
    int _longitude;
    int _latitude;
    int _h;

    GPSPosition(int lon, int lat, int h) {
      _longitude = lon;
      _latitude = lat;
      _h = h;
    }
  };

 public:

  //
  //methods
  //
  GPS();
  ~GPS();

  const char *class_name() const  { return "GPS"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0-1/0-1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

 private:
  //
  //member
  //

 public:
  int _debug;

  String _gpsdevice;
  String _gpsd_ip;
  String _gpsd_port;

  int update_interval;

};

CLICK_ENDDECLS
#endif
