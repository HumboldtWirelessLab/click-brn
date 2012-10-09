#ifndef VECTOR3D_HH
#define VECTOR3D_HH

CLICK_DECLS

class Vector3D {
 public:

#if CLICK_LINUXMODULE || CLICK_BSDMODULE
  int _x, _y, _z;
#else
  double _x, _y, _z;
#endif

  Vector3D() {
    _x = _y = _z = 0;
  }

#if CLICK_LINUXMODULE || CLICK_BSDMODULE
  Vector3D(int x, int y, int z) {
    _x = x; _y = y; _z = z;
  }
#else
  Vector3D(double x, double y, double z) {
    _x = x; _y = y; _z = z;
  }
#endif

  ~Vector3D() {}


#if CLICK_LINUXMODULE || CLICK_BSDMODULE
  void mul(int fac);
  void div(int fac);
#else
  void mul(double fac);
  void div(double fac);
#endif

  void normalize(int norm = 1);
  void add(Vector3D &vec);
  void sub(Vector3D &vec);

#if CLICK_LINUXMODULE || CLICK_BSDMODULE
  int length();
#else
  double length();
#endif

  String unparse();

  void zero() {
    _x = _y = _z = 0;
  }

#if CLICK_LINUXMODULE || CLICK_BSDMODULE
  void limit(int lim);
#else
  void limit(double lim);
#endif

  bool equals(Vector3D v) {
    return ((v._x == _x) && (v._y == _y) && (v._z == _z));
  }

};

CLICK_ENDDECLS
#endif
