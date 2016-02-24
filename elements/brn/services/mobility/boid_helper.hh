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

#ifndef BOIDHELPERELEMENT_HH
#define BOIDHELPERELEMENT_HH

#include <elements/brn/standard/fixpointnumber.hh>
#include <elements/brn/standard/vector/vector3d.hh>

CLICK_DECLS

class Gravitation {
  public:
    Vector3D _position;
    int _mass;

    Gravitation() :
      _position(Vector3D()), _mass(0)
    {
    }

    Gravitation(int x, int y, int z, int m) :
      _position(Vector3D(x,y,z)), _mass(m)
    {
    }

    ~Gravitation() {};
};

typedef Vector<Gravitation> GravitationList;
typedef GravitationList::const_iterator GravitationListIter;

class Predator {
  public:
    Vector3D _position;
    int _power;

    Predator() :
      _position(Vector3D()), _power(0)
    {
    }

    Predator(int x, int y, int z, int p) :
      _position(Vector3D(x,y,z)), _power(p)
    {
    }

    ~Predator() {};
};

typedef Vector<Predator> PredatorList;
typedef PredatorList::const_iterator PredatorListIter;

class BoidMove {
  public:
    BoidMove() :
      _direction(Vector3D()),_speed(0),_move_type(0)
    {}

    Vector3D _direction;
    int      _speed;
    int      _move_type;
};

CLICK_ENDDECLS
#endif
