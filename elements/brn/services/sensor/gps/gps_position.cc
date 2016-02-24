#include <click/config.h>
#include <click/confparse.hh>

#include "gps_position.hh"

CLICK_DECLS

GPSPosition::GPSPosition()
 :_latitude(FixPointNumber()), _longitude(FixPointNumber()), _altitude(FixPointNumber()), _speed(FixPointNumber()), _x(0),_y(0),_z(0)
{
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(GPSPosition)
