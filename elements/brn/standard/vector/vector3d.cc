#include <click/config.h>

#if CLICK_LINUXMODULE || CLICK_BSDMODULE
#include "elements/brn2/standard/fixpointnumber.hh"
#else //Userspace
#include "math.h"
#endif

#include "elements/brn2/brn2.h"
#include "vector3d.hh"

CLICK_DECLS

void
#if CLICK_LINUXMODULE || CLICK_BSDMODULE
Vector3D::mul(int fac)
#else //Userspace
Vector3D::mul(double fac)
#endif
{
  _x *= fac;
  _y *= fac;
  _z *= fac;
}

void
#if CLICK_LINUXMODULE || CLICK_BSDMODULE
Vector3D::div(int fac)
#else //Userspace
Vector3D::div(double fac)
#endif
{
  _x /= fac;
  _y /= fac;
  _z /= fac;
}

#if CLICK_LINUXMODULE || CLICK_BSDMODULE
int
#else
double
#endif
Vector3D::length()
{
#if CLICK_LINUXMODULE || CLICK_BSDMODULE
  return isqrt32(_x*_x + _y*_y + _z*_z);
#else
  return sqrt(_x*_x + _y*_y + _z*_z);
#endif
}

void
Vector3D::normalize(int norm)
{
#if CLICK_LINUXMODULE || CLICK_BSDMODULE
  int len = length();

  if ( len > norm ) {
    int fac = len / norm;

    vec[0] *= fac;
    vec[1] *= fac;
    vec[2] *= fac;
  } else {
    int fac = norm / len;

    vec[0] /= fac;
    vec[1] /= fac;
    vec[2] /= fac;
  }
#else
  double len = length()/(double)norm;
  if ( len != 0.0 ) {
    _x /= len;
    _y /= len;
    _z /= len;
  }

#endif
}

void
Vector3D::add(Vector3D &vec)
{
  _x += vec._x;
  _y += vec._y;
  _z += vec._z;
}

void
Vector3D::sub(Vector3D &vec)
{
  _x -= vec._x;
  _y -= vec._y;
  _z -= vec._z;
}

void
#if CLICK_LINUXMODULE || CLICK_BSDMODULE
Vector3D::limit(int lim)
#else
Vector3D::limit(double lim)
#endif
{
  if ( length() <= lim ) return;

#if CLICK_LINUXMODULE || CLICK_BSDMODULE
  int len = length();

  if ( len > lim ) {
    int fac = len / lim;

    vec[0] *= fac;
    vec[1] *= fac;
    vec[2] *= fac;
  } else {
    int fac = lim / len;

    vec[0] /= fac;
    vec[1] /= fac;
    vec[2] /= fac;
  }
#else
  double len = length() / lim;

  if ( len != 0 ) {
    _x /= len;
    _y /= len;
    _z /= len;
  }

#endif

}

String
Vector3D::unparse()
{
  return "(" + String(_x) + "," +  String(_y) + "," +  String(_z) + ")";
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(ns|userlevel)
ELEMENT_PROVIDES(Vector3D)
ELEMENT_LIBS(-lm)
