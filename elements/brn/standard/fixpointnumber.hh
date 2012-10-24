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
  FixPointNumber(int n);
  FixPointNumber(String n);

  ~FixPointNumber();

  int fromString(String n);
  String unparse();
  String toString() { return unparse(); }

  void fromInt(int n) { number = n * PREFACTOR; };
  int toInt() { return (number/PREFACTOR); }

  void convertFromPrefactor(int fixp, int prefactor) {
    if ( prefactor < PREFACTOR ) number = fixp * (PREFACTOR/prefactor);
    else number = fixp / (prefactor/PREFACTOR);
  };

  int getPacketInt(void) { return number; }
  void setPacketInt(int n) { number = n; }

 private:

  static int sizeup(int i, int digest);
  int32_t number;

};

#define FPN FixPointNumber

CLICK_ENDDECLS
#endif
