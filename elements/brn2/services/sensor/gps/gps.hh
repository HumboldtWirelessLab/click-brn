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
#include <click/ipaddress.hh>

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

#define GPSMODE_HANDLER 0
#define GPSMODE_GPSD 0
#define GPSMODE_SERIAL 0

struct gps_position {
  int x;
  int y;
  int z;
} __attribute__ ((packed));


class GPSPosition {
 public:
  int _longitude;
  int _latitude;
  int _h;

  int _x,_y,_z;

  Timestamp gpstime;

  GPSPosition() {
    _longitude = 0;
    _latitude = 0;
    _h = 0;
    _z=0; _x=0; _y=0;
  }

  GPSPosition(struct gps_position *pos) {
    _z=pos->z; _x=pos->x; _y=pos->y;
  }

  GPSPosition(int lon, int lat, int h) {
    _longitude = lon;
    _latitude = lat;
    _h = h;
  }

  void setCC(int x, int y, int z) {
    _z=z; _x=x; _y=y;
  }

  int isrqt(uint32_t n) {
    int x,x1;

    if ( n == 0 ) return 0;

    x1 = n;

    do {
      x = x1;
      x1 = (x + n/x) >> 1;
    } while ((( (x - x1)  > 1 ) || ( (x - x1)  < -1 )) && ( x1 != 0 ));

    return x1;
  }

  int getDistance(GPSPosition *pos) {
    return isrqt(((pos->_x - _x) * (pos->_x - _x)) + ((pos->_y - _y) * (pos->_y - _y)) + ((pos->_z - _z) * (pos->_z - _z)));
  }

  void getPosition(struct gps_position *pos)
  {
    pos->x = _x;
    pos->y = _y;
    pos->z = _z;
  }

  void setPosition(struct gps_position *pos)
  {
    _x = pos->x;
    _y = pos->y;
    _z = pos->z;
  }
};

class GPS : public Element {

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

  GPSPosition *getPosition() { return &_position;}

 private:
  //
  //member
  //
  GPSPosition _position;

 public:
  int _gpsmode;
  int _debug;

  String _gpsdevice;
  IPAddress _gpsd_ip;
  int _gpsd_port;

  int update_interval;
};

CLICK_ENDDECLS
#endif
