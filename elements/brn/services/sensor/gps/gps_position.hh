#ifndef GPSPOSITION_HH
#define GPSPOSITION_HH

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/timestamp.hh>


#include <elements/brn2/standard/vector/vector3d.hh>
#include <elements/brn2/standard/fixpointnumber.hh>

CLICK_DECLS

struct gps_position {
  int x;
  int y;
  int z;
  int speed;
} __attribute__ ((packed));

struct gps_speed {
  int x;
  int y;
  int z;
} __attribute__ ((packed));


class GPSPosition {
 public:
  FixPointNumber _latitude;
  FixPointNumber _longitude;
  FixPointNumber _altitude;

  FixPointNumber _speed;

  int _x,_y,_z;

  Timestamp gpstime;

  GPSPosition();

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
    //click_chatter("set loc to coordinates: %d, %d, %d", x, y, z);
    _z=z; _x=x; _y=y;
  }

  void setGPS(String lat, String lon, String alt) {
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

#pragma message "Check setSpeed usage in GPS"
  void setSpeed(FixPointNumber speed)
  {
    _speed = speed;
  }

  void setSpeed(int speed)
  {
    _speed.setPacketInt(speed);
  }

  void setSpeed(String speed)
  {
    _speed.fromString(speed);
  }

  String unparse() {
	  StringAccum sa;
	  sa << "LAT: " << _latitude.unparse() << " ";
	  sa << "LONG: " << _longitude.unparse() << " ";
	  sa << "ALT: " << _altitude.unparse() << " ";
	  sa << "SPEED: " << _speed.unparse();
	  return sa.take_string().c_str();
  }

  String unparse_coord() {
	  StringAccum sa;
      sa << _x << " " << _y << " " << _z;
	  return sa.take_string().c_str();
  }

  Vector3D vector3D() {
    return Vector3D(_x,_y,_z);
  }
};

CLICK_ENDDECLS
#endif
