#ifndef FIXPOINTNUMBER_HH
#define FIXPOINTNUMBER_HH

#include <click/config.h>
#include <click/straccum.hh>

CLICK_DECLS

/*
 * max int > 2000000000
 * split to 000.000000
*/

#define PREFACTOR 1000000

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

  int getPacketInt(void) { return number; }
  void setPacketInt(int n) { number = n; }

 private:

  static int sizeup(int i, int digest);
  int number;

};

#define FPN FixPointNumber

CLICK_ENDDECLS
#endif
