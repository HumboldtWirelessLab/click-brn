#ifndef FIXPOINTNUMBER_HH
#define FIXPOINTNUMBER_HH

#include <click/config.h>
#include <click/straccum.hh>

CLICK_DECLS

/*
 * max int > 2 000 000 000
 * split to 000.000000
 * max.  ~  999.999999
*/

#define PREFACTOR 1000000 /*6 digests*/

class FixPointNumber
{
 public:

  FixPointNumber();
  explicit FixPointNumber(int n);
  explicit FixPointNumber(String n);

  ~FixPointNumber();

  int fromString(String n);

  inline operator String() const;
  String unparse() const;
  String toString() { return unparse(); }

  void fromInt(int n) { number = n * PREFACTOR; };
  int toInt() { return (number/PREFACTOR); }

  void convertFromPrefactor(int fixp, int prefactor) {
    if ( prefactor < PREFACTOR ) number = fixp * (PREFACTOR/prefactor);
    else number = fixp / (prefactor/PREFACTOR);
  };

  int getPacketInt(void) { return number; }
  void setPacketInt(int n) { number = n; }

  static int sizeup(int i, int digest);
  int32_t number;

};

inline
FixPointNumber::operator String() const
{
    return unparse();
}

inline bool
operator==(const FixPointNumber &a, const FixPointNumber &b)
{
  return (a.number == b.number);
}

inline bool
operator!=(const FixPointNumber &a, const FixPointNumber &b)
{
  return (a.number != b.number);
}

inline FixPointNumber &
operator/=(FixPointNumber &a, const FixPointNumber &b)
{
  uint64_t a_big = (uint64_t)a.number;
  uint64_t b_big = (uint64_t)(b.number);

  a_big *= PREFACTOR;
  a_big /= b_big;
  a.number = (uint32_t)a_big;

  return a;
}

inline FixPointNumber
operator/(const FixPointNumber &a, const FixPointNumber &b)
{
  uint64_t a_big = (uint64_t)a.number;
  uint64_t b_big = (uint64_t)(b.number);

  a_big *= PREFACTOR;
  a_big /= b_big;

  return FixPointNumber((uint32_t)a_big);
}

#define FPN FixPointNumber

CLICK_ENDDECLS
#endif
