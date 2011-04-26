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
#include <click/vector.hh>
#include <click/ipaddress.hh>

#include <elements/brn2/standard/fixpointnumber.hh>

#include <elements/brn2/brnelement.hh>


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
#define GPSMODE_GPSD    1
#define GPSMODE_SERIAL  2

struct gps_position {
  int x;
  int y;
  int z;
  int speed;
} __attribute__ ((packed));


class GPSPosition {
 public:
   FixPointNumber _latitude;
   FixPointNumber _longitude;
   FixPointNumber _altitude;

   FixPointNumber _speed;

  int _x,_y,_z;

  Timestamp gpstime;

  GPSPosition() {
    _latitude = FixPointNumber();
    _longitude = FixPointNumber();
    _altitude = FixPointNumber();

    _speed = FixPointNumber();

    _z=0; _x=0; _y=0;
  }

  GPSPosition(struct gps_position *pos) {
    _z=pos->z; _x=pos->x; _y=pos->y;
  }

  GPSPosition(FixPointNumber lat, FixPointNumber lon, FixPointNumber alt) {
    _latitude = lat;
    _longitude = lon;
    _altitude = alt;
  }

  void setGPS(FixPointNumber lat, FixPointNumber lon, FixPointNumber alt) {
    _latitude = lat;
    _longitude = lon;
    _altitude = alt;
  }

  void setCC(int x, int y, int z) {
    _z=z; _x=x; _y=y;
  }

  void setGPSC(String lat, String lon, String alt) {
    _latitude.fromString(lat);
    _longitude.fromString(lon);
    _altitude.fromString(alt);
  }

  /*sqrt for integer*/
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
    pos->speed = _speed.getPacketInt();
  }

  void setPosition(struct gps_position *pos)
  {
    _x = pos->x;
    _y = pos->y;
    _z = pos->z;
    _speed.setPacketInt(pos->speed);
  }

  void setSpeed(int speed)
  {
    _speed.setPacketInt(speed);
  }

  void setSpeed(String speed)
  {
    _speed.fromString(speed);
  }
};

class GPS : public BRNElement {

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
  inline String read_gps();

 private:
  //
  //member
  //
  GPSPosition _position;

 public:
  int _gpsmode;

  void set_gps(FixPointNumber lat, FixPointNumber lon, FixPointNumber alt) {
    _position.setGPS(lat,lon,alt);
  }

  String _gpsdevice;

  IPAddress _gpsd_ip;
  int _gpsd_port;

  int update_interval;
};

CLICK_ENDDECLS
#endif
